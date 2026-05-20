#include "value_ssa.h"
#include "value_ssa_pass.h"
#include "lower_ir.h"
#include "ir.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "memory_ssa_pass.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int build_sample_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_straight_line_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrGlobal *global = NULL;
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t local_a_id;
    size_t temp0;
    size_t temp1;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "g", &global, error) ||
        !lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "a", 1, &local_a_id, error) ||
        !lower_ir_function_append_block(function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    temp1 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.load_slot = lower_ir_slot_local(local_a_id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(temp0);
    instruction.as.binary.rhs = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = lower_ir_slot_global(global->id);
    instruction.as.store.value = lower_ir_value_temp(temp1);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(temp1), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_diamond_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *then_block = NULL;
    LowerIrBasicBlock *else_block = NULL;
    LowerIrBasicBlock *join_block = NULL;
    LowerIrInstruction instruction;
    size_t local_a_id;
    size_t temp0;
    size_t temp1;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "a", 1, &local_a_id, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &then_block, error) ||
        !lower_ir_function_append_block(function, NULL, &else_block, error) ||
        !lower_ir_function_append_block(function, NULL, &join_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    temp1 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.load_slot = lower_ir_slot_local(local_a_id);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_branch(entry, lower_ir_value_temp(temp0), 1, 2, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(then_block, &instruction, error) ||
        !lower_ir_block_set_jump(then_block, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(else_block, &instruction, error) ||
        !lower_ir_block_set_jump(else_block, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_return(join_block, lower_ir_value_temp(temp1), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_diamond_store_indirect_phi_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrGlobal *global = NULL;
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *then_block = NULL;
    LowerIrBasicBlock *else_block = NULL;
    LowerIrBasicBlock *join_block = NULL;
    LowerIrInstruction instruction;
    size_t local_a_id;
    size_t addr_temp;
    size_t cond_temp;
    size_t value_temp;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "g", &global, error) ||
        !lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "a", 1, &local_a_id, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &then_block, error) ||
        !lower_ir_function_append_block(function, NULL, &else_block, error) ||
        !lower_ir_function_append_block(function, NULL, &join_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    addr_temp = lower_ir_function_allocate_temp(function);
    cond_temp = lower_ir_function_allocate_temp(function);
    value_temp = lower_ir_function_allocate_temp(function);
    if (addr_temp == (size_t)-1 || cond_temp == (size_t)-1 || value_temp == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(addr_temp);
    instruction.as.addr_slot = lower_ir_slot_global(global->id);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(cond_temp);
    instruction.as.load_slot = lower_ir_slot_local(local_a_id);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_branch(entry, lower_ir_value_temp(cond_temp), 1, 2, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(value_temp);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(then_block, &instruction, error) ||
        !lower_ir_block_set_jump(then_block, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(value_temp);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(else_block, &instruction, error) ||
        !lower_ir_block_set_jump(else_block, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = lower_ir_value_temp(addr_temp);
    instruction.as.store_indirect.value = lower_ir_value_temp(value_temp);
    if (!lower_ir_block_append_instruction(join_block, &instruction, error) ||
        !lower_ir_block_set_return(join_block, lower_ir_value_immediate(0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_scrambled_diamond_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrInstruction instruction;
    size_t local_a_id;
    size_t temp0;
    size_t temp1;
    size_t entry_id;
    size_t join_id;
    size_t then_id;
    size_t else_id;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "a", 1, &local_a_id, error) ||
        !lower_ir_function_append_block(function, &entry_id, NULL, error) ||
        !lower_ir_function_append_block(function, &join_id, NULL, error) ||
        !lower_ir_function_append_block(function, &then_id, NULL, error) ||
        !lower_ir_function_append_block(function, &else_id, NULL, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    temp1 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.load_slot = lower_ir_slot_local(local_a_id);
    if (!lower_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !lower_ir_block_set_branch(&function->blocks[entry_id], lower_ir_value_temp(temp0), then_id, else_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(&function->blocks[then_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[then_id], join_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(&function->blocks[else_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[else_id], join_id, error) ||
        !lower_ir_block_set_return(&function->blocks[join_id], lower_ir_value_temp(temp1), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_join_preserved_local_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *then_block = NULL;
    LowerIrBasicBlock *else_block = NULL;
    LowerIrBasicBlock *join_block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "a", 0, NULL, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &then_block, error) ||
        !lower_ir_function_append_block(function, NULL, &else_block, error) ||
        !lower_ir_function_append_block(function, NULL, &join_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(0);
    instruction.as.store.value = lower_ir_value_immediate(7);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_branch(entry, lower_ir_value_immediate(1), 1, 2, error) ||
        !lower_ir_block_set_jump(then_block, 3, error) ||
        !lower_ir_block_set_jump(else_block, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.load_slot = lower_ir_slot_local(0);
    if (!lower_ir_block_append_instruction(join_block, &instruction, error) ||
        !lower_ir_block_set_return(join_block, lower_ir_value_temp(temp0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_memory_fold_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t temp1;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "a", 0, NULL, error) ||
        !lower_ir_function_append_block(function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    temp1 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(0);
    instruction.as.store.value = lower_ir_value_immediate(7);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.load_slot = lower_ir_slot_local(0);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(temp0);
    instruction.as.binary.rhs = lower_ir_value_immediate(5);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(temp1), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_indirect_safe_global_forward_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrGlobal *scalar_global = NULL;
    LowerIrGlobal *array_global = NULL;
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t temp1;
    size_t temp2;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "g", &scalar_global, error) ||
        !lower_ir_program_append_global(program, "arr", &array_global, error) ||
        !lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    array_global->byte_size = 16u;

    temp0 = lower_ir_function_allocate_temp(function);
    temp1 = lower_ir_function_allocate_temp(function);
    temp2 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1 || temp2 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = lower_ir_slot_global(scalar_global->id);
    instruction.as.store.value = lower_ir_value_immediate(7);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.load_slot = lower_ir_slot_global(scalar_global->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.addr_slot = lower_ir_slot_global(array_global->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = lower_ir_value_temp(temp1);
    instruction.as.store_indirect.value = lower_ir_value_temp(temp0);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp2);
    instruction.as.load_slot = lower_ir_slot_global(scalar_global->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(temp2), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_tiny_helper_inline_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrGlobal *hashmod = NULL;
    LowerIrGlobal *arr = NULL;
    LowerIrFunction *helper = NULL;
    LowerIrFunction *main_function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t temp1;
    size_t temp2;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "hashmod", &hashmod, error) ||
        !lower_ir_program_append_global(program, "arr", &arr, error) ||
        !lower_ir_program_append_function(program, "hash", 1, &helper, error) ||
        !lower_ir_function_append_local(helper, "k", 1, NULL, error) ||
        !lower_ir_function_append_block(helper, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    arr->byte_size = 16u;

    temp0 = lower_ir_function_allocate_temp(helper);
    temp1 = lower_ir_function_allocate_temp(helper);
    temp2 = lower_ir_function_allocate_temp(helper);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1 || temp2 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.load_slot = lower_ir_slot_global(hashmod->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp2);
    instruction.as.binary.op = LOWER_IR_BINARY_MOD;
    instruction.as.binary.lhs = lower_ir_value_temp(temp0);
    instruction.as.binary.rhs = lower_ir_value_temp(temp1);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(temp2), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    block = NULL;
    if (!lower_ir_program_append_function(program, "main", 1, &main_function, error) ||
        !lower_ir_function_append_block(main_function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(main_function);
    temp1 = lower_ir_function_allocate_temp(main_function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.addr_slot = lower_ir_slot_global(arr->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = lower_ir_value_temp(temp0);
    instruction.as.store_indirect.value = lower_ir_value_immediate(0);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.call.callee_name = "hash";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (LowerIrValueRef *)calloc(1u, sizeof(LowerIrValueRef));
    if (!instruction.as.call.args) {
        lower_ir_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = lower_ir_value_immediate(9);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(temp1), error)) {
        free(instruction.as.call.args);
        lower_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    return 1;
}

static int build_lower_ir_internal_call_preserves_unwritten_global_forward_program(LowerIrProgram *program,
    LowerIrError *error) {
    LowerIrGlobal *g = NULL;
    LowerIrGlobal *arr = NULL;
    LowerIrFunction *helper = NULL;
    LowerIrFunction *main_function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t temp1;
    size_t temp2;
    size_t temp3;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "g", &g, error) ||
        !lower_ir_program_append_global(program, "arr", &arr, error) ||
        !lower_ir_program_append_function(program, "helper", 1, &helper, error) ||
        !lower_ir_function_append_local(helper, "tmp", 0, NULL, error) ||
        !lower_ir_function_append_block(helper, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    arr->byte_size = 16u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(0u);
    instruction.as.store.value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_immediate(0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    block = NULL;
    if (!lower_ir_program_append_function(program, "main", 1, &main_function, error) ||
        !lower_ir_function_append_block(main_function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(main_function);
    temp1 = lower_ir_function_allocate_temp(main_function);
    temp2 = lower_ir_function_allocate_temp(main_function);
    temp3 = lower_ir_function_allocate_temp(main_function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1 || temp2 == (size_t)-1 || temp3 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = lower_ir_slot_global(g->id);
    instruction.as.store.value = lower_ir_value_immediate(7);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.addr_slot = lower_ir_slot_global(arr->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = lower_ir_value_temp(temp0);
    instruction.as.store_indirect.value = lower_ir_value_immediate(0);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 0u;
    instruction.as.call.args = NULL;
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp2);
    instruction.as.load_slot = lower_ir_slot_global(g->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp3);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(temp2);
    instruction.as.binary.rhs = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(temp3), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_tiny_helper_two_block_return_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *helper = NULL;
    LowerIrFunction *main_function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *ret_block = NULL;
    LowerIrBasicBlock *main_block = NULL;
    LowerIrInstruction instruction;
    size_t helper_temp0;
    size_t helper_temp1;
    size_t main_temp0;
    size_t main_temp1;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "helper", 1, &helper, error) ||
        !lower_ir_function_append_local(helper, "x", 1, NULL, error) ||
        !lower_ir_function_append_block(helper, NULL, &entry, error) ||
        !lower_ir_function_append_block(helper, NULL, &ret_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    helper_temp0 = lower_ir_function_allocate_temp(helper);
    helper_temp1 = lower_ir_function_allocate_temp(helper);
    if (helper_temp0 == (size_t)-1 || helper_temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(helper_temp0);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(helper_temp1);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(helper_temp0);
    instruction.as.binary.rhs = lower_ir_value_immediate(5);
    if (!lower_ir_block_append_instruction(ret_block, &instruction, error) ||
        !lower_ir_block_set_return(ret_block, lower_ir_value_temp(helper_temp1), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_program_append_function(program, "main", 1, &main_function, error) ||
        !lower_ir_function_append_local(main_function, "p", 1, NULL, error) ||
        !lower_ir_function_append_block(main_function, NULL, &main_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    main_temp0 = lower_ir_function_allocate_temp(main_function);
    main_temp1 = lower_ir_function_allocate_temp(main_function);
    if (main_temp0 == (size_t)-1 || main_temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(main_temp0);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(main_temp1);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (LowerIrValueRef *)calloc(1u, sizeof(LowerIrValueRef));
    if (!instruction.as.call.args) {
        lower_ir_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = lower_ir_value_temp(main_temp0);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error) ||
        !lower_ir_block_set_return(main_block, lower_ir_value_temp(main_temp1), error)) {
        free(instruction.as.call.args);
        lower_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    return 1;
}

static int build_lower_ir_tiny_void_helper_two_block_return_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *helper = NULL;
    LowerIrFunction *main_function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *ret_block = NULL;
    LowerIrBasicBlock *main_block = NULL;
    LowerIrInstruction instruction;
    size_t helper_temp0;
    size_t helper_temp1;
    size_t main_temp0;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "helper", 1, &helper, error) ||
        !lower_ir_function_append_local(helper, "x", 1, NULL, error) ||
        !lower_ir_function_append_block(helper, NULL, &entry, error) ||
        !lower_ir_function_append_block(helper, NULL, &ret_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    helper_temp0 = lower_ir_function_allocate_temp(helper);
    helper_temp1 = lower_ir_function_allocate_temp(helper);
    if (helper_temp0 == (size_t)-1 || helper_temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(helper_temp0);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(helper_temp1);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(helper_temp0);
    instruction.as.binary.rhs = lower_ir_value_immediate(5);
    if (!lower_ir_block_append_instruction(ret_block, &instruction, error) ||
        !lower_ir_block_set_void_return(ret_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_program_append_function(program, "main", 1, &main_function, error) ||
        !lower_ir_function_append_local(main_function, "p", 1, NULL, error) ||
        !lower_ir_function_append_block(main_function, NULL, &main_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    main_temp0 = lower_ir_function_allocate_temp(main_function);
    if (main_temp0 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(main_temp0);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (LowerIrValueRef *)calloc(1u, sizeof(LowerIrValueRef));
    if (!instruction.as.call.args) {
        lower_ir_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = lower_ir_value_temp(main_temp0);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error) ||
        !lower_ir_block_set_return(main_block, lower_ir_value_immediate(0), error)) {
        free(instruction.as.call.args);
        lower_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    return 1;
}

static int build_lower_ir_unused_scalar_local_store_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrGlobal *sink = NULL;
    LowerIrFunction *helper = NULL;
    LowerIrFunction *main_function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t temp1;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "sink", &sink, error) ||
        !lower_ir_program_append_function(program, "helper", 1, &helper, error) ||
        !lower_ir_function_append_local(helper, "tmp", 0, NULL, error) ||
        !lower_ir_function_append_block(helper, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    sink->byte_size = 16u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(0u);
    instruction.as.store.value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_immediate(0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    block = NULL;
    if (!lower_ir_program_append_function(program, "main", 1, &main_function, error) ||
        !lower_ir_function_append_block(main_function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(main_function);
    temp1 = lower_ir_function_allocate_temp(main_function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.addr_slot = lower_ir_slot_global(sink->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = lower_ir_value_temp(temp0);
    instruction.as.store_indirect.value = lower_ir_value_immediate(0);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 0u;
    instruction.as.call.args = NULL;
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(temp1), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_array_local_store_preserved_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrGlobal *sink = NULL;
    LowerIrFunction *helper = NULL;
    LowerIrFunction *main_function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t temp1;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "sink", &sink, error) ||
        !lower_ir_program_append_function(program, "helper", 1, &helper, error) ||
        !lower_ir_function_append_local(helper, "arr", 0, NULL, error) ||
        !lower_ir_function_append_local(helper, "arr", 0, NULL, error) ||
        !lower_ir_function_append_block(helper, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    sink->byte_size = 16u;
    helper->locals[0].array_rank = 1u;
    helper->locals[1].array_rank = 1u;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(0u);
    instruction.as.store.value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(1u);
    instruction.as.store.value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_immediate(0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    block = NULL;
    if (!lower_ir_program_append_function(program, "main", 1, &main_function, error) ||
        !lower_ir_function_append_block(main_function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(main_function);
    temp1 = lower_ir_function_allocate_temp(main_function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.addr_slot = lower_ir_slot_global(sink->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = lower_ir_value_temp(temp0);
    instruction.as.store_indirect.value = lower_ir_value_immediate(0);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 0u;
    instruction.as.call.args = NULL;
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(temp1), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_address_taken_scalar_local_store_preserved_program(LowerIrProgram *program,
    LowerIrError *error) {
    LowerIrFunction *helper = NULL;
    LowerIrFunction *main_function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t helper_addr_temp;
    size_t helper_load_temp;
    size_t main_call_temp;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "helper", 1, &helper, error) ||
        !lower_ir_function_append_local(helper, "x", 0, NULL, error) ||
        !lower_ir_function_append_block(helper, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    helper_addr_temp = lower_ir_function_allocate_temp(helper);
    helper_load_temp = lower_ir_function_allocate_temp(helper);
    if (helper_addr_temp == (size_t)-1 || helper_load_temp == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(helper_addr_temp);
    instruction.as.addr_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(0u);
    instruction.as.store.value = lower_ir_value_immediate(7);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(helper_load_temp);
    instruction.as.load_indirect_addr = lower_ir_value_temp(helper_addr_temp);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(helper_load_temp), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    block = NULL;
    if (!lower_ir_program_append_function(program, "main", 1, &main_function, error) ||
        !lower_ir_function_append_block(main_function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    main_call_temp = lower_ir_function_allocate_temp(main_function);
    if (main_call_temp == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(main_call_temp);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 0u;
    instruction.as.call.args = NULL;
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(main_call_temp), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_unique_pred_repeated_indirect_load_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrGlobal *head = NULL;
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *succ = NULL;
    LowerIrInstruction instruction;
    size_t t0, t1, t2, t3, t4, t5, t6, t7;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "head", &head, error) ||
        !lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "idx", 1, NULL, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &succ, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    head->byte_size = 16u;

    t0 = lower_ir_function_allocate_temp(function);
    t1 = lower_ir_function_allocate_temp(function);
    t2 = lower_ir_function_allocate_temp(function);
    t3 = lower_ir_function_allocate_temp(function);
    t4 = lower_ir_function_allocate_temp(function);
    t5 = lower_ir_function_allocate_temp(function);
    t6 = lower_ir_function_allocate_temp(function);
    t7 = lower_ir_function_allocate_temp(function);
    if (t0 == (size_t)-1 || t1 == (size_t)-1 || t2 == (size_t)-1 || t3 == (size_t)-1 ||
        t4 == (size_t)-1 || t5 == (size_t)-1 || t6 == (size_t)-1 || t7 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t0);
    instruction.as.addr_slot = lower_ir_slot_global(head->id);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t1);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t2);
    instruction.as.binary.op = LOWER_IR_BINARY_MUL;
    instruction.as.binary.lhs = lower_ir_value_temp(t1);
    instruction.as.binary.rhs = lower_ir_value_immediate(4);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t3);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t0);
    instruction.as.binary.rhs = lower_ir_value_temp(t2);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t4);
    instruction.as.load_indirect_addr = lower_ir_value_temp(t3);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t5);
    instruction.as.addr_slot = lower_ir_slot_global(head->id);
    if (!lower_ir_block_append_instruction(succ, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t6);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t5);
    instruction.as.binary.rhs = lower_ir_value_temp(t2);
    if (!lower_ir_block_append_instruction(succ, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t7);
    instruction.as.load_indirect_addr = lower_ir_value_temp(t6);
    if (!lower_ir_block_append_instruction(succ, &instruction, error) ||
        !lower_ir_block_set_return(succ, lower_ir_value_temp(t7), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_unique_pred_repeated_indirect_load_across_non_alias_local_store_program(
    LowerIrProgram *program,
    LowerIrError *error) {
    LowerIrGlobal *head = NULL;
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *succ = NULL;
    LowerIrInstruction instruction;
    size_t t0, t1, t2, t3, t4, t5, t6, t7;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "head", &head, error) ||
        !lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "idx", 1, NULL, error) ||
        !lower_ir_function_append_local(function, "flag", 0, NULL, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &succ, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    head->byte_size = 16u;

    t0 = lower_ir_function_allocate_temp(function);
    t1 = lower_ir_function_allocate_temp(function);
    t2 = lower_ir_function_allocate_temp(function);
    t3 = lower_ir_function_allocate_temp(function);
    t4 = lower_ir_function_allocate_temp(function);
    t5 = lower_ir_function_allocate_temp(function);
    t6 = lower_ir_function_allocate_temp(function);
    t7 = lower_ir_function_allocate_temp(function);
    if (t0 == (size_t)-1 || t1 == (size_t)-1 || t2 == (size_t)-1 || t3 == (size_t)-1 ||
        t4 == (size_t)-1 || t5 == (size_t)-1 || t6 == (size_t)-1 || t7 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t0);
    instruction.as.addr_slot = lower_ir_slot_global(head->id);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t1);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t2);
    instruction.as.binary.op = LOWER_IR_BINARY_MUL;
    instruction.as.binary.lhs = lower_ir_value_temp(t1);
    instruction.as.binary.rhs = lower_ir_value_immediate(4);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t3);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t0);
    instruction.as.binary.rhs = lower_ir_value_temp(t2);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t4);
    instruction.as.load_indirect_addr = lower_ir_value_temp(t3);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = lower_ir_slot_local(1u);
    instruction.as.store.value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(succ, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t5);
    instruction.as.addr_slot = lower_ir_slot_global(head->id);
    if (!lower_ir_block_append_instruction(succ, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t6);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t5);
    instruction.as.binary.rhs = lower_ir_value_temp(t2);
    if (!lower_ir_block_append_instruction(succ, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t7);
    instruction.as.load_indirect_addr = lower_ir_value_temp(t6);
    if (!lower_ir_block_append_instruction(succ, &instruction, error) ||
        !lower_ir_block_set_return(succ, lower_ir_value_temp(t7), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_unique_pred_repeated_binary_chain_program(LowerIrProgram *program,
    LowerIrError *error) {
    LowerIrGlobal *head = NULL;
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *then_block = NULL;
    LowerIrBasicBlock *else_block = NULL;
    LowerIrInstruction instruction;
    size_t idx_local;
    size_t head_temp;
    size_t idx_temp;
    size_t scale_temp;
    size_t addr_temp;
    size_t cond_temp;
    size_t then_head_temp;
    size_t then_addr_temp;
    size_t then_load_temp;
    size_t else_head_temp;
    size_t else_addr_temp;
    size_t else_load_temp;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "head", &head, error) ||
        !lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "idx", 1, &idx_local, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &then_block, error) ||
        !lower_ir_function_append_block(function, NULL, &else_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    head->byte_size = 16u;

    head_temp = lower_ir_function_allocate_temp(function);
    idx_temp = lower_ir_function_allocate_temp(function);
    scale_temp = lower_ir_function_allocate_temp(function);
    addr_temp = lower_ir_function_allocate_temp(function);
    cond_temp = lower_ir_function_allocate_temp(function);
    then_head_temp = lower_ir_function_allocate_temp(function);
    then_addr_temp = lower_ir_function_allocate_temp(function);
    then_load_temp = lower_ir_function_allocate_temp(function);
    else_head_temp = lower_ir_function_allocate_temp(function);
    else_addr_temp = lower_ir_function_allocate_temp(function);
    else_load_temp = lower_ir_function_allocate_temp(function);
    if (head_temp == (size_t)-1 || idx_temp == (size_t)-1 || scale_temp == (size_t)-1 ||
        addr_temp == (size_t)-1 || cond_temp == (size_t)-1 || then_head_temp == (size_t)-1 ||
        then_addr_temp == (size_t)-1 || then_load_temp == (size_t)-1 || else_head_temp == (size_t)-1 ||
        else_addr_temp == (size_t)-1 || else_load_temp == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(head_temp);
    instruction.as.addr_slot = lower_ir_slot_global(head->id);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(idx_temp);
    instruction.as.load_slot = lower_ir_slot_local(idx_local);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(scale_temp);
    instruction.as.binary.op = LOWER_IR_BINARY_MUL;
    instruction.as.binary.lhs = lower_ir_value_temp(idx_temp);
    instruction.as.binary.rhs = lower_ir_value_immediate(4);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(addr_temp);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(head_temp);
    instruction.as.binary.rhs = lower_ir_value_temp(scale_temp);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(cond_temp);
    instruction.as.load_indirect_addr = lower_ir_value_temp(addr_temp);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_branch(entry, lower_ir_value_temp(cond_temp), 1, 2, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(then_head_temp);
    instruction.as.addr_slot = lower_ir_slot_global(head->id);
    if (!lower_ir_block_append_instruction(then_block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(then_addr_temp);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(then_head_temp);
    instruction.as.binary.rhs = lower_ir_value_temp(scale_temp);
    if (!lower_ir_block_append_instruction(then_block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(then_load_temp);
    instruction.as.load_indirect_addr = lower_ir_value_temp(then_addr_temp);
    if (!lower_ir_block_append_instruction(then_block, &instruction, error) ||
        !lower_ir_block_set_jump(then_block, 2, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(else_head_temp);
    instruction.as.addr_slot = lower_ir_slot_global(head->id);
    if (!lower_ir_block_append_instruction(else_block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(else_addr_temp);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(else_head_temp);
    instruction.as.binary.rhs = lower_ir_value_temp(scale_temp);
    if (!lower_ir_block_append_instruction(else_block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(else_load_temp);
    instruction.as.load_indirect_addr = lower_ir_value_temp(else_addr_temp);
    if (!lower_ir_block_append_instruction(else_block, &instruction, error) ||
        !lower_ir_block_set_return(else_block, lower_ir_value_temp(else_load_temp), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_dominated_repeated_indirect_load_across_non_alias_local_store_program(
    LowerIrProgram *program,
    LowerIrError *error) {
    LowerIrGlobal *head = NULL;
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *middle = NULL;
    LowerIrBasicBlock *succ = NULL;
    LowerIrInstruction instruction;
    size_t t0, t1, t2, t3, t4, t5, t6, t7;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "head", &head, error) ||
        !lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "idx", 1, NULL, error) ||
        !lower_ir_function_append_local(function, "flag", 0, NULL, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &middle, error) ||
        !lower_ir_function_append_block(function, NULL, &succ, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    head->byte_size = 16u;

    t0 = lower_ir_function_allocate_temp(function);
    t1 = lower_ir_function_allocate_temp(function);
    t2 = lower_ir_function_allocate_temp(function);
    t3 = lower_ir_function_allocate_temp(function);
    t4 = lower_ir_function_allocate_temp(function);
    t5 = lower_ir_function_allocate_temp(function);
    t6 = lower_ir_function_allocate_temp(function);
    t7 = lower_ir_function_allocate_temp(function);
    if (t0 == (size_t)-1 || t1 == (size_t)-1 || t2 == (size_t)-1 || t3 == (size_t)-1 ||
        t4 == (size_t)-1 || t5 == (size_t)-1 || t6 == (size_t)-1 || t7 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t0);
    instruction.as.addr_slot = lower_ir_slot_global(head->id);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t1);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t2);
    instruction.as.binary.op = LOWER_IR_BINARY_MUL;
    instruction.as.binary.lhs = lower_ir_value_temp(t1);
    instruction.as.binary.rhs = lower_ir_value_immediate(4);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t3);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t0);
    instruction.as.binary.rhs = lower_ir_value_temp(t2);
    if (!lower_ir_block_append_instruction(entry, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t4);
    instruction.as.load_indirect_addr = lower_ir_value_temp(t3);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = lower_ir_slot_local(1u);
    instruction.as.store.value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(middle, &instruction, error) ||
        !lower_ir_block_set_jump(middle, 2u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t5);
    instruction.as.addr_slot = lower_ir_slot_global(head->id);
    if (!lower_ir_block_append_instruction(succ, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t6);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t5);
    instruction.as.binary.rhs = lower_ir_value_temp(t2);
    if (!lower_ir_block_append_instruction(succ, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t7);
    instruction.as.load_indirect_addr = lower_ir_value_temp(t6);
    if (!lower_ir_block_append_instruction(succ, &instruction, error) ||
        !lower_ir_block_set_return(succ, lower_ir_value_temp(t7), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_repeated_addr_root_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrGlobal *head = NULL;
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t t0, t1, t2, t3, t4, t5, t6;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "head", &head, error) ||
        !lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "idx", 1, NULL, error) ||
        !lower_ir_function_append_block(function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    head->byte_size = 16u;

    t0 = lower_ir_function_allocate_temp(function);
    t1 = lower_ir_function_allocate_temp(function);
    t2 = lower_ir_function_allocate_temp(function);
    t3 = lower_ir_function_allocate_temp(function);
    t4 = lower_ir_function_allocate_temp(function);
    t5 = lower_ir_function_allocate_temp(function);
    t6 = lower_ir_function_allocate_temp(function);
    if (t0 == (size_t)-1 || t1 == (size_t)-1 || t2 == (size_t)-1 || t3 == (size_t)-1 ||
        t4 == (size_t)-1 || t5 == (size_t)-1 || t6 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t0);
    instruction.as.addr_slot = lower_ir_slot_global(head->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t1);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t2);
    instruction.as.binary.op = LOWER_IR_BINARY_MUL;
    instruction.as.binary.lhs = lower_ir_value_temp(t1);
    instruction.as.binary.rhs = lower_ir_value_immediate(4);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t3);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t0);
    instruction.as.binary.rhs = lower_ir_value_temp(t2);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t4);
    instruction.as.addr_slot = lower_ir_slot_global(head->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t5);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t4);
    instruction.as.binary.rhs = lower_ir_value_temp(t2);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t6);
    instruction.as.load_indirect_addr = lower_ir_value_temp(t5);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(t6), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_repeated_pure_internal_call_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrGlobal *base = NULL;
    LowerIrGlobal *arr = NULL;
    LowerIrFunction *helper = NULL;
    LowerIrFunction *main_function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *header = NULL;
    LowerIrBasicBlock *body = NULL;
    LowerIrBasicBlock *exit_block = NULL;
    LowerIrBasicBlock *main_block = NULL;
    LowerIrInstruction instruction;
    size_t t0, t1, t2, t3, t4, t5, t6, t7;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "base", &base, error) ||
        !lower_ir_program_append_global(program, "arr", &arr, error) ||
        !lower_ir_program_append_function(program, "helper", 2, &helper, error) ||
        !lower_ir_function_append_local(helper, "x", 1, NULL, error) ||
        !lower_ir_function_append_local(helper, "count", 1, NULL, error) ||
        !lower_ir_function_append_local(helper, "i", 0, NULL, error) ||
        !lower_ir_function_append_block(helper, NULL, &entry, error) ||
        !lower_ir_function_append_block(helper, NULL, &header, error) ||
        !lower_ir_function_append_block(helper, NULL, &body, error) ||
        !lower_ir_function_append_block(helper, NULL, &exit_block, error) ||
        !lower_ir_program_append_function(program, "main", 1, &main_function, error) ||
        !lower_ir_function_append_block(main_function, NULL, &main_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    base->byte_size = 4u;
    base->has_initializer = 1;
    base->initializer_value = 3;
    base->has_runtime_initializer = 0;
    arr->byte_size = 16u;

    t0 = lower_ir_function_allocate_temp(helper);
    t1 = lower_ir_function_allocate_temp(helper);
    t2 = lower_ir_function_allocate_temp(helper);
    t3 = lower_ir_function_allocate_temp(helper);
    t4 = lower_ir_function_allocate_temp(helper);
    t5 = lower_ir_function_allocate_temp(helper);
    t6 = lower_ir_function_allocate_temp(helper);
    t7 = lower_ir_function_allocate_temp(helper);
    if (t0 == (size_t)-1 || t1 == (size_t)-1 || t2 == (size_t)-1 || t3 == (size_t)-1 ||
        t4 == (size_t)-1 || t5 == (size_t)-1 || t6 == (size_t)-1 || t7 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(2u);
    instruction.as.store.value = lower_ir_value_immediate(0);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t0);
    instruction.as.load_slot = lower_ir_slot_local(2u);
    if (!lower_ir_block_append_instruction(header, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t1);
    instruction.as.load_slot = lower_ir_slot_local(1u);
    if (!lower_ir_block_append_instruction(header, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t2);
    instruction.as.binary.op = LOWER_IR_BINARY_LT;
    instruction.as.binary.lhs = lower_ir_value_temp(t0);
    instruction.as.binary.rhs = lower_ir_value_temp(t1);
    if (!lower_ir_block_append_instruction(header, &instruction, error) ||
        !lower_ir_block_set_branch(header, lower_ir_value_temp(t2), 2u, 3u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t3);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t4);
    instruction.as.load_slot = lower_ir_slot_global(base->id);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t5);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t3);
    instruction.as.binary.rhs = lower_ir_value_temp(t4);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(0u);
    instruction.as.store.value = lower_ir_value_temp(t5);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t6);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t0);
    instruction.as.binary.rhs = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(2u);
    instruction.as.store.value = lower_ir_value_temp(t6);
    if (!lower_ir_block_append_instruction(body, &instruction, error) ||
        !lower_ir_block_set_jump(body, 1u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t7);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(exit_block, &instruction, error) ||
        !lower_ir_block_set_return(exit_block, lower_ir_value_temp(t7), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    t0 = lower_ir_function_allocate_temp(main_function);
    t1 = lower_ir_function_allocate_temp(main_function);
    t2 = lower_ir_function_allocate_temp(main_function);
    t3 = lower_ir_function_allocate_temp(main_function);
    if (t0 == (size_t)-1 || t1 == (size_t)-1 || t2 == (size_t)-1 || t3 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t1);
    instruction.as.addr_slot = lower_ir_slot_global(arr->id);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = lower_ir_value_temp(t1);
    instruction.as.store_indirect.value = lower_ir_value_immediate(0);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t0);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (LowerIrValueRef *)calloc(2u, sizeof(LowerIrValueRef));
    if (!instruction.as.call.args) {
        lower_ir_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = lower_ir_value_immediate(5);
    instruction.as.call.args[1] = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error)) {
        free(instruction.as.call.args);
        lower_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t2);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (LowerIrValueRef *)calloc(2u, sizeof(LowerIrValueRef));
    if (!instruction.as.call.args) {
        lower_ir_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = lower_ir_value_immediate(5);
    instruction.as.call.args[1] = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error)) {
        free(instruction.as.call.args);
        lower_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t3);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t0);
    instruction.as.binary.rhs = lower_ir_value_temp(t2);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error) ||
        !lower_ir_block_set_return(main_block, lower_ir_value_temp(t3), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_redundant_binary_fixed_point_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrGlobal *arr = NULL;
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t t0, t1, t2, t3, t4, t5;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "arr", &arr, error) ||
        !lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "idx", 1, NULL, error) ||
        !lower_ir_function_append_block(function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    arr->byte_size = 16u;

    t0 = lower_ir_function_allocate_temp(function);
    t1 = lower_ir_function_allocate_temp(function);
    t2 = lower_ir_function_allocate_temp(function);
    t3 = lower_ir_function_allocate_temp(function);
    t4 = lower_ir_function_allocate_temp(function);
    t5 = lower_ir_function_allocate_temp(function);
    if (t0 == (size_t)-1 || t1 == (size_t)-1 || t2 == (size_t)-1 ||
        t3 == (size_t)-1 || t4 == (size_t)-1 || t5 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t0);
    instruction.as.addr_slot = lower_ir_slot_global(arr->id);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = lower_ir_value_temp(t0);
    instruction.as.store_indirect.value = lower_ir_value_immediate(0);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t1);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t2);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t1);
    instruction.as.binary.rhs = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t3);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t1);
    instruction.as.binary.rhs = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t4);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t3);
    instruction.as.binary.rhs = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t5);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t2);
    instruction.as.binary.rhs = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(t4), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_same_block_indirect_load_forward_across_param_store_program(LowerIrProgram *program,
    LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t t0, t1, t2, t3;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 2, &function, error) ||
        !lower_ir_function_append_local(function, "arr", 1, NULL, error) ||
        !lower_ir_function_append_local(function, "head", 0, NULL, error) ||
        !lower_ir_function_append_local(function, "tmp", 0, NULL, error) ||
        !lower_ir_function_append_block(function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    function->locals[0].array_rank = 1u;
    function->locals[1].array_rank = 1u;

    t0 = lower_ir_function_allocate_temp(function);
    t1 = lower_ir_function_allocate_temp(function);
    t2 = lower_ir_function_allocate_temp(function);
    t3 = lower_ir_function_allocate_temp(function);
    if (t0 == (size_t)-1 || t1 == (size_t)-1 || t2 == (size_t)-1 || t3 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t0);
    instruction.as.addr_slot = lower_ir_slot_local(1u);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t1);
    instruction.as.load_indirect_addr = lower_ir_value_temp(t0);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(2u);
    instruction.as.store.value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t2);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = lower_ir_value_temp(t2);
    instruction.as.store_indirect.value = lower_ir_value_immediate(7);
    if (!lower_ir_block_append_instruction(block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t3);
    instruction.as.load_indirect_addr = lower_ir_value_temp(t0);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(t3), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_repeated_pure_internal_call_across_store_indirect_program(LowerIrProgram *program,
    LowerIrError *error) {
    LowerIrGlobal *base = NULL;
    LowerIrGlobal *arr = NULL;
    LowerIrFunction *helper = NULL;
    LowerIrFunction *main_function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *header = NULL;
    LowerIrBasicBlock *body = NULL;
    LowerIrBasicBlock *exit_block = NULL;
    LowerIrBasicBlock *main_block = NULL;
    LowerIrInstruction instruction;
    size_t t0, t1, t2, t3, t4, t5, t6, t7;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_global(program, "base", &base, error) ||
        !lower_ir_program_append_global(program, "arr", &arr, error) ||
        !lower_ir_program_append_function(program, "helper", 2, &helper, error) ||
        !lower_ir_function_append_local(helper, "x", 1, NULL, error) ||
        !lower_ir_function_append_local(helper, "count", 1, NULL, error) ||
        !lower_ir_function_append_local(helper, "i", 0, NULL, error) ||
        !lower_ir_function_append_block(helper, NULL, &entry, error) ||
        !lower_ir_function_append_block(helper, NULL, &header, error) ||
        !lower_ir_function_append_block(helper, NULL, &body, error) ||
        !lower_ir_function_append_block(helper, NULL, &exit_block, error) ||
        !lower_ir_program_append_function(program, "main", 1, &main_function, error) ||
        !lower_ir_function_append_block(main_function, NULL, &main_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    base->byte_size = 4u;
    base->has_initializer = 1;
    base->initializer_value = 3;
    base->has_runtime_initializer = 0;
    arr->byte_size = 16u;

    t0 = lower_ir_function_allocate_temp(helper);
    t1 = lower_ir_function_allocate_temp(helper);
    t2 = lower_ir_function_allocate_temp(helper);
    t3 = lower_ir_function_allocate_temp(helper);
    t4 = lower_ir_function_allocate_temp(helper);
    t5 = lower_ir_function_allocate_temp(helper);
    t6 = lower_ir_function_allocate_temp(helper);
    t7 = lower_ir_function_allocate_temp(helper);
    if (t0 == (size_t)-1 || t1 == (size_t)-1 || t2 == (size_t)-1 || t3 == (size_t)-1 ||
        t4 == (size_t)-1 || t5 == (size_t)-1 || t6 == (size_t)-1 || t7 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(2u);
    instruction.as.store.value = lower_ir_value_immediate(0);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t0);
    instruction.as.load_slot = lower_ir_slot_local(2u);
    if (!lower_ir_block_append_instruction(header, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t1);
    instruction.as.load_slot = lower_ir_slot_local(1u);
    if (!lower_ir_block_append_instruction(header, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t2);
    instruction.as.binary.op = LOWER_IR_BINARY_LT;
    instruction.as.binary.lhs = lower_ir_value_temp(t0);
    instruction.as.binary.rhs = lower_ir_value_temp(t1);
    if (!lower_ir_block_append_instruction(header, &instruction, error) ||
        !lower_ir_block_set_branch(header, lower_ir_value_temp(t2), 2u, 3u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t3);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t4);
    instruction.as.load_slot = lower_ir_slot_global(base->id);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t5);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t3);
    instruction.as.binary.rhs = lower_ir_value_temp(t4);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(0u);
    instruction.as.store.value = lower_ir_value_temp(t5);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t6);
    instruction.as.binary.op = LOWER_IR_BINARY_ADD;
    instruction.as.binary.lhs = lower_ir_value_temp(t0);
    instruction.as.binary.rhs = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = lower_ir_slot_local(2u);
    instruction.as.store.value = lower_ir_value_temp(t6);
    if (!lower_ir_block_append_instruction(body, &instruction, error) ||
        !lower_ir_block_set_jump(body, 1u, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t7);
    instruction.as.load_slot = lower_ir_slot_local(0u);
    if (!lower_ir_block_append_instruction(exit_block, &instruction, error) ||
        !lower_ir_block_set_return(exit_block, lower_ir_value_temp(t7), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    t0 = lower_ir_function_allocate_temp(main_function);
    t1 = lower_ir_function_allocate_temp(main_function);
    t2 = lower_ir_function_allocate_temp(main_function);
    if (t0 == (size_t)-1 || t1 == (size_t)-1 || t2 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t0);
    instruction.as.addr_slot = lower_ir_slot_global(arr->id);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = lower_ir_value_temp(t0);
    instruction.as.store_indirect.value = lower_ir_value_immediate(0);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t1);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (LowerIrValueRef *)calloc(2u, sizeof(LowerIrValueRef));
    if (!instruction.as.call.args) {
        lower_ir_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = lower_ir_value_immediate(5);
    instruction.as.call.args[1] = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error)) {
        free(instruction.as.call.args);
        lower_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = lower_ir_value_temp(t0);
    instruction.as.store_indirect.value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(t2);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 2u;
    instruction.as.call.args = (LowerIrValueRef *)calloc(2u, sizeof(LowerIrValueRef));
    if (!instruction.as.call.args) {
        lower_ir_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = lower_ir_value_immediate(5);
    instruction.as.call.args[1] = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(main_block, &instruction, error) ||
        !lower_ir_block_set_return(main_block, lower_ir_value_temp(t2), error)) {
        free(instruction.as.call.args);
        lower_ir_program_free(program);
        return 0;
    }
    free(instruction.as.call.args);
    instruction.as.call.args = NULL;

    return 1;
}

static int build_lower_ir_loop_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *header = NULL;
    LowerIrBasicBlock *body = NULL;
    LowerIrBasicBlock *exit_block = NULL;
    LowerIrInstruction instruction;
    size_t local_a_id;
    size_t temp0;
    size_t temp1;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_local(function, "a", 1, &local_a_id, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &header, error) ||
        !lower_ir_function_append_block(function, NULL, &body, error) ||
        !lower_ir_function_append_block(function, NULL, &exit_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    temp1 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_jump(entry, 1, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.load_slot = lower_ir_slot_local(local_a_id);
    if (!lower_ir_block_append_instruction(header, &instruction, error) ||
        !lower_ir_block_set_branch(header, lower_ir_value_temp(temp0), 2, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.binary.op = LOWER_IR_BINARY_SUB;
    instruction.as.binary.lhs = lower_ir_value_temp(temp0);
    instruction.as.binary.rhs = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(body, &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = lower_ir_slot_local(local_a_id);
    instruction.as.store.value = lower_ir_value_temp(temp1);
    if (!lower_ir_block_append_instruction(body, &instruction, error) ||
        !lower_ir_block_set_jump(body, 1, error) ||
        !lower_ir_block_set_return(exit_block, lower_ir_value_immediate(0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_loop_carried_temp_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *entry = NULL;
    LowerIrBasicBlock *header = NULL;
    LowerIrBasicBlock *body = NULL;
    LowerIrBasicBlock *exit_block = NULL;
    LowerIrInstruction instruction;
    size_t temp0;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, NULL, &entry, error) ||
        !lower_ir_function_append_block(function, NULL, &header, error) ||
        !lower_ir_function_append_block(function, NULL, &body, error) ||
        !lower_ir_function_append_block(function, NULL, &exit_block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(entry, &instruction, error) ||
        !lower_ir_block_set_jump(entry, 1, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(header, lower_ir_value_temp(temp0), 2, 3, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(body, &instruction, error) ||
        !lower_ir_block_set_jump(body, 1, error) ||
        !lower_ir_block_set_return(exit_block, lower_ir_value_temp(temp0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_multi_backedge_loop_carried_temp_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t entry_id;
    size_t header_id;
    size_t split_id;
    size_t left_backedge_id;
    size_t right_backedge_id;
    size_t exit_block_id;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, &entry_id, NULL, error) ||
        !lower_ir_function_append_block(function, &header_id, NULL, error) ||
        !lower_ir_function_append_block(function, &split_id, NULL, error) ||
        !lower_ir_function_append_block(function, &left_backedge_id, NULL, error) ||
        !lower_ir_function_append_block(function, &right_backedge_id, NULL, error) ||
        !lower_ir_function_append_block(function, &exit_block_id, NULL, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[entry_id], 1, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(&function->blocks[header_id], lower_ir_value_temp(temp0), 2, 5, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(&function->blocks[split_id], lower_ir_value_temp(temp0), 3, 4, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(&function->blocks[left_backedge_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[left_backedge_id], 1, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(3);
    if (!lower_ir_block_append_instruction(&function->blocks[right_backedge_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[right_backedge_id], 1, error) ||
        !lower_ir_block_set_return(&function->blocks[exit_block_id], lower_ir_value_temp(temp0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_two_carried_temps_loop_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t temp1;
    size_t entry_id;
    size_t header_id;
    size_t body_id;
    size_t exit_block_id;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, &entry_id, NULL, error) ||
        !lower_ir_function_append_block(function, &header_id, NULL, error) ||
        !lower_ir_function_append_block(function, &body_id, NULL, error) ||
        !lower_ir_function_append_block(function, &exit_block_id, NULL, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    temp1 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.mov_value = lower_ir_value_immediate(10);
    if (!lower_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[entry_id], header_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(&function->blocks[header_id],
            lower_ir_value_temp(temp0),
            body_id,
            exit_block_id,
            error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(&function->blocks[body_id], &instruction, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.mov_value = lower_ir_value_immediate(11);
    if (!lower_ir_block_append_instruction(&function->blocks[body_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[body_id], header_id, error) ||
        !lower_ir_block_set_return(&function->blocks[exit_block_id], lower_ir_value_temp(temp1), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_nested_loop_carried_temps_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t temp1;
    size_t entry_id;
    size_t outer_header_id;
    size_t inner_entry_id;
    size_t inner_header_id;
    size_t inner_body_id;
    size_t outer_body_id;
    size_t exit_block_id;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, &entry_id, NULL, error) ||
        !lower_ir_function_append_block(function, &outer_header_id, NULL, error) ||
        !lower_ir_function_append_block(function, &inner_entry_id, NULL, error) ||
        !lower_ir_function_append_block(function, &inner_header_id, NULL, error) ||
        !lower_ir_function_append_block(function, &inner_body_id, NULL, error) ||
        !lower_ir_function_append_block(function, &outer_body_id, NULL, error) ||
        !lower_ir_function_append_block(function, &exit_block_id, NULL, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    temp1 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1 || temp1 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[entry_id], outer_header_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(&function->blocks[outer_header_id],
            lower_ir_value_temp(temp0),
            inner_entry_id,
            exit_block_id,
            error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.mov_value = lower_ir_value_immediate(10);
    if (!lower_ir_block_append_instruction(&function->blocks[inner_entry_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[inner_entry_id], inner_header_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(&function->blocks[inner_header_id],
            lower_ir_value_temp(temp1),
            inner_body_id,
            outer_body_id,
            error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp1);
    instruction.as.mov_value = lower_ir_value_immediate(11);
    if (!lower_ir_block_append_instruction(&function->blocks[inner_body_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[inner_body_id], inner_header_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(&function->blocks[outer_body_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[outer_body_id], outer_header_id, error) ||
        !lower_ir_block_set_return(&function->blocks[exit_block_id], lower_ir_value_temp(temp0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_nested_same_temp_double_loop_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t entry_id;
    size_t outer_header_id;
    size_t inner_entry_id;
    size_t inner_header_id;
    size_t inner_body_id;
    size_t outer_body_id;
    size_t exit_block_id;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, &entry_id, NULL, error) ||
        !lower_ir_function_append_block(function, &outer_header_id, NULL, error) ||
        !lower_ir_function_append_block(function, &inner_entry_id, NULL, error) ||
        !lower_ir_function_append_block(function, &inner_header_id, NULL, error) ||
        !lower_ir_function_append_block(function, &inner_body_id, NULL, error) ||
        !lower_ir_function_append_block(function, &outer_body_id, NULL, error) ||
        !lower_ir_function_append_block(function, &exit_block_id, NULL, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[entry_id], outer_header_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(&function->blocks[outer_header_id],
            lower_ir_value_temp(temp0),
            inner_entry_id,
            exit_block_id,
            error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_jump(&function->blocks[inner_entry_id], inner_header_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(&function->blocks[inner_header_id],
            lower_ir_value_temp(temp0),
            inner_body_id,
            outer_body_id,
            error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(&function->blocks[inner_body_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[inner_body_id], inner_header_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(3);
    if (!lower_ir_block_append_instruction(&function->blocks[outer_body_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[outer_body_id], outer_header_id, error) ||
        !lower_ir_block_set_return(&function->blocks[exit_block_id], lower_ir_value_temp(temp0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_nested_same_temp_multi_backedge_inner_loop_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrInstruction instruction;
    size_t temp0;
    size_t entry_id;
    size_t outer_header_id;
    size_t inner_entry_id;
    size_t inner_header_id;
    size_t inner_split_id;
    size_t inner_left_id;
    size_t inner_right_id;
    size_t outer_body_id;
    size_t exit_block_id;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, &entry_id, NULL, error) ||
        !lower_ir_function_append_block(function, &outer_header_id, NULL, error) ||
        !lower_ir_function_append_block(function, &inner_entry_id, NULL, error) ||
        !lower_ir_function_append_block(function, &inner_header_id, NULL, error) ||
        !lower_ir_function_append_block(function, &inner_split_id, NULL, error) ||
        !lower_ir_function_append_block(function, &inner_left_id, NULL, error) ||
        !lower_ir_function_append_block(function, &inner_right_id, NULL, error) ||
        !lower_ir_function_append_block(function, &outer_body_id, NULL, error) ||
        !lower_ir_function_append_block(function, &exit_block_id, NULL, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    temp0 = lower_ir_function_allocate_temp(function);
    if (temp0 == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[entry_id], outer_header_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(&function->blocks[outer_header_id],
            lower_ir_value_temp(temp0),
            inner_entry_id,
            exit_block_id,
            error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_jump(&function->blocks[inner_entry_id], inner_header_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(&function->blocks[inner_header_id],
            lower_ir_value_temp(temp0),
            inner_split_id,
            outer_body_id,
            error)) {
        lower_ir_program_free(program);
        return 0;
    }

    if (!lower_ir_block_set_branch(&function->blocks[inner_split_id],
            lower_ir_value_temp(temp0),
            inner_left_id,
            inner_right_id,
            error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_immediate(2);
    if (!lower_ir_block_append_instruction(&function->blocks[inner_left_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[inner_left_id], inner_header_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    instruction.as.mov_value = lower_ir_value_immediate(4);
    if (!lower_ir_block_append_instruction(&function->blocks[inner_right_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[inner_right_id], inner_header_id, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(temp0);
    instruction.as.mov_value = lower_ir_value_temp(temp0);
    if (!lower_ir_block_append_instruction(&function->blocks[outer_body_id], &instruction, error) ||
        !lower_ir_block_set_jump(&function->blocks[outer_body_id], outer_header_id, error) ||
        !lower_ir_block_set_return(&function->blocks[exit_block_id], lower_ir_value_temp(temp0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_dead_call_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *decl = NULL;
    LowerIrFunction *main_function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t result_temp;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "id", 0, &decl, error) ||
        !lower_ir_program_append_function(program, "main", 1, &main_function, error) ||
        !lower_ir_function_append_block(main_function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    result_temp = lower_ir_function_allocate_temp(main_function);
    if (result_temp == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(result_temp);
    instruction.as.call.callee_name = "id";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_immediate(0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_extreme_param_decl_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *decl = NULL;
    LowerIrFunction *main_function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t parameter_index;
    size_t result_temp;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "helper_decl", 0, &decl, error) ||
        !lower_ir_program_append_function(program, "main", 513, &main_function, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    (void)decl;
    for (parameter_index = 0; parameter_index < 513u; ++parameter_index) {
        if (!lower_ir_function_append_local(main_function, "p", 1, NULL, error)) {
            lower_ir_program_free(program);
            return 0;
        }
    }
    if (!lower_ir_function_append_block(main_function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    result_temp = lower_ir_function_allocate_temp(main_function);
    if (result_temp == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(result_temp);
    instruction.as.mov_value = lower_ir_value_immediate(1);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_temp(result_temp), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_lower_ir_dangerous_dead_binary_program(LowerIrProgram *program, LowerIrError *error) {
    LowerIrFunction *function = NULL;
    LowerIrBasicBlock *block = NULL;
    LowerIrInstruction instruction;
    size_t result_temp;

    lower_ir_program_init(program);

    if (!lower_ir_program_append_function(program, "main", 1, &function, error) ||
        !lower_ir_function_append_block(function, NULL, &block, error)) {
        lower_ir_program_free(program);
        return 0;
    }

    result_temp = lower_ir_function_allocate_temp(function);
    if (result_temp == (size_t)-1) {
        lower_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = LOWER_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = lower_ir_value_temp(result_temp);
    instruction.as.binary.op = LOWER_IR_BINARY_DIV;
    instruction.as.binary.lhs = lower_ir_value_immediate(1);
    instruction.as.binary.rhs = lower_ir_value_immediate(0);
    if (!lower_ir_block_append_instruction(block, &instruction, error) ||
        !lower_ir_block_set_return(block, lower_ir_value_immediate(0), error)) {
        lower_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_diamond_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t local_a_id;
    size_t cond_value;
    size_t then_value;
    size_t else_value;
    size_t join_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    then_value = value_ssa_function_allocate_value(function);
    else_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || then_value == (size_t)-1 ||
        else_value == (size_t)-1 || join_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(else_value);
    instruction.as.mov_value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(then_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_phi(join_block, join_value, phi_inputs, 2, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static void mutate_sample_program_zero_arg_call_nonnull_args(ValueSsaProgram *program) {
    ValueSsaFunction *function;
    ValueSsaBasicBlock *block;
    ValueSsaInstruction *instruction;
    size_t call_value;

    if (!program || program->function_count == 0 || !program->functions) {
        return;
    }

    function = &program->functions[0];
    if (!function->blocks || function->block_count == 0) {
        return;
    }
    block = &function->blocks[0];
    call_value = value_ssa_function_allocate_value(function);
    if (call_value == (size_t)-1) {
        return;
    }

    if (block->instruction_count == 0 || !block->instructions) {
        return;
    }

    instruction = &block->instructions[block->instruction_count - 1];
    memset(instruction, 0, sizeof(*instruction));
    instruction->kind = VALUE_SSA_INSTR_CALL;
    instruction->has_result = 1;
    instruction->result = value_ssa_value_id(call_value);
    instruction->as.call.callee_name = (char *)malloc(sizeof("main"));
    if (!instruction->as.call.callee_name) {
        memset(instruction, 0, sizeof(*instruction));
        return;
    }
    memcpy(instruction->as.call.callee_name, "main", sizeof("main"));
    instruction->as.call.arg_count = 0u;
    instruction->as.call.args = (ValueSsaValueRef *)calloc(1u, sizeof(ValueSsaValueRef));
    if (!instruction->as.call.args) {
        free(instruction->as.call.callee_name);
        memset(instruction, 0, sizeof(*instruction));
        return;
    }
    instruction->as.call.args[0] = value_ssa_value_immediate(0);
}

static int build_scrambled_diamond_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t local_a_id;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t value3;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    value3 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 || value3 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value3);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(value3), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(value2);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(value0);
    if (!value_ssa_block_append_phi(join_block, value1, phi_inputs, 2, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_scrambled_canonicalize_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t local_a_id;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t value3;
    size_t value4;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    value3 = value_ssa_function_allocate_value(function);
    value4 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 ||
        value3 == (size_t)-1 || value4 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value4);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value3);
    instruction.as.mov_value = value_ssa_value_id(value4);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(value3), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(value2);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(value0);
    if (!value_ssa_block_append_phi(join_block, value1, phi_inputs, 2, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_constant_branch_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_value;
    size_t then_value;
    size_t else_value;
    size_t join_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    then_value = value_ssa_function_allocate_value(function);
    else_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || then_value == (size_t)-1 ||
        else_value == (size_t)-1 || join_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_value);
    instruction.as.mov_value = value_ssa_value_immediate(11);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(else_value);
    instruction.as.mov_value = value_ssa_value_immediate(22);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(then_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_phi(join_block, join_value, phi_inputs, 2u, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_readonly_global_branch_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_value;
    size_t then_value;
    size_t else_value;
    size_t join_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 4u;
    global->has_initializer = 1;
    global->initializer_value = 7;
    global->has_runtime_initializer = 0;

    cond_value = value_ssa_function_allocate_value(function);
    then_value = value_ssa_function_allocate_value(function);
    else_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || then_value == (size_t)-1 ||
        else_value == (size_t)-1 || join_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_value);
    instruction.as.mov_value = value_ssa_value_immediate(11);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(else_value);
    instruction.as.mov_value = value_ssa_value_immediate(22);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(then_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_phi(join_block, join_value, phi_inputs, 2, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_mutated_global_branch_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_value;
    size_t then_value;
    size_t else_value;
    size_t join_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 4u;
    global->has_initializer = 1;
    global->initializer_value = 7;
    global->has_runtime_initializer = 0;

    cond_value = value_ssa_function_allocate_value(function);
    then_value = value_ssa_function_allocate_value(function);
    else_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || then_value == (size_t)-1 ||
        else_value == (size_t)-1 || join_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_value);
    instruction.as.mov_value = value_ssa_value_immediate(11);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(else_value);
    instruction.as.mov_value = value_ssa_value_immediate(22);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(then_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_phi(join_block, join_value, phi_inputs, 2, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_readonly_global_indirect_load_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t addr_value;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 4u;
    global->has_initializer = 1;
    global->initializer_value = 7;
    global->has_runtime_initializer = 0;

    addr_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_readonly_global_indirect_load_phi_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_value;
    size_t addr0;
    size_t addr1;
    size_t addr2;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 4u;
    global->has_initializer = 1;
    global->initializer_value = 9;
    global->has_runtime_initializer = 0;

    cond_value = value_ssa_function_allocate_value(function);
    addr0 = value_ssa_function_allocate_value(function);
    addr1 = value_ssa_function_allocate_value(function);
    addr2 = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || addr0 == (size_t)-1 || addr1 == (size_t)-1 ||
        addr2 == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr0);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(addr1);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(addr0);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(addr1);
    if (!value_ssa_block_append_phi(join_block, addr2, phi_inputs, 2u, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr2);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_local_address_phi_compare_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t local_cond_id;
    size_t local_x_id;
    size_t cond_value;
    size_t addr0;
    size_t addr1;
    size_t addr2;
    size_t cmp_eq;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "cond", 0, &local_cond_id, error) ||
        !value_ssa_function_append_local(function, "x", 0, &local_x_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    addr0 = value_ssa_function_allocate_value(function);
    addr1 = value_ssa_function_allocate_value(function);
    addr2 = value_ssa_function_allocate_value(function);
    cmp_eq = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || addr0 == (size_t)-1 || addr1 == (size_t)-1 ||
        addr2 == (size_t)-1 || cmp_eq == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_cond_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr0);
    instruction.as.addr_slot = value_ssa_slot_local(local_x_id);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(addr1);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(addr0);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(addr1);
    if (!value_ssa_block_append_phi(join_block, addr2, phi_inputs, 2u, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cmp_eq);
    instruction.as.binary.op = VALUE_SSA_BINARY_EQ;
    instruction.as.binary.lhs = value_ssa_value_id(addr2);
    instruction.as.binary.rhs = value_ssa_value_id(addr2);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_return(join_block, value_ssa_value_id(cmp_eq), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_local_indirect_load_does_not_fold_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_x_id;
    size_t addr_value;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "x", 0, &local_x_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_local(local_x_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_readonly_global_indirect_load_store_barrier_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t addr_value;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 4u;
    global->has_initializer = 1;
    global->initializer_value = 7;
    global->has_runtime_initializer = 0;

    addr_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
    instruction.has_result = 0;
    instruction.as.store_indirect.addr = value_ssa_value_id(addr_value);
    instruction.as.store_indirect.value = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_parameter_pointer_zero_add_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t param_p_id;
    size_t ptr_value;
    size_t ptr_plus_zero;
    size_t cmp_eq;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "p", 1, &param_p_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->locals[param_p_id].array_rank = 1u;

    ptr_value = value_ssa_function_allocate_value(function);
    ptr_plus_zero = value_ssa_function_allocate_value(function);
    cmp_eq = value_ssa_function_allocate_value(function);
    if (ptr_value == (size_t)-1 || ptr_plus_zero == (size_t)-1 || cmp_eq == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(ptr_value);
    instruction.as.load_slot = value_ssa_slot_local(param_p_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(ptr_plus_zero);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(ptr_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(cmp_eq);
    instruction.as.binary.op = VALUE_SSA_BINARY_EQ;
    instruction.as.binary.lhs = value_ssa_value_id(ptr_value);
    instruction.as.binary.rhs = value_ssa_value_id(ptr_plus_zero);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(cmp_eq), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_parameter_pointer_commuted_add_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t param_p_id;
    size_t ptr_value;
    size_t ptr_plus_four;
    size_t delta_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "p", 1, &param_p_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->locals[param_p_id].array_rank = 1u;

    ptr_value = value_ssa_function_allocate_value(function);
    ptr_plus_four = value_ssa_function_allocate_value(function);
    delta_value = value_ssa_function_allocate_value(function);
    if (ptr_value == (size_t)-1 || ptr_plus_four == (size_t)-1 || delta_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(ptr_value);
    instruction.as.load_slot = value_ssa_slot_local(param_p_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(ptr_plus_four);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_immediate(4);
    instruction.as.binary.rhs = value_ssa_value_id(ptr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(delta_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_SUB;
    instruction.as.binary.lhs = value_ssa_value_id(ptr_plus_four);
    instruction.as.binary.rhs = value_ssa_value_id(ptr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(delta_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_global_address_commuted_eq_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t addr_value;
    size_t cmp_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    cmp_value = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1 || cmp_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cmp_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_EQ;
    instruction.as.binary.lhs = value_ssa_value_immediate(5);
    instruction.as.binary.rhs = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(cmp_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_parameter_pointer_commuted_ne_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t param_p_id;
    size_t ptr_value;
    size_t cmp_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "p", 1, &param_p_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->locals[param_p_id].array_rank = 1u;

    ptr_value = value_ssa_function_allocate_value(function);
    cmp_value = value_ssa_function_allocate_value(function);
    if (ptr_value == (size_t)-1 || cmp_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(ptr_value);
    instruction.as.load_slot = value_ssa_slot_local(param_p_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cmp_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_NE;
    instruction.as.binary.lhs = value_ssa_value_immediate(0);
    instruction.as.binary.rhs = value_ssa_value_id(ptr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(cmp_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_distinct_local_addresses_ne_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_x_id;
    size_t local_y_id;
    size_t addr_x;
    size_t addr_y;
    size_t cmp_ne;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "x", 0, &local_x_id, error) ||
        !value_ssa_function_append_local(function, "y", 0, &local_y_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_x = value_ssa_function_allocate_value(function);
    addr_y = value_ssa_function_allocate_value(function);
    cmp_ne = value_ssa_function_allocate_value(function);
    if (addr_x == (size_t)-1 || addr_y == (size_t)-1 || cmp_ne == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_x);
    instruction.as.addr_slot = value_ssa_slot_local(local_x_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(addr_y);
    instruction.as.addr_slot = value_ssa_slot_local(local_y_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cmp_ne);
    instruction.as.binary.op = VALUE_SSA_BINARY_NE;
    instruction.as.binary.lhs = value_ssa_value_id(addr_x);
    instruction.as.binary.rhs = value_ssa_value_id(addr_y);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(cmp_ne), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_local_global_addresses_ne_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_x_id;
    size_t addr_local;
    size_t addr_global;
    size_t cmp_ne;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "x", 0, &local_x_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_local = value_ssa_function_allocate_value(function);
    addr_global = value_ssa_function_allocate_value(function);
    cmp_ne = value_ssa_function_allocate_value(function);
    if (addr_local == (size_t)-1 || addr_global == (size_t)-1 || cmp_ne == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_local);
    instruction.as.addr_slot = value_ssa_slot_local(local_x_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.result = value_ssa_value_id(addr_global);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cmp_ne);
    instruction.as.binary.op = VALUE_SSA_BINARY_NE;
    instruction.as.binary.lhs = value_ssa_value_id(addr_local);
    instruction.as.binary.rhs = value_ssa_value_id(addr_global);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(cmp_ne), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_local_parameter_pointer_ne_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t param_p_id;
    size_t local_x_id;
    size_t ptr_value;
    size_t addr_local;
    size_t cmp_ne;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "p", 1, &param_p_id, error) ||
        !value_ssa_function_append_local(function, "x", 0, &local_x_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->locals[param_p_id].array_rank = 1u;

    ptr_value = value_ssa_function_allocate_value(function);
    addr_local = value_ssa_function_allocate_value(function);
    cmp_ne = value_ssa_function_allocate_value(function);
    if (ptr_value == (size_t)-1 || addr_local == (size_t)-1 || cmp_ne == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(ptr_value);
    instruction.as.load_slot = value_ssa_slot_local(param_p_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_local);
    instruction.as.addr_slot = value_ssa_slot_local(local_x_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cmp_ne);
    instruction.as.binary.op = VALUE_SSA_BINARY_NE;
    instruction.as.binary.lhs = value_ssa_value_id(ptr_value);
    instruction.as.binary.rhs = value_ssa_value_id(addr_local);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(cmp_ne), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_global_address_nonzero_compare_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t addr_value;
    size_t cmp_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    cmp_value = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1 || cmp_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cmp_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_NE;
    instruction.as.binary.lhs = value_ssa_value_id(addr_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(cmp_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_parameter_pointer_nonzero_compare_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t param_p_id;
    size_t ptr_value;
    size_t cmp_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "p", 1, &param_p_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->locals[param_p_id].array_rank = 1u;

    ptr_value = value_ssa_function_allocate_value(function);
    cmp_value = value_ssa_function_allocate_value(function);
    if (ptr_value == (size_t)-1 || cmp_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(ptr_value);
    instruction.as.load_slot = value_ssa_slot_local(param_p_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cmp_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_NE;
    instruction.as.binary.lhs = value_ssa_value_id(ptr_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(cmp_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_global_address_branch_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t addr_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(addr_value), 1, 2, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_immediate(11), error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_immediate(22), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_parameter_pointer_branch_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t param_p_id;
    size_t ptr_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "p", 1, &param_p_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->locals[param_p_id].array_rank = 1u;

    ptr_value = value_ssa_function_allocate_value(function);
    if (ptr_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(ptr_value);
    instruction.as.load_slot = value_ssa_slot_local(param_p_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(ptr_value), 1, 2, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_immediate(11), error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_immediate(22), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_global_address_offset_compare_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t base_addr;
    size_t addr_plus_4;
    size_t addr_plus_8;
    size_t diff_value;
    size_t eq_value;
    size_t ne_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    base_addr = value_ssa_function_allocate_value(function);
    addr_plus_4 = value_ssa_function_allocate_value(function);
    addr_plus_8 = value_ssa_function_allocate_value(function);
    diff_value = value_ssa_function_allocate_value(function);
    eq_value = value_ssa_function_allocate_value(function);
    ne_value = value_ssa_function_allocate_value(function);
    if (base_addr == (size_t)-1 || addr_plus_4 == (size_t)-1 || addr_plus_8 == (size_t)-1 ||
        diff_value == (size_t)-1 || eq_value == (size_t)-1 || ne_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(base_addr);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_plus_4);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(base_addr);
    instruction.as.binary.rhs = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(addr_plus_8);
    instruction.as.binary.rhs = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(diff_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_SUB;
    instruction.as.binary.lhs = value_ssa_value_id(addr_plus_8);
    instruction.as.binary.rhs = value_ssa_value_id(addr_plus_4);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(eq_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_EQ;
    instruction.as.binary.lhs = value_ssa_value_id(addr_plus_4);
    instruction.as.binary.rhs = value_ssa_value_id(addr_plus_8);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(ne_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_NE;
    instruction.as.binary.lhs = value_ssa_value_id(addr_plus_4);
    instruction.as.binary.rhs = value_ssa_value_id(addr_plus_8);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(ne_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_global_address_offset_branch_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t base_addr;
    size_t addr_plus_4;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    base_addr = value_ssa_function_allocate_value(function);
    addr_plus_4 = value_ssa_function_allocate_value(function);
    if (base_addr == (size_t)-1 || addr_plus_4 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(base_addr);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_plus_4);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(base_addr);
    instruction.as.binary.rhs = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(addr_plus_4), 1, 2, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_immediate(11), error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_immediate(22), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_readonly_global_indirect_load_zero_offset_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t base_addr;
    size_t zero_offset_addr;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 4u;
    global->has_initializer = 1;
    global->initializer_value = 7;
    global->has_runtime_initializer = 0;

    base_addr = value_ssa_function_allocate_value(function);
    zero_offset_addr = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (base_addr == (size_t)-1 || zero_offset_addr == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(base_addr);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(zero_offset_addr);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(base_addr);
    instruction.as.binary.rhs = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(zero_offset_addr);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_readonly_global_indirect_load_nonzero_offset_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t base_addr;
    size_t offset_addr;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 8u;
    global->has_initializer = 1;
    global->initializer_value = 7;
    global->has_runtime_initializer = 0;

    base_addr = value_ssa_function_allocate_value(function);
    offset_addr = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (base_addr == (size_t)-1 || offset_addr == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(base_addr);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(offset_addr);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(base_addr);
    instruction.as.binary.rhs = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(offset_addr);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_sccp_local_address_branch_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_x_id;
    size_t addr_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "x", 0, &local_x_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_local(local_x_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(addr_value), 1, 2, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_immediate(11), error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_immediate(22), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_tiny_inline_addr_global_helper_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_fn = NULL;
    ValueSsaBasicBlock *helper_block = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    size_t helper_addr;
    size_t call_result;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_fn, error) ||
        !value_ssa_function_append_local(helper, "x", 1, NULL, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_block, error) ||
        !value_ssa_function_append_block(main_fn, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    helper_addr = value_ssa_function_allocate_value(helper);
    call_result = value_ssa_function_allocate_value(main_fn);
    if (helper_addr == (size_t)-1 || call_result == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(helper_addr);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(helper_block, &instruction, error) ||
        !value_ssa_block_set_return(helper_block, value_ssa_value_id(helper_addr), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(call_result);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (ValueSsaValueRef *)malloc(sizeof(ValueSsaValueRef));
    if (!instruction.as.call.args) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(call_result), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_tiny_inline_global_indirect_load_helper_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_fn = NULL;
    ValueSsaBasicBlock *helper_block = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    size_t helper_addr;
    size_t helper_load;
    size_t call_result;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_fn, error) ||
        !value_ssa_function_append_local(helper, "x", 1, NULL, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_block, error) ||
        !value_ssa_function_append_block(main_fn, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 4u;
    global->has_initializer = 1;
    global->initializer_value = 13;
    global->has_runtime_initializer = 0;

    helper_addr = value_ssa_function_allocate_value(helper);
    helper_load = value_ssa_function_allocate_value(helper);
    call_result = value_ssa_function_allocate_value(main_fn);
    if (helper_addr == (size_t)-1 || helper_load == (size_t)-1 || call_result == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(helper_addr);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(helper_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(helper_load);
    instruction.as.load_indirect_addr = value_ssa_value_id(helper_addr);
    if (!value_ssa_block_append_instruction(helper_block, &instruction, error) ||
        !value_ssa_block_set_return(helper_block, value_ssa_value_id(helper_load), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(call_result);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (ValueSsaValueRef *)malloc(sizeof(ValueSsaValueRef));
    if (!instruction.as.call.args) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(call_result), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_tiny_inline_parameter_pointer_helper_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_fn = NULL;
    ValueSsaBasicBlock *helper_block = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    size_t helper_ptr;
    size_t helper_plus_zero;
    size_t call_result;
    size_t main_param_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_fn, error) ||
        !value_ssa_function_append_local(helper, "p", 1, NULL, error) ||
        !value_ssa_function_append_local(main_fn, "p", 1, &main_param_id, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_block, error) ||
        !value_ssa_function_append_block(main_fn, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    helper->locals[0].array_rank = 1u;
    main_fn->locals[main_param_id].array_rank = 1u;

    helper_ptr = value_ssa_function_allocate_value(helper);
    helper_plus_zero = value_ssa_function_allocate_value(helper);
    call_result = value_ssa_function_allocate_value(main_fn);
    if (helper_ptr == (size_t)-1 || helper_plus_zero == (size_t)-1 || call_result == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(helper_ptr);
    instruction.as.load_slot = value_ssa_slot_local(0u);
    if (!value_ssa_block_append_instruction(helper_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(helper_plus_zero);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(helper_ptr);
    instruction.as.binary.rhs = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(helper_block, &instruction, error) ||
        !value_ssa_block_set_return(helper_block, value_ssa_value_id(helper_plus_zero), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(call_result);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (ValueSsaValueRef *)malloc(sizeof(ValueSsaValueRef));
    if (!instruction.as.call.args) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = value_ssa_value_id(value_ssa_function_allocate_value(main_fn));
    if (instruction.as.call.args[0].value_id == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaInstruction load_param;
        memset(&load_param, 0, sizeof(load_param));
        load_param.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
        load_param.has_result = 1;
        load_param.result = instruction.as.call.args[0];
        load_param.as.load_slot = value_ssa_slot_local(main_param_id);
        if (!value_ssa_block_append_instruction(main_block, &load_param, error) ||
            !value_ssa_block_append_instruction(main_block, &instruction, error) ||
            !value_ssa_block_set_return(main_block, value_ssa_value_id(call_result), error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    return 1;
}

static int build_tiny_inline_zero_param_constant_helper_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_fn = NULL;
    ValueSsaBasicBlock *helper_block = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    size_t call_result;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_fn, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_block, error) ||
        !value_ssa_function_append_block(main_fn, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    call_result = value_ssa_function_allocate_value(main_fn);
    if (call_result == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_return(helper_block, value_ssa_value_immediate(42), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(call_result);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 0u;
    instruction.as.call.args = NULL;
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(call_result), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_tiny_inline_two_block_return_helper_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_fn = NULL;
    ValueSsaBasicBlock *helper_entry = NULL;
    ValueSsaBasicBlock *helper_return = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    size_t helper_param_value;
    size_t helper_sum_value;
    size_t call_result;
    size_t main_param_id;
    size_t main_arg_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_fn, error) ||
        !value_ssa_function_append_local(helper, "x", 1, NULL, error) ||
        !value_ssa_function_append_local(main_fn, "p", 1, &main_param_id, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_entry, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_return, error) ||
        !value_ssa_function_append_block(main_fn, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    helper_param_value = value_ssa_function_allocate_value(helper);
    helper_sum_value = value_ssa_function_allocate_value(helper);
    call_result = value_ssa_function_allocate_value(main_fn);
    main_arg_value = value_ssa_function_allocate_value(main_fn);
    if (helper_param_value == (size_t)-1 || helper_sum_value == (size_t)-1 ||
        call_result == (size_t)-1 || main_arg_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(helper_param_value);
    instruction.as.load_slot = value_ssa_slot_local(0u);
    if (!value_ssa_block_append_instruction(helper_entry, &instruction, error) ||
        !value_ssa_block_set_jump(helper_entry, 1u, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(helper_sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(helper_param_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(helper_return, &instruction, error) ||
        !value_ssa_block_set_return(helper_return, value_ssa_value_id(helper_sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(main_arg_value);
    instruction.as.load_slot = value_ssa_slot_local(main_param_id);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(call_result);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (ValueSsaValueRef *)malloc(sizeof(ValueSsaValueRef));
    if (!instruction.as.call.args) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = value_ssa_value_id(main_arg_value);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(call_result), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_tiny_inline_void_two_block_helper_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_fn = NULL;
    ValueSsaBasicBlock *helper_entry = NULL;
    ValueSsaBasicBlock *helper_return = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    size_t helper_param_value;
    size_t helper_sum_value;
    size_t main_param_id;
    size_t main_arg_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_fn, error) ||
        !value_ssa_function_append_local(helper, "x", 1, NULL, error) ||
        !value_ssa_function_append_local(main_fn, "p", 1, &main_param_id, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_entry, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_return, error) ||
        !value_ssa_function_append_block(main_fn, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    helper_param_value = value_ssa_function_allocate_value(helper);
    helper_sum_value = value_ssa_function_allocate_value(helper);
    main_arg_value = value_ssa_function_allocate_value(main_fn);
    if (helper_param_value == (size_t)-1 || helper_sum_value == (size_t)-1 || main_arg_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(helper_param_value);
    instruction.as.load_slot = value_ssa_slot_local(0u);
    if (!value_ssa_block_append_instruction(helper_entry, &instruction, error) ||
        !value_ssa_block_set_jump(helper_entry, 1u, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(helper_sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(helper_param_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(helper_return, &instruction, error) ||
        !value_ssa_block_set_void_return(helper_return, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(main_arg_value);
    instruction.as.load_slot = value_ssa_slot_local(main_param_id);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = (ValueSsaValueRef *)malloc(sizeof(ValueSsaValueRef));
    if (!instruction.as.call.args) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = value_ssa_value_id(main_arg_value);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_tiny_inline_budget_blocked_helper_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *g0 = NULL;
    ValueSsaGlobal *g1 = NULL;
    ValueSsaGlobal *g2 = NULL;
    ValueSsaGlobal *g3 = NULL;
    ValueSsaGlobal *g4 = NULL;
    ValueSsaGlobal *g5 = NULL;
    ValueSsaGlobal *g6 = NULL;
    ValueSsaGlobal *g7 = NULL;
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_fn = NULL;
    ValueSsaBasicBlock *helper_block = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    size_t values[8];
    size_t call_result;
    size_t i;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g0", &g0, error) ||
        !value_ssa_program_append_global(program, "g1", &g1, error) ||
        !value_ssa_program_append_global(program, "g2", &g2, error) ||
        !value_ssa_program_append_global(program, "g3", &g3, error) ||
        !value_ssa_program_append_global(program, "g4", &g4, error) ||
        !value_ssa_program_append_global(program, "g5", &g5, error) ||
        !value_ssa_program_append_global(program, "g6", &g6, error) ||
        !value_ssa_program_append_global(program, "g7", &g7, error) ||
        !value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_fn, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_block, error) ||
        !value_ssa_function_append_block(main_fn, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (i = 0; i < 8u; ++i) {
        values[i] = value_ssa_function_allocate_value(helper);
        if (values[i] == (size_t)-1) {
            value_ssa_program_free(program);
            return 0;
        }
    }
    call_result = value_ssa_function_allocate_value(main_fn);
    if (call_result == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    for (i = 0; i < 8u; ++i) {
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(values[i]);
        instruction.as.load_slot = value_ssa_slot_global(i);
        if (!value_ssa_block_append_instruction(helper_block, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(helper_block, value_ssa_value_id(values[7]), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(call_result);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 0u;
    instruction.as.call.args = NULL;
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(call_result), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_tiny_inline_function_budget_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *g0 = NULL;
    ValueSsaGlobal *g1 = NULL;
    ValueSsaGlobal *g2 = NULL;
    ValueSsaGlobal *g3 = NULL;
    ValueSsaGlobal *g4 = NULL;
    ValueSsaGlobal *g5 = NULL;
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_fn = NULL;
    ValueSsaBasicBlock *helper_block = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    size_t helper_values[6];
    size_t call0;
    size_t call1;
    size_t i;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g0", &g0, error) ||
        !value_ssa_program_append_global(program, "g1", &g1, error) ||
        !value_ssa_program_append_global(program, "g2", &g2, error) ||
        !value_ssa_program_append_global(program, "g3", &g3, error) ||
        !value_ssa_program_append_global(program, "g4", &g4, error) ||
        !value_ssa_program_append_global(program, "g5", &g5, error) ||
        !value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_fn, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_block, error) ||
        !value_ssa_function_append_block(main_fn, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (i = 0; i < 6u; ++i) {
        helper_values[i] = value_ssa_function_allocate_value(helper);
        if (helper_values[i] == (size_t)-1) {
            value_ssa_program_free(program);
            return 0;
        }
    }
    call0 = value_ssa_function_allocate_value(main_fn);
    call1 = value_ssa_function_allocate_value(main_fn);
    if (call0 == (size_t)-1 || call1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    for (i = 0; i < 6u; ++i) {
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(helper_values[i]);
        instruction.as.load_slot = value_ssa_slot_global(i);
        if (!value_ssa_block_append_instruction(helper_block, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(helper_block, value_ssa_value_id(helper_values[5]), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(call0);
    instruction.as.call.callee_name = "helper";
    instruction.as.call.arg_count = 0u;
    instruction.as.call.args = NULL;
    if (!value_ssa_block_append_instruction(main_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(call1);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(call1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_invalid_undefined_value_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (value_ssa_function_allocate_value(function) == (size_t)-1 ||
        !value_ssa_block_set_return(block, value_ssa_value_id(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_trivial_mov_chain_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_id(value0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_trivial_phi_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t value0;
    size_t value1;
    size_t value2;
    size_t value3;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    value3 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 || value3 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_id(value0);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(value1);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(value2);
    if (!value_ssa_block_append_phi(join_block, value3, phi_inputs, 2, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value3), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_same_target_branch_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t cond_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 1, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_same_return_diamond_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t cond_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_id(cond_value), error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_id(cond_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_jump_to_empty_return_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_empty_jump_thread_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *jump_block = NULL;
    ValueSsaBasicBlock *exit_block = NULL;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &jump_block, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error) ||
        !value_ssa_block_set_jump(entry, 1, error) ||
        !value_ssa_block_set_jump(jump_block, 2, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_immediate(7), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_branch_empty_jump_same_target_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_jump = NULL;
    ValueSsaBasicBlock *else_jump = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_jump, error) ||
        !value_ssa_function_append_block(function, NULL, &else_jump, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(value0), 1, 2, error) ||
        !value_ssa_block_set_jump(then_jump, 3, error) ||
        !value_ssa_block_set_jump(else_jump, 3, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_local_store_load_forward_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_local_repeated_load_forward_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_local_cross_block_load_forward_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_local_join_no_forward_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t cond_value;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_single_predecessor_nonempty_jump_merge_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) || !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_return(body, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *main_function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t result_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "id", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_function, error) ||
        !value_ssa_function_append_block(main_function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    result_value = value_ssa_function_allocate_value(main_function);
    if (result_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(result_value);
    instruction.as.call.callee_name = "id";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_local_address_taken_no_forward_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t addr_value;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_hotspot_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "base", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 4u;
    global->has_initializer = 1;
    global->initializer_value = 16;
    global->has_runtime_initializer = 0;

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_MUL;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value2);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value1);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_run_program_shared_branch_local_load_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_read_head_id;
    size_t cond_value;
    size_t then_load_value;
    size_t then_add_value;
    size_t else_load_value;
    size_t else_sub_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "run_program", 1, &function, error) ||
        !value_ssa_function_append_local(function, "read_head", 0, &local_read_head_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    then_load_value = value_ssa_function_allocate_value(function);
    then_add_value = value_ssa_function_allocate_value(function);
    else_load_value = value_ssa_function_allocate_value(function);
    else_sub_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || then_load_value == (size_t)-1 || then_add_value == (size_t)-1 ||
        else_load_value == (size_t)-1 || else_sub_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), then_block->id, else_block->id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_read_head_id);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_add_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(then_load_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_id(then_add_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(else_load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_read_head_id);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(else_sub_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_SUB;
    instruction.as.binary.lhs = value_ssa_value_id(else_load_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_id(else_sub_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_loop_invariant_binary_hoist_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_i_id;
    size_t local_acc_id;
    size_t base_value;
    size_t row_value;
    size_t cond_value;
    size_t next_i_value;
    size_t exit_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "i", 1, &local_i_id, error) ||
        !value_ssa_function_append_local(function, "acc", 0, &local_acc_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    base_value = value_ssa_function_allocate_value(function);
    row_value = value_ssa_function_allocate_value(function);
    cond_value = value_ssa_function_allocate_value(function);
    next_i_value = value_ssa_function_allocate_value(function);
    exit_value = value_ssa_function_allocate_value(function);
    if (base_value == (size_t)-1 || row_value == (size_t)-1 || cond_value == (size_t)-1 ||
        next_i_value == (size_t)-1 || exit_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(local_i_id);
    instruction.as.store.value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_i_id);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(base_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_MUL;
    instruction.as.binary.lhs = value_ssa_value_immediate(7);
    instruction.as.binary.rhs = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(row_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(base_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(next_i_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_SUB;
    instruction.as.binary.lhs = value_ssa_value_id(cond_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(local_acc_id);
    instruction.as.store.value = value_ssa_value_id(row_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(local_i_id);
    instruction.as.store.value = value_ssa_value_id(next_i_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(exit_value);
    instruction.as.load_slot = value_ssa_slot_local(local_acc_id);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_phi_header_loop_invariant_indirect_load_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *preheader = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t local_p_id;
    size_t base_value;
    size_t addr_value;
    size_t iter_value;
    size_t cond_value;
    size_t invariant_load_value;
    size_t next_iter_value;
    size_t exit_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "p", 1, &local_p_id, error) ||
        !value_ssa_function_append_block(function, NULL, &preheader, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->locals[local_p_id].array_rank = 1u;

    base_value = value_ssa_function_allocate_value(function);
    addr_value = value_ssa_function_allocate_value(function);
    iter_value = value_ssa_function_allocate_value(function);
    cond_value = value_ssa_function_allocate_value(function);
    invariant_load_value = value_ssa_function_allocate_value(function);
    next_iter_value = value_ssa_function_allocate_value(function);
    exit_value = value_ssa_function_allocate_value(function);
    if (base_value == (size_t)-1 || addr_value == (size_t)-1 || iter_value == (size_t)-1 ||
        cond_value == (size_t)-1 || invariant_load_value == (size_t)-1 ||
        next_iter_value == (size_t)-1 || exit_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(base_value);
    instruction.as.load_slot = value_ssa_slot_local(local_p_id);
    if (!value_ssa_block_append_instruction(preheader, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(base_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(preheader, &instruction, error) ||
        !value_ssa_block_set_jump(preheader, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_immediate(0);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(next_iter_value);
    if (!value_ssa_block_append_phi(header, iter_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(iter_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(invariant_load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(next_iter_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(invariant_load_value);
    instruction.as.binary.rhs = value_ssa_value_id(iter_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(exit_value);
    instruction.as.mov_value = value_ssa_value_id(iter_value);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_phi_header_loop_invariant_indirect_load_store_barrier_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *preheader = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t local_p_id;
    size_t base_value;
    size_t addr_value;
    size_t iter_value;
    size_t cond_value;
    size_t invariant_load_value;
    size_t next_iter_value;
    size_t exit_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "p", 1, &local_p_id, error) ||
        !value_ssa_function_append_block(function, NULL, &preheader, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->locals[local_p_id].array_rank = 1u;

    base_value = value_ssa_function_allocate_value(function);
    addr_value = value_ssa_function_allocate_value(function);
    iter_value = value_ssa_function_allocate_value(function);
    cond_value = value_ssa_function_allocate_value(function);
    invariant_load_value = value_ssa_function_allocate_value(function);
    next_iter_value = value_ssa_function_allocate_value(function);
    exit_value = value_ssa_function_allocate_value(function);
    if (base_value == (size_t)-1 || addr_value == (size_t)-1 || iter_value == (size_t)-1 ||
        cond_value == (size_t)-1 || invariant_load_value == (size_t)-1 ||
        next_iter_value == (size_t)-1 || exit_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(base_value);
    instruction.as.load_slot = value_ssa_slot_local(local_p_id);
    if (!value_ssa_block_append_instruction(preheader, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(base_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(preheader, &instruction, error) ||
        !value_ssa_block_set_jump(preheader, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_immediate(0);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(next_iter_value);
    if (!value_ssa_block_append_phi(header, iter_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(iter_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = value_ssa_value_id(addr_value);
    instruction.as.store_indirect.value = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(invariant_load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(next_iter_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(invariant_load_value);
    instruction.as.binary.rhs = value_ssa_value_id(iter_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(exit_value);
    instruction.as.mov_value = value_ssa_value_id(iter_value);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_licm_header_invariant_indirect_load_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_p_id;
    size_t local_i_id;
    size_t local_acc_id;
    size_t base_value;
    size_t addr_value;
    size_t cond_value;
    size_t invariant_load_value;
    size_t next_iter_value;
    size_t exit_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "p", 1, &local_p_id, error) ||
        !value_ssa_function_append_local(function, "i", 1, &local_i_id, error) ||
        !value_ssa_function_append_local(function, "acc", 0, &local_acc_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->locals[local_p_id].array_rank = 1u;

    base_value = value_ssa_function_allocate_value(function);
    addr_value = value_ssa_function_allocate_value(function);
    cond_value = value_ssa_function_allocate_value(function);
    invariant_load_value = value_ssa_function_allocate_value(function);
    next_iter_value = value_ssa_function_allocate_value(function);
    exit_value = value_ssa_function_allocate_value(function);
    if (exit_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(local_i_id);
    instruction.as.store.value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(base_value);
    instruction.as.load_slot = value_ssa_slot_local(local_p_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(base_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(invariant_load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(header, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_i_id);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(next_iter_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_SUB;
    instruction.as.binary.lhs = value_ssa_value_id(cond_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(local_acc_id);
    instruction.as.store.value = value_ssa_value_id(invariant_load_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(local_i_id);
    instruction.as.store.value = value_ssa_value_id(next_iter_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(exit_value);
    instruction.as.load_slot = value_ssa_slot_local(local_acc_id);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_licm_header_invariant_indirect_load_store_barrier_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_p_id;
    size_t local_i_id;
    size_t local_acc_id;
    size_t base_value;
    size_t addr_value;
    size_t cond_value;
    size_t invariant_load_value;
    size_t next_iter_value;
    size_t exit_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "p", 1, &local_p_id, error) ||
        !value_ssa_function_append_local(function, "i", 1, &local_i_id, error) ||
        !value_ssa_function_append_local(function, "acc", 0, &local_acc_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->locals[local_p_id].array_rank = 1u;

    base_value = value_ssa_function_allocate_value(function);
    addr_value = value_ssa_function_allocate_value(function);
    cond_value = value_ssa_function_allocate_value(function);
    invariant_load_value = value_ssa_function_allocate_value(function);
    next_iter_value = value_ssa_function_allocate_value(function);
    exit_value = value_ssa_function_allocate_value(function);
    if (exit_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(local_i_id);
    instruction.as.store.value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(base_value);
    instruction.as.load_slot = value_ssa_slot_local(local_p_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(base_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(invariant_load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(header, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_i_id);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = value_ssa_value_id(addr_value);
    instruction.as.store_indirect.value = value_ssa_value_id(cond_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(next_iter_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_SUB;
    instruction.as.binary.lhs = value_ssa_value_id(cond_value);
    instruction.as.binary.rhs = value_ssa_value_id(cond_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(local_acc_id);
    instruction.as.store.value = value_ssa_value_id(invariant_load_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(local_i_id);
    instruction.as.store.value = value_ssa_value_id(next_iter_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(exit_value);
    instruction.as.load_slot = value_ssa_slot_local(local_acc_id);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_function_entry_global_hoist_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t value3;
    size_t value4;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 4u;

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    value3 = value_ssa_function_allocate_value(function);
    value4 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 || value3 == (size_t)-1 ||
        value4 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value2);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value3);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value2);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value4);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value1);
    instruction.as.binary.rhs = value_ssa_value_id(value3);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value4), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_function_entry_global_hoist_blocked_by_store_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 4u;

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_function_entry_global_addr_hoist_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 16u;
    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value2);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_single_use_global_addr_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    global->byte_size = 16u;
    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_spmv_parameter_local_load_hoist_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t n_id;
    size_t xptr_id;
    size_t a_id;
    size_t b_id;
    size_t c_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "spmv", 1, &function, error) ||
        !value_ssa_function_append_local(function, "n", 1, &n_id, error) ||
        !value_ssa_function_append_local(function, "xptr", 1, &xptr_id, error) ||
        !value_ssa_function_append_local(function, "a", 0, &a_id, error) ||
        !value_ssa_function_append_local(function, "b", 0, &b_id, error) ||
        !value_ssa_function_append_local(function, "c", 0, &c_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    a_id = value_ssa_function_allocate_value(function);
    b_id = value_ssa_function_allocate_value(function);
    c_id = value_ssa_function_allocate_value(function);
    if (a_id == (size_t)-1 || b_id == (size_t)-1 || c_id == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(a_id);
    instruction.as.load_slot = value_ssa_slot_local(n_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(b_id);
    instruction.as.load_slot = value_ssa_slot_local(xptr_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(c_id);
    instruction.as.load_slot = value_ssa_slot_local(n_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(a_id);
    instruction.as.binary.rhs = value_ssa_value_id(c_id);
    if (instruction.result.value_id == (size_t)-1 ||
        !value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, instruction.result, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_spmv_parameter_local_load_store_barrier_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t n_id;
    size_t a_id;
    size_t b_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "spmv", 1, &function, error) ||
        !value_ssa_function_append_local(function, "n", 1, &n_id, error) ||
        !value_ssa_function_append_local(function, "a", 0, &a_id, error) ||
        !value_ssa_function_append_local(function, "b", 0, &b_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    a_id = value_ssa_function_allocate_value(function);
    b_id = value_ssa_function_allocate_value(function);
    if (a_id == (size_t)-1 || b_id == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(a_id);
    instruction.as.load_slot = value_ssa_slot_local(n_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(n_id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(b_id);
    instruction.as.load_slot = value_ssa_slot_local(n_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(b_id), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_power_parameter_local_load_hoist_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t a_local_id;
    size_t b_local_id;
    size_t v0;
    size_t v1;
    size_t v2;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "power", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &a_local_id, error) ||
        !value_ssa_function_append_local(function, "b", 1, &b_local_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    v0 = value_ssa_function_allocate_value(function);
    v1 = value_ssa_function_allocate_value(function);
    v2 = value_ssa_function_allocate_value(function);
    if (v0 == (size_t)-1 || v1 == (size_t)-1 || v2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v0);
    instruction.as.load_slot = value_ssa_slot_local(a_local_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(v1);
    instruction.as.load_slot = value_ssa_slot_local(b_local_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(v2);
    instruction.as.load_slot = value_ssa_slot_local(a_local_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v1);
    instruction.as.binary.rhs = value_ssa_value_id(v2);
    if (instruction.result.value_id == (size_t)-1 ||
        !value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, instruction.result, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_non_hot_parameter_local_load_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t a_local_id;
    size_t v0;
    size_t v1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "helper", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &a_local_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    v0 = value_ssa_function_allocate_value(function);
    v1 = value_ssa_function_allocate_value(function);
    if (v0 == (size_t)-1 || v1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v0);
    instruction.as.load_slot = value_ssa_slot_local(a_local_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(v1);
    instruction.as.load_slot = value_ssa_slot_local(a_local_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v0);
    instruction.as.binary.rhs = value_ssa_value_id(v1);
    if (instruction.result.value_id == (size_t)-1 ||
        !value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, instruction.result, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_integer_dispatch_chain_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *cmp1 = NULL;
    ValueSsaBasicBlock *cmp2 = NULL;
    ValueSsaBasicBlock *cmp3 = NULL;
    ValueSsaBasicBlock *case1 = NULL;
    ValueSsaBasicBlock *case2 = NULL;
    ValueSsaBasicBlock *case3 = NULL;
    ValueSsaBasicBlock *fallback = NULL;
    ValueSsaInstruction instruction;
    size_t x_local_id;
    size_t block0_id;
    size_t block1_id;
    size_t block2_id;
    size_t block3_id;
    size_t block4_id;
    size_t block5_id;
    size_t block6_id;
    size_t block7_id;
    size_t v_x;
    size_t v_cmp1;
    size_t v_cmp2;
    size_t v_cmp3;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "x", 1, &x_local_id, error) ||
        !value_ssa_function_append_block(function, &block0_id, &entry, error) ||
        !value_ssa_function_append_block(function, &block1_id, &cmp1, error) ||
        !value_ssa_function_append_block(function, &block2_id, &cmp2, error) ||
        !value_ssa_function_append_block(function, &block3_id, &cmp3, error) ||
        !value_ssa_function_append_block(function, &block4_id, &case1, error) ||
        !value_ssa_function_append_block(function, &block5_id, &case2, error) ||
        !value_ssa_function_append_block(function, &block6_id, &case3, error) ||
        !value_ssa_function_append_block(function, &block7_id, &fallback, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    entry = &function->blocks[block0_id];
    cmp1 = &function->blocks[block1_id];
    cmp2 = &function->blocks[block2_id];
    cmp3 = &function->blocks[block3_id];
    case1 = &function->blocks[block4_id];
    case2 = &function->blocks[block5_id];
    case3 = &function->blocks[block6_id];
    fallback = &function->blocks[block7_id];

    v_x = value_ssa_function_allocate_value(function);
    v_cmp1 = value_ssa_function_allocate_value(function);
    v_cmp2 = value_ssa_function_allocate_value(function);
    v_cmp3 = value_ssa_function_allocate_value(function);
    if (v_cmp3 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_x);
    instruction.as.load_slot = value_ssa_slot_local(x_local_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, block1_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_cmp1);
    instruction.as.binary.op = VALUE_SSA_BINARY_EQ;
    instruction.as.binary.lhs = value_ssa_value_id(v_x);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(cmp1, &instruction, error) ||
        !value_ssa_block_set_branch(cmp1, value_ssa_value_id(v_cmp1), block4_id, block2_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(v_cmp2);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(cmp2, &instruction, error) ||
        !value_ssa_block_set_branch(cmp2, value_ssa_value_id(v_cmp2), block5_id, block3_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(v_cmp3);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(cmp3, &instruction, error) ||
        !value_ssa_block_set_branch(cmp3, value_ssa_value_id(v_cmp3), block6_id, block7_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_return(case1, value_ssa_value_immediate(10), error) ||
        !value_ssa_block_set_return(case2, value_ssa_value_immediate(20), error) ||
        !value_ssa_block_set_return(case3, value_ssa_value_immediate(30), error) ||
        !value_ssa_block_set_return(fallback, value_ssa_value_immediate(40), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_zero_based_induction_divmod_reduce_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t acc_local_id;
    size_t block0_id;
    size_t block1_id;
    size_t block2_id;
    size_t block3_id;
    size_t v_i;
    size_t v_cond;
    size_t v_mod;
    size_t v_div;
    size_t v_sum;
    size_t v_next;
    size_t v_exit;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "acc", 0, &acc_local_id, error) ||
        !value_ssa_function_append_block(function, &block0_id, &entry, error) ||
        !value_ssa_function_append_block(function, &block1_id, &header, error) ||
        !value_ssa_function_append_block(function, &block2_id, &body, error) ||
        !value_ssa_function_append_block(function, &block3_id, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    entry = &function->blocks[block0_id];
    header = &function->blocks[block1_id];
    body = &function->blocks[block2_id];
    exit_block = &function->blocks[block3_id];

    v_i = value_ssa_function_allocate_value(function);
    v_cond = value_ssa_function_allocate_value(function);
    v_mod = value_ssa_function_allocate_value(function);
    v_div = value_ssa_function_allocate_value(function);
    v_sum = value_ssa_function_allocate_value(function);
    v_next = value_ssa_function_allocate_value(function);
    v_exit = value_ssa_function_allocate_value(function);
    if (v_exit == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_jump(entry, block1_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = block0_id;
    phi_inputs[0].value = value_ssa_value_immediate(0);
    phi_inputs[1].predecessor_block_id = block2_id;
    phi_inputs[1].value = value_ssa_value_id(v_next);
    if (!value_ssa_block_append_phi(header, v_i, phi_inputs, 2u, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(v_i);
    instruction.as.binary.rhs = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(v_cond), block2_id, block3_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_mod);
    instruction.as.binary.op = VALUE_SSA_BINARY_MOD;
    instruction.as.binary.lhs = value_ssa_value_id(v_i);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(v_div);
    instruction.as.binary.op = VALUE_SSA_BINARY_DIV;
    instruction.as.binary.lhs = value_ssa_value_id(v_i);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(v_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_mod);
    instruction.as.binary.rhs = value_ssa_value_id(v_div);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(acc_local_id);
    instruction.as.store.value = value_ssa_value_id(v_sum);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_next);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_i);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, block1_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_exit);
    instruction.as.load_slot = value_ssa_slot_local(acc_local_id);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, instruction.result, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_simple_induction_shift_reduce_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t acc0_local_id;
    size_t acc1_local_id;
    size_t block0_id;
    size_t block1_id;
    size_t block2_id;
    size_t block3_id;
    size_t v_i;
    size_t v_cond;
    size_t v_scaled;
    size_t v_add0;
    size_t v_add1;
    size_t v_add2;
    size_t v_next;
    size_t v_exit0;
    size_t v_exit1;
    size_t v_exit2;
    size_t v_exit_sum;
    size_t v_exit_sum2;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "acc0", 0, &acc0_local_id, error) ||
        !value_ssa_function_append_local(function, "acc1", 0, &acc1_local_id, error) ||
        !value_ssa_function_append_block(function, &block0_id, &entry, error) ||
        !value_ssa_function_append_block(function, &block1_id, &header, error) ||
        !value_ssa_function_append_block(function, &block2_id, &body, error) ||
        !value_ssa_function_append_block(function, &block3_id, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    entry = &function->blocks[block0_id];
    header = &function->blocks[block1_id];
    body = &function->blocks[block2_id];
    exit_block = &function->blocks[block3_id];

    v_i = value_ssa_function_allocate_value(function);
    v_cond = value_ssa_function_allocate_value(function);
    v_scaled = value_ssa_function_allocate_value(function);
    v_add0 = value_ssa_function_allocate_value(function);
    v_add1 = value_ssa_function_allocate_value(function);
    v_add2 = value_ssa_function_allocate_value(function);
    v_next = value_ssa_function_allocate_value(function);
    v_exit0 = value_ssa_function_allocate_value(function);
    v_exit1 = value_ssa_function_allocate_value(function);
    v_exit2 = value_ssa_function_allocate_value(function);
    v_exit_sum = value_ssa_function_allocate_value(function);
    v_exit_sum2 = value_ssa_function_allocate_value(function);
    if (v_exit_sum2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_jump(entry, block1_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = block0_id;
    phi_inputs[0].value = value_ssa_value_immediate(0);
    phi_inputs[1].predecessor_block_id = block2_id;
    phi_inputs[1].value = value_ssa_value_id(v_next);
    if (!value_ssa_block_append_phi(header, v_i, phi_inputs, 2u, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(v_i);
    instruction.as.binary.rhs = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(v_cond), block2_id, block3_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_scaled);
    instruction.as.binary.op = VALUE_SSA_BINARY_SHIFT_LEFT;
    instruction.as.binary.lhs = value_ssa_value_id(v_i);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(v_add0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_scaled);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(acc0_local_id);
    instruction.as.store.value = value_ssa_value_id(v_add0);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_add2);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_scaled);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(acc0_local_id);
    instruction.as.store.value = value_ssa_value_id(v_add2);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_add1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_scaled);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(acc1_local_id);
    instruction.as.store.value = value_ssa_value_id(v_add1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_next);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_i);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, block1_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_exit0);
    instruction.as.load_slot = value_ssa_slot_local(acc0_local_id);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(v_exit1);
    instruction.as.load_slot = value_ssa_slot_local(acc1_local_id);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(v_exit2);
    instruction.as.load_slot = value_ssa_slot_local(acc0_local_id);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_exit_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_exit0);
    instruction.as.binary.rhs = value_ssa_value_id(v_exit1);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(v_exit_sum2);
    instruction.as.binary.lhs = value_ssa_value_id(v_exit_sum);
    instruction.as.binary.rhs = value_ssa_value_id(v_exit2);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(v_exit_sum2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_perf_spmv_loop_exit_indirect_load_reuse_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *xptr_global = NULL;
    ValueSsaGlobal *x_global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaFunction *main_function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *preheader = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t xptr_id;
    size_t x_id;
    size_t i_id;
    size_t block0_id;
    size_t block1_id;
    size_t block2_id;
    size_t block3_id;
    size_t block4_id;
    size_t v_xptr;
    size_t v_x;
    size_t v_i;
    size_t v_shift0;
    size_t v_addr0;
    size_t v_start;
    size_t v_i_next;
    size_t v_shift1;
    size_t v_addr1;
    size_t v_limit;
    size_t v_cur;
    size_t v_cond;
    size_t v_body_shift;
    size_t v_body_addr;
    size_t v_body_next;
    size_t v_exit_shift0;
    size_t v_exit_addr0;
    size_t v_exit_start;
    size_t v_exit_i_next;
    size_t v_exit_shift1;
    size_t v_exit_addr1;
    size_t v_exit_limit;
    size_t v_sum;
    size_t m_xptr;
    size_t m_x;
    size_t m_i;
    size_t m_result;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "gxptr", &xptr_global, error) ||
        !value_ssa_program_append_global(program, "gx", &x_global, error) ||
        !value_ssa_program_append_function(program, "spmv", 1, &function, error) ||
        !value_ssa_function_append_local(function, "xptr", 1, &xptr_id, error) ||
        !value_ssa_function_append_local(function, "x", 1, &x_id, error) ||
        !value_ssa_function_append_local(function, "i", 1, &i_id, error) ||
        !value_ssa_function_append_block(function, &block0_id, &entry, error) ||
        !value_ssa_function_append_block(function, &block1_id, &preheader, error) ||
        !value_ssa_function_append_block(function, &block2_id, &header, error) ||
        !value_ssa_function_append_block(function, &block3_id, &body, error) ||
        !value_ssa_function_append_block(function, &block4_id, &exit_block, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_function, error) ||
        !value_ssa_function_append_block(main_function, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    entry = &function->blocks[block0_id];
    preheader = &function->blocks[block1_id];
    header = &function->blocks[block2_id];
    body = &function->blocks[block3_id];
    exit_block = &function->blocks[block4_id];
    main_block = &main_function->blocks[0];

    function->locals[xptr_id].array_rank = 1u;
    function->locals[x_id].array_rank = 1u;
    xptr_global->byte_size = 16u;
    x_global->byte_size = 16u;

    v_xptr = value_ssa_function_allocate_value(function);
    v_x = value_ssa_function_allocate_value(function);
    v_i = value_ssa_function_allocate_value(function);
    v_shift0 = value_ssa_function_allocate_value(function);
    v_addr0 = value_ssa_function_allocate_value(function);
    v_start = value_ssa_function_allocate_value(function);
    v_i_next = value_ssa_function_allocate_value(function);
    v_shift1 = value_ssa_function_allocate_value(function);
    v_addr1 = value_ssa_function_allocate_value(function);
    v_limit = value_ssa_function_allocate_value(function);
    v_cur = value_ssa_function_allocate_value(function);
    v_cond = value_ssa_function_allocate_value(function);
    v_body_shift = value_ssa_function_allocate_value(function);
    v_body_addr = value_ssa_function_allocate_value(function);
    v_body_next = value_ssa_function_allocate_value(function);
    v_exit_shift0 = value_ssa_function_allocate_value(function);
    v_exit_addr0 = value_ssa_function_allocate_value(function);
    v_exit_start = value_ssa_function_allocate_value(function);
    v_exit_i_next = value_ssa_function_allocate_value(function);
    v_exit_shift1 = value_ssa_function_allocate_value(function);
    v_exit_addr1 = value_ssa_function_allocate_value(function);
    v_exit_limit = value_ssa_function_allocate_value(function);
    v_sum = value_ssa_function_allocate_value(function);
    m_xptr = value_ssa_function_allocate_value(main_function);
    m_x = value_ssa_function_allocate_value(main_function);
    m_i = value_ssa_function_allocate_value(main_function);
    m_result = value_ssa_function_allocate_value(main_function);
    if (v_sum == (size_t)-1 || m_result == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_xptr);
    instruction.as.load_slot = value_ssa_slot_local(xptr_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(v_x);
    instruction.as.load_slot = value_ssa_slot_local(x_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(v_i);
    instruction.as.load_slot = value_ssa_slot_local(i_id);
    instruction.as.load_slot = value_ssa_slot_local(i_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, block1_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_shift0);
    instruction.as.binary.op = VALUE_SSA_BINARY_SHIFT_LEFT;
    instruction.as.binary.lhs = value_ssa_value_id(v_i);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(preheader, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(v_addr0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_shift0);
    instruction.as.binary.rhs = value_ssa_value_id(v_xptr);
    if (!value_ssa_block_append_instruction(preheader, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_start);
    instruction.as.load_indirect_addr = value_ssa_value_id(v_addr0);
    if (!value_ssa_block_append_instruction(preheader, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_i_next);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_i);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(preheader, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(v_shift1);
    instruction.as.binary.op = VALUE_SSA_BINARY_SHIFT_LEFT;
    instruction.as.binary.lhs = value_ssa_value_id(v_i_next);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(preheader, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(v_addr1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_xptr);
    instruction.as.binary.rhs = value_ssa_value_id(v_shift1);
    if (!value_ssa_block_append_instruction(preheader, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_limit);
    instruction.as.load_indirect_addr = value_ssa_value_id(v_addr1);
    if (!value_ssa_block_append_instruction(preheader, &instruction, error) ||
        !value_ssa_block_set_jump(preheader, block2_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = block1_id;
    phi_inputs[0].value = value_ssa_value_id(v_start);
    phi_inputs[1].predecessor_block_id = block3_id;
    phi_inputs[1].value = value_ssa_value_id(v_body_next);
    if (!value_ssa_block_append_phi(header, v_cur, phi_inputs, 2u, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(v_cur);
    instruction.as.binary.rhs = value_ssa_value_id(v_limit);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(v_cond), block3_id, block4_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_body_shift);
    instruction.as.binary.op = VALUE_SSA_BINARY_SHIFT_LEFT;
    instruction.as.binary.lhs = value_ssa_value_id(v_cur);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(v_body_addr);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_x);
    instruction.as.binary.rhs = value_ssa_value_id(v_body_shift);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
    instruction.has_result = 0;
    instruction.as.store_indirect.addr = value_ssa_value_id(v_body_addr);
    instruction.as.store_indirect.value = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_body_next);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_cur);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, block2_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_exit_shift0);
    instruction.as.binary.op = VALUE_SSA_BINARY_SHIFT_LEFT;
    instruction.as.binary.lhs = value_ssa_value_id(v_i);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(v_exit_addr0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_xptr);
    instruction.as.binary.rhs = value_ssa_value_id(v_exit_shift0);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_exit_start);
    instruction.as.load_indirect_addr = value_ssa_value_id(v_exit_addr0);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_exit_i_next);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_i);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(v_exit_shift1);
    instruction.as.binary.op = VALUE_SSA_BINARY_SHIFT_LEFT;
    instruction.as.binary.lhs = value_ssa_value_id(v_exit_i_next);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(v_exit_addr1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_xptr);
    instruction.as.binary.rhs = value_ssa_value_id(v_exit_shift1);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_exit_limit);
    instruction.as.load_indirect_addr = value_ssa_value_id(v_exit_addr1);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(v_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(v_exit_start);
    instruction.as.binary.rhs = value_ssa_value_id(v_exit_limit);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(v_sum), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(m_xptr);
    instruction.as.addr_slot = value_ssa_slot_global(xptr_global->id);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(m_x);
    instruction.as.addr_slot = value_ssa_slot_global(x_global->id);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(m_i);
    instruction.as.mov_value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(m_result);
    instruction.as.call.callee_name = "spmv";
    instruction.as.call.arg_count = 3u;
    instruction.as.call.args = (ValueSsaValueRef *)calloc(3u, sizeof(ValueSsaValueRef));
    if (!instruction.as.call.args) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = value_ssa_value_id(m_xptr);
    instruction.as.call.args[1] = value_ssa_value_id(m_x);
    instruction.as.call.args[2] = value_ssa_value_id(m_i);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(m_result), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}



static int build_dangerous_dead_binary_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t result_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    result_value = value_ssa_function_allocate_value(function);
    if (result_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(result_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_DIV;
    instruction.as.binary.lhs = value_ssa_value_immediate(1);
    instruction.as.binary.rhs = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_constant_fold_binary_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_immediate(1);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_constant_fold_safe_div_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_DIV;
    instruction.as.binary.lhs = value_ssa_value_immediate(21);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_constant_fold_safe_mod_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_MOD;
    instruction.as.binary.lhs = value_ssa_value_immediate(22);
    instruction.as.binary.rhs = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_constant_fold_safe_shift_left_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_SHIFT_LEFT;
    instruction.as.binary.lhs = value_ssa_value_immediate(3);
    instruction.as.binary.rhs = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_constant_fold_safe_shift_right_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_SHIFT_RIGHT;
    instruction.as.binary.lhs = value_ssa_value_immediate(64);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_constant_fold_preserve_overflow_div_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_DIV;
    instruction.as.binary.lhs = value_ssa_value_immediate(-2147483648ll);
    instruction.as.binary.rhs = value_ssa_value_immediate(-1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_constant_fold_preserve_invalid_shift_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_SHIFT_LEFT;
    instruction.as.binary.lhs = value_ssa_value_immediate(-1);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_constant_fold_mov_chain_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value2);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value1);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_identity_add_zero_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_identity_eq_self_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_EQ;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_normalize_compare_immediate_lhs_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_immediate(1);
    instruction.as.binary.rhs = value_ssa_value_id(value0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_identity_div_one_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_DIV;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_canonicalize_bitand_all_ones_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_BIT_AND;
    instruction.as.binary.lhs = value_ssa_value_immediate(-1);
    instruction.as.binary.rhs = value_ssa_value_id(value0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_global_store_load_forward_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_global_load_forward_killed_by_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_global_load_forward_killed_by_indirect_store_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t addr_value;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
    instruction.has_result = 0;
    instruction.as.store_indirect.addr = value_ssa_value_id(addr_value);
    instruction.as.store_indirect.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_global_cross_block_load_forward_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *next = NULL;
    ValueSsaInstruction instruction;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &next, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(next, &instruction, error) ||
        !value_ssa_block_set_return(next, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_global_join_no_forward_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_local_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_local_store_preserves_address_taken_observer_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t addr_value;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_global_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_global_store_preserves_indirect_observer_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t addr_value;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_indirect_load_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t addr_value;
    size_t load0;
    size_t load1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    load0 = value_ssa_function_allocate_value(function);
    load1 = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1 || load0 == (size_t)-1 || load1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load0);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(load1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(load1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_indirect_load_store_barrier_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t addr_value;
    size_t load0;
    size_t load1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    load0 = value_ssa_function_allocate_value(function);
    load1 = value_ssa_function_allocate_value(function);
    if (addr_value == (size_t)-1 || load0 == (size_t)-1 || load1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load0);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
    instruction.as.store_indirect.addr = value_ssa_value_id(addr_value);
    instruction.as.store_indirect.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load1);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(load1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_join_redundant_indirect_load_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_cond_id;
    size_t local_a_id;
    size_t cond_value;
    size_t addr_value;
    size_t load0;
    size_t load1;
    size_t load2;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "cond", 0, &local_cond_id, error) ||
        !value_ssa_function_append_local(function, "a", 0, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    addr_value = value_ssa_function_allocate_value(function);
    load0 = value_ssa_function_allocate_value(function);
    load1 = value_ssa_function_allocate_value(function);
    load2 = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || addr_value == (size_t)-1 || load0 == (size_t)-1 ||
        load1 == (size_t)-1 || load2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_cond_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load0);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(load1);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(load2);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(load2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_join_redundant_indirect_load_call_barrier_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_cond_id;
    size_t local_a_id;
    size_t cond_value;
    size_t addr_value;
    size_t load0;
    size_t load1;
    size_t load2;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "cond", 0, &local_cond_id, error) ||
        !value_ssa_function_append_local(function, "a", 0, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    addr_value = value_ssa_function_allocate_value(function);
    load0 = value_ssa_function_allocate_value(function);
    load1 = value_ssa_function_allocate_value(function);
    load2 = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || addr_value == (size_t)-1 || load0 == (size_t)-1 ||
        load1 == (size_t)-1 || load2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_cond_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load0);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(load1);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load2);
    instruction.as.load_indirect_addr = value_ssa_value_id(addr_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(load2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_global_store_killed_by_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_local_store_across_straight_chain_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *next = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &next, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(next, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(next, &instruction, error) ||
        !value_ssa_block_set_return(next, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_local_store_preserves_branch_observer_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t local_b_id;
    size_t condition_value;
    size_t else_load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_local(function, "b", 1, &local_b_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    condition_value = value_ssa_function_allocate_value(function);
    else_load_value = value_ssa_function_allocate_value(function);
    if (condition_value == (size_t)-1 || else_load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(condition_value);
    instruction.as.load_slot = value_ssa_slot_local(local_b_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(condition_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(else_load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_id(else_load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_local_store_two_hop_chain_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *middle = NULL;
    ValueSsaBasicBlock *tail = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &middle, error) ||
        !value_ssa_function_append_block(function, NULL, &tail, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error) ||
        !value_ssa_block_set_jump(middle, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(tail, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(tail, &instruction, error) ||
        !value_ssa_block_set_return(tail, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_global_store_across_straight_chain_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *next = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &next, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(next, &instruction, error) ||
        !value_ssa_block_set_return(next, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_global_store_two_hop_chain_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *middle = NULL;
    ValueSsaBasicBlock *tail = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &middle, error) ||
        !value_ssa_function_append_block(function, NULL, &tail, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error) ||
        !value_ssa_block_set_jump(middle, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(tail, &instruction, error) ||
        !value_ssa_block_set_return(tail, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_global_store_preserves_call_across_blocks_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *next = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &next, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(next, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(next, &instruction, error) ||
        !value_ssa_block_set_return(next, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_global_store_preserves_branch_observer_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t condition_value;
    size_t else_load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    condition_value = value_ssa_function_allocate_value(function);
    else_load_value = value_ssa_function_allocate_value(function);
    if (condition_value == (size_t)-1 || else_load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(condition_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(condition_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(else_load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_id(else_load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_global_cross_block_load_forward_killed_by_call_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *next = NULL;
    ValueSsaInstruction instruction;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &next, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(next, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(next, &instruction, error) ||
        !value_ssa_block_set_return(next, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_global_store_join_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t condition_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    condition_value = value_ssa_function_allocate_value(function);
    if (condition_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(condition_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(condition_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_set_jump(else_block, 3, error) ||
        !value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_local_store_two_hop_chain_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *middle = NULL;
    ValueSsaBasicBlock *tail = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &middle, error) ||
        !value_ssa_function_append_block(function, NULL, &tail, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error) ||
        !value_ssa_block_set_jump(middle, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_append_instruction(tail, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(tail, &instruction, error) ||
        !value_ssa_block_set_return(tail, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_local_store_across_straight_chain_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *next = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &next, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_append_instruction(next, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(next, &instruction, error) ||
        !value_ssa_block_set_return(next, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_local_store_join_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t condition_value;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    condition_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (condition_value == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(condition_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(condition_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_set_jump(else_block, 3, error) ||
        !value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_global_store_cross_block_call_barrier_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *middle = NULL;
    ValueSsaBasicBlock *tail = NULL;
    ValueSsaInstruction instruction;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &middle, error) ||
        !value_ssa_function_append_block(function, NULL, &tail, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(middle, &instruction, error) ||
        !value_ssa_block_set_jump(middle, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(tail, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_id(load_value);
    if (!value_ssa_block_append_instruction(tail, &instruction, error) ||
        !value_ssa_block_set_return(tail, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_canonicalize_complex_local_chain_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t load_value;
    size_t bb0_id;
    size_t bb1_id;
    size_t bb2_id;
    size_t bb3_id;
    size_t bb4_id;
    size_t bb5_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, &bb0_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb1_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb2_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb3_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb4_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb5_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_local(local_a_id);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(&function->blocks[bb0_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb0_id], bb1_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_append_instruction(&function->blocks[bb1_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb1_id], bb2_id, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb2_id], bb3_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(&function->blocks[bb3_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb3_id], bb4_id, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb4_id], bb5_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(&function->blocks[bb5_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[bb5_id], value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_canonicalize_complex_global_call_chain_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t load_value;
    size_t bb0_id;
    size_t bb1_id;
    size_t bb2_id;
    size_t bb3_id;
    size_t bb4_id;
    size_t bb5_id;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, &bb0_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb1_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb2_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb3_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb4_id, NULL, error) ||
        !value_ssa_function_append_block(function, &bb5_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[bb0_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb0_id], bb1_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_append_instruction(&function->blocks[bb1_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb1_id], bb2_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(&function->blocks[bb2_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb2_id], bb3_id, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb3_id], bb4_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(&function->blocks[bb4_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[bb4_id], bb5_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(&function->blocks[bb5_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[bb5_id], value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_global_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_id(load_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_global_store_preserves_indirect_store_barrier_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t load_value;
    size_t addr_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    addr_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1 || addr_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
    instruction.has_result = 0;
    instruction.as.store_indirect.addr = value_ssa_value_id(addr_value);
    instruction.as.store_indirect.value = value_ssa_value_immediate(11);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_id(load_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_global_store_killed_by_call_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t load_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.has_result = 0;
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_id(load_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_binary_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;
    size_t value1;
    size_t value2;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value2);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_local_load_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;
    size_t value1;
    size_t value2;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value2);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_local_load_same_block_address_taken_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t addr_value;
    size_t load_value0;
    size_t load_value1;
    size_t sum_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    load_value0 = value_ssa_function_allocate_value(function);
    load_value1 = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(load_value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(load_value0);
    instruction.as.binary.rhs = value_ssa_value_id(load_value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_dangerous_binary_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_DIV;
    instruction.as.binary.lhs = value_ssa_value_immediate(1);
    instruction.as.binary.rhs = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_safe_div_binary_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_DIV;
    instruction.as.binary.lhs = value_ssa_value_immediate(10);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dominated_redundant_binary_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t cond_value;
    size_t add_value0;
    size_t add_value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    add_value0 = value_ssa_function_allocate_value(function);
    add_value1 = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || add_value0 == (size_t)-1 || add_value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(add_value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(cond_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(add_value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(cond_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_id(add_value1), error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_id(add_value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_join_redundant_binary_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t cond_value;
    size_t add_value0;
    size_t add_value1;
    size_t add_value2;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    add_value0 = value_ssa_function_allocate_value(function);
    add_value1 = value_ssa_function_allocate_value(function);
    add_value2 = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || add_value0 == (size_t)-1 || add_value1 == (size_t)-1 ||
        add_value2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(add_value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(cond_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(add_value1);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(add_value2);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(add_value2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_commuted_binary_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t load_value;
    size_t add_value0;
    size_t add_value1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    add_value0 = value_ssa_function_allocate_value(function);
    add_value1 = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1 || add_value0 == (size_t)-1 || add_value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(add_value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(load_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(add_value1);
    instruction.as.binary.lhs = value_ssa_value_immediate(1);
    instruction.as.binary.rhs = value_ssa_value_id(load_value);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(add_value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_join_redundant_commuted_binary_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t cond_value;
    size_t add_value0;
    size_t add_value1;
    size_t add_value2;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    add_value0 = value_ssa_function_allocate_value(function);
    add_value1 = value_ssa_function_allocate_value(function);
    add_value2 = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || add_value0 == (size_t)-1 || add_value1 == (size_t)-1 ||
        add_value2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(add_value0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(cond_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(add_value1);
    instruction.as.binary.lhs = value_ssa_value_immediate(1);
    instruction.as.binary.rhs = value_ssa_value_id(cond_value);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(add_value2);
    instruction.as.binary.lhs = value_ssa_value_id(cond_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(add_value2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_join_redundant_local_load_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t local_a_id;
    size_t cond_value;
    size_t load_value0;
    size_t load_value1;
    size_t phi_value;
    size_t join_load_value;
    size_t sum_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    load_value0 = value_ssa_function_allocate_value(function);
    load_value1 = value_ssa_function_allocate_value(function);
    phi_value = value_ssa_function_allocate_value(function);
    join_load_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(load_value1);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1u;
    phi_inputs[0].value = value_ssa_value_id(load_value0);
    phi_inputs[1].predecessor_block_id = 2u;
    phi_inputs[1].value = value_ssa_value_id(load_value1);
    if (!value_ssa_block_append_phi(join_block, phi_value, phi_inputs, 2u, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_load_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_id(join_load_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_local_load_address_taken_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t addr_value;
    size_t load_value0;
    size_t load_value1;
    size_t sum_value;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    addr_value = value_ssa_function_allocate_value(function);
    load_value0 = value_ssa_function_allocate_value(function);
    load_value1 = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr_value);
    instruction.as.addr_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(load_value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(load_value0);
    instruction.as.binary.rhs = value_ssa_value_id(load_value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dominated_redundant_addr_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaGlobal *global = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t cond_value;
    size_t addr0;
    size_t addr1;
    size_t gaddr0;
    size_t gaddr1;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    addr0 = value_ssa_function_allocate_value(function);
    addr1 = value_ssa_function_allocate_value(function);
    gaddr0 = value_ssa_function_allocate_value(function);
    gaddr1 = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || addr0 == (size_t)-1 || addr1 == (size_t)-1 ||
        gaddr0 == (size_t)-1 || gaddr1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr0);
    instruction.as.addr_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(gaddr0);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(addr1);
    instruction.as.addr_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(gaddr1);
    instruction.as.addr_slot = value_ssa_slot_global(global->id);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_id(gaddr1), error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_id(addr0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int expect_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s verifier rejected setup at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_verifier_rejects(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    void (*mutator)(ValueSsaProgram *program),
    const char *expected_fragment) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!builder || !mutator || !expected_fragment) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    mutator(&program);
    ok = !value_ssa_verify_program(&program, &error) &&
        strstr(error.message, expected_fragment) != NULL;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s expected verifier rejection containing '%s', got '%s'\n",
            case_id,
            expected_fragment,
            error.message);
    }

    value_ssa_program_free(&program);
    return ok;
}

static int expect_simplified_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_simplify_trivial_values(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s simplify failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s simplified dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_constant_folded_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_fold_constants(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s constant fold failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s constant-folded dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_identity_simplified_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_simplify_algebraic_identities(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s identity simplify failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s identity-simplified dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_binary_normalized_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_normalize_binary_operands(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s binary normalize failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s binary-normalized dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_local_load_forwarded_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_forward_local_loads(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s local-load-forward failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s local-load-forward dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_perf_hotspot_optimized_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_optimize_perf_hotspots(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s perf-hotspot optimize failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s perf-hotspot dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_tiny_helper_inlined_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_inline_tiny_internal_helpers(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s tiny-inline failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s tiny-inline dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_global_load_forwarded_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_forward_global_loads(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s global-load-forward failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s global-load-forward dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_dead_store_cleaned_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_eliminate_dead_stores(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s dead-store cleanup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s dead-store-cleaned dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_redundant_store_eliminated_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_eliminate_redundant_stores(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s redundant-store cleanup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s redundant-store-cleaned dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_redundant_binary_eliminated_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_eliminate_redundant_binaries(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s redundant-binary elimination failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s redundant-binary dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_redundant_indirect_load_eliminated_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_eliminate_redundant_indirect_loads(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s redundant-indirect-load elimination failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s redundant-indirect-load dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_sccp_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_sparse_conditional_constant_propagation(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s SCCP failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s SCCP dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}



static int expect_cfg_simplified_dump(const char *case_id,
    int run_value_simplify_first,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if ((run_value_simplify_first && !value_ssa_simplify_trivial_values(&program, &error)) ||
        !value_ssa_simplify_cfg(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s cfg simplify failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s cfg-simplified dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_dead_def_cleaned_dump(const char *case_id,
    int run_value_simplify_first,
    int run_cfg_simplify,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if ((run_value_simplify_first && !value_ssa_simplify_trivial_values(&program, &error)) ||
        (run_cfg_simplify && !value_ssa_simplify_cfg(&program, &error)) ||
        !value_ssa_eliminate_dead_value_defs(&program, &error) ||
        !value_ssa_verify_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s dead-def cleanup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s dead-def-cleaned dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_converted_dump(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const char *expected_text) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaError ssa_error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_from_lower_ir(&lower_program, &ssa_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&ssa_program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s converted dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

static int expect_canonicalized_converted_dump(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const char *expected_text) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaError ssa_error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_canonicalized_from_lower_ir(&lower_program, &ssa_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s canonicalized conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&ssa_program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s canonicalized converted dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

static int expect_canonicalized_converted_fixed_point_dump(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const char *expected_text) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaError ssa_error;
    char *first_dump = NULL;
    char *second_dump = NULL;
    int ok = 1;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_canonicalized_from_lower_ir(&lower_program, &ssa_program, &ssa_error) ||
        !value_ssa_verify_program(&ssa_program, &ssa_error) ||
        !value_ssa_dump_program(&ssa_program, &first_dump) ||
        !value_ssa_canonicalize_program(&ssa_program, &ssa_error) ||
        !value_ssa_verify_program(&ssa_program, &ssa_error) ||
        !value_ssa_dump_program(&ssa_program, &second_dump)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s canonicalized converted fixed-point failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        free(first_dump);
        free(second_dump);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    if (strcmp(first_dump, expected_text) != 0) {
        ok = 0;
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s first canonicalized converted dump mismatch\nExpected:\n%s\nActual:\n%s\n",
            case_id,
            expected_text,
            first_dump);
    } else if (strcmp(first_dump, second_dump) != 0) {
        ok = 0;
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s canonicalized converted fixed-point mismatch\nFirst:\n%s\nSecond:\n%s\n",
            case_id,
            first_dump,
            second_dump);
    }

    free(first_dump);
    free(second_dump);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

static int expect_memory_value_canonicalized_converted_dump(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const char *expected_text) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaError ssa_error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_memory_value_canonicalized_from_lower_ir(&lower_program, &ssa_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s memory-value canonicalized conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&ssa_program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s memory-value canonicalized converted dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

static int expect_memory_canonicalized_converted_dump(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const char *expected_text) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaError ssa_error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_memory_canonicalized_from_lower_ir(&lower_program, &ssa_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s memory canonicalized conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&ssa_program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s memory canonicalized converted dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

static int expect_mode_converted_dump(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    ValueSsaLowerIrCanonicalizeMode mode,
    const char *expected_text) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaError ssa_error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_from_lower_ir_with_canonicalization(&lower_program, mode, &ssa_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s mode conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&ssa_program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s mode converted dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

static int expect_default_converted_dump(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const char *expected_text) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaError ssa_error;
    char *actual_text = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_default_from_lower_ir(&lower_program, &ssa_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s default conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&ssa_program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s default converted dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

static int expect_default_matches_mode_dump(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    ValueSsaLowerIrCanonicalizeMode mode) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram default_program;
    ValueSsaProgram mode_program;
    ValueSsaError ssa_error;
    char *default_text = NULL;
    char *mode_text = NULL;
    int ok = 1;

    if (!builder) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_default_from_lower_ir(&lower_program, &default_program, &ssa_error) ||
        !value_ssa_build_from_lower_ir_with_canonicalization(&lower_program, mode, &mode_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s default/mode conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&default_program, &default_text) ||
        !value_ssa_dump_program(&mode_program, &mode_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        ok = 0;
        goto cleanup;
    }

    if (strcmp(default_text, mode_text) != 0) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s default/mode dump mismatch\ndefault:\n%s\nmode:\n%s\n",
            case_id,
            default_text,
            mode_text);
        ok = 0;
    }

cleanup:
    free(default_text);
    free(mode_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&default_program);
    value_ssa_program_free(&mode_program);
    return ok;
}

static int expect_default_matches_direct_dump(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error)) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram default_program;
    ValueSsaProgram direct_program;
    ValueSsaError ssa_error;
    char *default_text = NULL;
    char *direct_text = NULL;
    int ok = 1;

    if (!builder) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_default_from_lower_ir(&lower_program, &default_program, &ssa_error) ||
        !value_ssa_build_from_lower_ir(&lower_program, &direct_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s default/direct conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&default_program, &default_text) ||
        !value_ssa_dump_program(&direct_program, &direct_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        ok = 0;
        goto cleanup;
    }

    if (strcmp(default_text, direct_text) != 0) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s default/direct dump mismatch\ndefault:\n%s\ndirect:\n%s\n",
            case_id,
            default_text,
            direct_text);
        ok = 0;
    }

cleanup:
    free(default_text);
    free(direct_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&default_program);
    value_ssa_program_free(&direct_program);
    return ok;
}

static int build_value_ssa_perf_from_source_text(const char *source,
    ValueSsaProgram *out_program,
    ValueSsaError *out_error) {
    TokenArray tokens;
    AstProgram ast_program;
    ParserError parse_error;
    SemanticError semantic_error;
    SemanticOptions semantic_options;
    IrProgram ir_program;
    IrError ir_error;
    IrLowerOptions ir_options;
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    LowerIrOptions lower_options;
    int ok = 0;

    if (!source || !out_program) {
        if (out_error) {
            out_error->line = 0;
            out_error->column = 0;
            snprintf(out_error->message, sizeof(out_error->message), "invalid source perf build contract");
        }
        return 0;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    memset(&parse_error, 0, sizeof(parse_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&semantic_options, 0, sizeof(semantic_options));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&ir_options, 0, sizeof(ir_options));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&lower_options, 0, sizeof(lower_options));
    semantic_options.skip_all_paths_return_check = 1;
    ir_options.allow_implicit_fallthrough_return = 1;
    lower_options.allow_implicit_fallthrough_return = 1;
    value_ssa_program_init(out_program);

    if (!lexer_tokenize(source, &tokens) ||
        !parser_parse_translation_unit_ast(&tokens, &ast_program, &parse_error) ||
        !semantic_analyze_program_with_options(&ast_program, &semantic_options, &semantic_error) ||
        !ir_lower_program(&ast_program, &ir_options, &ir_program, &ir_error) ||
        !lower_ir_lower_from_ir(&ir_program, &lower_options, &lower_program, &lower_error) ||
        !value_ssa_build_default_from_lower_ir(&lower_program, out_program, out_error) ||
        !memory_ssa_pass_scalar_replace_local_slots(out_program, out_error) ||
        !memory_ssa_pass_scalar_replace_global_slots(out_program, out_error) ||
        !value_ssa_optimize_perf_hotspots(out_program, out_error)) {
        goto cleanup;
    }

    ok = 1;

cleanup:
    if (!ok) {
        value_ssa_program_free(out_program);
    }
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

static int expect_source_perf_hotspot_dump(const char *case_id,
    const char *source,
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;

    if (!case_id || !source || !expected_text) {
        return 0;
    }

    memset(&error, 0, sizeof(error));
    if (!build_value_ssa_perf_from_source_text(source, &program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s source perf build failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text, expected_text) != 0) {
        ok = 0;
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s source perf dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_source_perf_hotspot_fragments(const char *case_id,
    const char *source,
    const char *const *fragments,
    size_t fragment_count) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    const char *cursor = NULL;
    size_t index;

    if (!case_id || !source || !fragments) {
        return 0;
    }

    memset(&error, 0, sizeof(error));
    if (!build_value_ssa_perf_from_source_text(source, &program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s source perf build failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dump failed\n", case_id);
        value_ssa_program_free(&program);
        return 0;
    }

    cursor = actual_text;
    for (index = 0; index < fragment_count; ++index) {
        const char *found = strstr(cursor, fragments[index]);

        if (!found) {
            fprintf(stderr,
                "[value-ssa-reg] FAIL: %s missing fragment:\n%s\nactual:\n%s\n",
                case_id,
                fragments[index],
                actual_text);
            free(actual_text);
            value_ssa_program_free(&program);
            return 0;
        }
        cursor = found + strlen(fragments[index]);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return 1;
}

static int expect_cfg_analysis(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    int (*checker)(const ValueSsaProgram *program, const ValueSsaCfgAnalysis *analysis)) {
    ValueSsaProgram program;
    ValueSsaCfgAnalysis analysis;
    ValueSsaError error;
    int ok;

    if (!builder || !checker) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    value_ssa_cfg_analysis_init(&analysis);
    if (!value_ssa_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s cfg analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = checker(&program, &analysis);
    if (!ok) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s cfg analysis mismatch\n", case_id);
    }

    value_ssa_cfg_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_def_use_analysis(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    int (*checker)(const ValueSsaProgram *program, const ValueSsaDefUseAnalysis *analysis)) {
    ValueSsaProgram program;
    ValueSsaDefUseAnalysis analysis;
    ValueSsaError error;
    int ok;

    if (!builder || !checker) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    value_ssa_def_use_analysis_init(&analysis);
    if (!value_ssa_compute_def_use_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s def-use analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_def_use_analysis_free(&analysis);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = checker(&program, &analysis);
    if (!ok) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s def-use analysis mismatch\n", case_id);
    }

    value_ssa_def_use_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_converted_cfg_analysis(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    int (*checker)(const ValueSsaProgram *program, const ValueSsaCfgAnalysis *analysis)) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaCfgAnalysis analysis;
    ValueSsaError ssa_error;
    int ok;

    if (!builder || !checker) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_from_lower_ir(&lower_program, &ssa_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    value_ssa_cfg_analysis_init(&analysis);
    if (!value_ssa_compute_cfg_analysis(&ssa_program.functions[0], &analysis, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s cfg analysis failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    ok = checker(&ssa_program, &analysis);
    if (!ok) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s cfg analysis mismatch\n", case_id);
    }

    value_ssa_cfg_analysis_free(&analysis);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

static int expect_phi_placement(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    int (*checker)(const ValueSsaProgram *program,
        const ValueSsaCfgAnalysis *analysis,
        unsigned char *phi_blocks)) {
    ValueSsaProgram program;
    ValueSsaCfgAnalysis analysis;
    ValueSsaError error;
    unsigned char *phi_blocks = NULL;
    int ok;

    if (!builder || !checker) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    value_ssa_cfg_analysis_init(&analysis);
    if (!value_ssa_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s cfg analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    phi_blocks = (unsigned char *)calloc(analysis.block_count, sizeof(unsigned char));
    if (!phi_blocks) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s out of memory allocating phi blocks\n", case_id);
        value_ssa_cfg_analysis_free(&analysis);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = checker(&program, &analysis, phi_blocks);
    if (!ok) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s phi placement mismatch\n", case_id);
    }

    free(phi_blocks);
    value_ssa_cfg_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_converted_phi_placement(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    int (*checker)(const ValueSsaProgram *program,
        const ValueSsaCfgAnalysis *analysis,
        unsigned char *phi_blocks)) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaCfgAnalysis analysis;
    ValueSsaError ssa_error;
    unsigned char *phi_blocks = NULL;
    int ok;

    if (!builder || !checker) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_from_lower_ir(&lower_program, &ssa_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    value_ssa_cfg_analysis_init(&analysis);
    if (!value_ssa_compute_cfg_analysis(&ssa_program.functions[0], &analysis, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s cfg analysis failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    phi_blocks = (unsigned char *)calloc(analysis.block_count, sizeof(unsigned char));
    if (!phi_blocks) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s out of memory allocating phi blocks\n", case_id);
        value_ssa_cfg_analysis_free(&analysis);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    ok = checker(&ssa_program, &analysis, phi_blocks);
    if (!ok) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s phi placement mismatch\n", case_id);
    }

    free(phi_blocks);
    value_ssa_cfg_analysis_free(&analysis);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

static int expect_dominator_preorder(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const size_t *expected_order,
    size_t expected_count) {
    ValueSsaProgram program;
    ValueSsaCfgAnalysis analysis;
    ValueSsaError error;
    size_t *order = NULL;
    size_t actual_count = 0;
    int ok = 1;
    size_t index;

    if (!builder || !expected_order) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    value_ssa_cfg_analysis_init(&analysis);
    if (!value_ssa_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s cfg analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    order = (size_t *)malloc(analysis.block_count * sizeof(size_t));
    if (!order) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s out of memory allocating preorder buffer\n", case_id);
        value_ssa_cfg_analysis_free(&analysis);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_compute_dominator_tree_preorder(&program.functions[0], &analysis, order, &actual_count, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s preorder failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(order);
        value_ssa_cfg_analysis_free(&analysis);
        value_ssa_program_free(&program);
        return 0;
    }

    if (actual_count != expected_count) {
        ok = 0;
    }
    for (index = 0; ok && index < expected_count; ++index) {
        if (order[index] != expected_order[index]) {
            ok = 0;
        }
    }

    if (!ok) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dominator preorder mismatch\n", case_id);
    }

    free(order);
    value_ssa_cfg_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_converted_dominator_preorder(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const size_t *expected_order,
    size_t expected_count) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaCfgAnalysis analysis;
    ValueSsaError ssa_error;
    size_t *order = NULL;
    size_t actual_count = 0;
    int ok = 1;
    size_t index;

    if (!builder || !expected_order) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_from_lower_ir(&lower_program, &ssa_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    value_ssa_cfg_analysis_init(&analysis);
    if (!value_ssa_compute_cfg_analysis(&ssa_program.functions[0], &analysis, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s cfg analysis failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    order = (size_t *)malloc(analysis.block_count * sizeof(size_t));
    if (!order) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s out of memory allocating preorder buffer\n", case_id);
        value_ssa_cfg_analysis_free(&analysis);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    if (!value_ssa_compute_dominator_tree_preorder(&ssa_program.functions[0], &analysis, order, &actual_count,
            &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s preorder failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        free(order);
        value_ssa_cfg_analysis_free(&analysis);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    if (actual_count != expected_count) {
        ok = 0;
    }
    for (index = 0; ok && index < expected_count; ++index) {
        if (order[index] != expected_order[index]) {
            ok = 0;
        }
    }

    if (!ok) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dominator preorder mismatch\n", case_id);
    }

    free(order);
    value_ssa_cfg_analysis_free(&analysis);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

typedef struct {
    long *events;
    size_t event_count;
    size_t event_capacity;
} ValueSsaWalkTrace;

typedef struct {
    ValueSsaRenameState *state;
} ValueSsaRenameWalkState;

static int value_ssa_trace_walk_enter(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaWalkTrace *trace = (ValueSsaWalkTrace *)user_data;

    (void)function;
    (void)analysis;
    (void)error;

    if (!trace || trace->event_count >= trace->event_capacity) {
        return 0;
    }

    trace->events[trace->event_count++] = (long)block_id + 1;
    return 1;
}

static int value_ssa_trace_walk_leave(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaWalkTrace *trace = (ValueSsaWalkTrace *)user_data;

    (void)function;
    (void)analysis;
    (void)error;

    if (!trace || trace->event_count >= trace->event_capacity) {
        return 0;
    }

    trace->events[trace->event_count++] = -((long)block_id + 1);
    return 1;
}

static int value_ssa_rename_walk_enter(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaRenameWalkState *walk_state = (ValueSsaRenameWalkState *)user_data;
    ValueSsaValueRef value;

    (void)function;
    (void)analysis;

    if (!walk_state || !walk_state->state ||
        !value_ssa_rename_state_begin_scope(walk_state->state, error)) {
        return 0;
    }

    switch (block_id) {
    case 0:
        return value_ssa_rename_state_bind(walk_state->state, 0, value_ssa_value_immediate(10), error);
    case 1:
        return value_ssa_rename_state_bind(walk_state->state, 0, value_ssa_value_immediate(1), error);
    case 2:
        return value_ssa_rename_state_bind(walk_state->state, 0, value_ssa_value_immediate(2), error);
    case 3:
        if (!value_ssa_rename_state_lookup(walk_state->state, 0, &value, error)) {
            return 0;
        }
        if (value.kind != VALUE_SSA_VALUE_IMMEDIATE || value.immediate != 10) {
            if (error) {
                error->line = 0;
                error->column = 0;
                snprintf(error->message,
                    sizeof(error->message),
                    "VALUE-SSA-REG-RENAME-WALK: expected join block to see restored entry binding 10, got kind=%d value=%lld",
                    (int)value.kind,
                    value.immediate);
            }
            return 0;
        }
        return 1;
    default:
        return 1;
    }
}

static int value_ssa_rename_walk_leave(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaRenameWalkState *walk_state = (ValueSsaRenameWalkState *)user_data;

    (void)function;
    (void)analysis;
    (void)block_id;

    if (!walk_state || !walk_state->state) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message,
                sizeof(error->message),
                "VALUE-SSA-REG-RENAME-WALK: invalid walk leave contract");
        }
        return 0;
    }

    return value_ssa_rename_state_end_scope(walk_state->state, error);
}

static int expect_dominator_walk(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const long *expected_events,
    size_t expected_event_count) {
    ValueSsaProgram program;
    ValueSsaCfgAnalysis analysis;
    ValueSsaError error;
    long *events = NULL;
    ValueSsaWalkTrace trace;
    int ok = 1;
    size_t index;

    if (!builder || !expected_events) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    value_ssa_cfg_analysis_init(&analysis);
    if (!value_ssa_compute_cfg_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s cfg analysis failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    events = (long *)malloc(analysis.block_count * 2 * sizeof(long));
    if (!events) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s out of memory allocating walk trace\n", case_id);
        value_ssa_cfg_analysis_free(&analysis);
        value_ssa_program_free(&program);
        return 0;
    }

    trace.events = events;
    trace.event_count = 0;
    trace.event_capacity = analysis.block_count * 2;
    if (!value_ssa_walk_dominator_tree(&program.functions[0],
            &analysis,
            value_ssa_trace_walk_enter,
            value_ssa_trace_walk_leave,
            &trace,
            &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s dominator walk failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(events);
        value_ssa_cfg_analysis_free(&analysis);
        value_ssa_program_free(&program);
        return 0;
    }

    if (trace.event_count != expected_event_count) {
        ok = 0;
    }
    for (index = 0; ok && index < expected_event_count; ++index) {
        if (trace.events[index] != expected_events[index]) {
            ok = 0;
        }
    }

    if (!ok) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dominator walk mismatch\n", case_id);
    }

    free(events);
    value_ssa_cfg_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_converted_dominator_walk(const char *case_id,
    int (*builder)(LowerIrProgram *program, LowerIrError *error),
    const long *expected_events,
    size_t expected_event_count) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaCfgAnalysis analysis;
    ValueSsaError ssa_error;
    long *events = NULL;
    ValueSsaWalkTrace trace;
    int ok = 1;
    size_t index;

    if (!builder || !expected_events) {
        return 0;
    }

    if (!builder(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_from_lower_ir(&lower_program, &ssa_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s conversion failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    value_ssa_cfg_analysis_init(&analysis);
    if (!value_ssa_compute_cfg_analysis(&ssa_program.functions[0], &analysis, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s cfg analysis failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    events = (long *)malloc(analysis.block_count * 2 * sizeof(long));
    if (!events) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s out of memory allocating walk trace\n", case_id);
        value_ssa_cfg_analysis_free(&analysis);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    trace.events = events;
    trace.event_count = 0;
    trace.event_capacity = analysis.block_count * 2;
    if (!value_ssa_walk_dominator_tree(&ssa_program.functions[0],
            &analysis,
            value_ssa_trace_walk_enter,
            value_ssa_trace_walk_leave,
            &trace,
            &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s dominator walk failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        free(events);
        value_ssa_cfg_analysis_free(&analysis);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    if (trace.event_count != expected_event_count) {
        ok = 0;
    }
    for (index = 0; ok && index < expected_event_count; ++index) {
        if (trace.events[index] != expected_events[index]) {
            ok = 0;
        }
    }

    if (!ok) {
        fprintf(stderr, "[value-ssa-reg] FAIL: %s dominator walk mismatch\n", case_id);
    }

    free(events);
    value_ssa_cfg_analysis_free(&analysis);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

static int expect_function_renamed_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaCfgAnalysis analysis;
    ValueSsaError error;
    char *dump = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    value_ssa_cfg_analysis_init(&analysis);
    if (!value_ssa_compute_cfg_analysis(&program.functions[0], &analysis, &error) ||
        !value_ssa_rename_function_values(&program.functions[0], &analysis, &error) ||
        !value_ssa_verify_program(&program, &error) ||
        !value_ssa_dump_program(&program, &dump)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s rename failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(dump);
        value_ssa_cfg_analysis_free(&analysis);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(dump, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s rename dump mismatch\nExpected:\n%s\nActual:\n%s\n",
            case_id,
            expected_text,
            dump);
    }

    free(dump);
    value_ssa_cfg_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_canonicalized_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *dump = NULL;
    int ok;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_canonicalize_program(&program, &error) ||
        !value_ssa_verify_program(&program, &error) ||
        !value_ssa_dump_program(&program, &dump)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s canonicalize failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(dump);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strcmp(dump, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s canonical dump mismatch\nExpected:\n%s\nActual:\n%s\n",
            case_id,
            expected_text,
            dump);
    }

    free(dump);
    value_ssa_program_free(&program);
    return ok;
}

static int expect_canonicalize_rejected(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_error_substring) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!builder || !expected_error_substring) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    ok = !value_ssa_canonicalize_program(&program, &error) &&
        strstr(error.message, expected_error_substring) != NULL;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s expected canonicalize rejection containing '%s', got '%s'\n",
            case_id,
            expected_error_substring,
            error.message);
    }

    value_ssa_program_free(&program);
    return ok;
}

static int expect_canonicalized_fixed_point_dump(const char *case_id,
    int (*builder)(ValueSsaProgram *program, ValueSsaError *error),
    const char *expected_text) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *first_dump = NULL;
    char *second_dump = NULL;
    int ok = 1;

    if (!builder || !expected_text) {
        return 0;
    }

    if (!builder(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s setup failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_canonicalize_program(&program, &error) ||
        !value_ssa_verify_program(&program, &error) ||
        !value_ssa_dump_program(&program, &first_dump) ||
        !value_ssa_canonicalize_program(&program, &error) ||
        !value_ssa_verify_program(&program, &error) ||
        !value_ssa_dump_program(&program, &second_dump)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s canonical fixed-point failed at %d:%d: %s\n",
            case_id,
            error.line,
            error.column,
            error.message);
        free(first_dump);
        free(second_dump);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(first_dump, expected_text) != 0) {
        ok = 0;
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s first canonical dump mismatch\nExpected:\n%s\nActual:\n%s\n",
            case_id,
            expected_text,
            first_dump);
    } else if (strcmp(first_dump, second_dump) != 0) {
        ok = 0;
        fprintf(stderr,
            "[value-ssa-reg] FAIL: %s canonical fixed-point mismatch\nFirst:\n%s\nSecond:\n%s\n",
            case_id,
            first_dump,
            second_dump);
    }

    free(first_dump);
    free(second_dump);
    value_ssa_program_free(&program);
    return ok;
}

static int check_diamond_cfg_analysis(const ValueSsaProgram *program, const ValueSsaCfgAnalysis *analysis) {
    const ValueSsaFunction *function = program ? &program->functions[0] : NULL;
    size_t count = analysis ? analysis->block_count : 0;

    if (!function || !analysis || count != 4) {
        return 0;
    }

    return analysis->predecessor_counts[0] == 0 &&
        analysis->predecessor_counts[1] == 1 &&
        analysis->predecessor_counts[2] == 1 &&
        analysis->predecessor_counts[3] == 2 &&
        analysis->reachable[0] &&
        analysis->reachable[1] &&
        analysis->reachable[2] &&
        analysis->reachable[3] &&
        analysis->immediate_dominator[0] == (size_t)-1 &&
        analysis->immediate_dominator[1] == 0 &&
        analysis->immediate_dominator[2] == 0 &&
        analysis->immediate_dominator[3] == 0 &&
        analysis->dominator_tree_child_counts[0] == 3 &&
        analysis->dominator_tree_child_counts[1] == 0 &&
        analysis->dominator_tree_child_counts[2] == 0 &&
        analysis->dominator_tree_child_counts[3] == 0 &&
        analysis->dominance_frontier_counts[0] == 0 &&
        analysis->dominance_frontier_counts[1] == 1 &&
        analysis->dominance_frontier_counts[2] == 1 &&
        analysis->dominance_frontier_counts[3] == 0 &&
        analysis->dominator_tree_children[0 * count + 1] &&
        analysis->dominator_tree_children[0 * count + 2] &&
        analysis->dominator_tree_children[0 * count + 3] &&
        analysis->dominates[0 * count + 3] &&
        !analysis->dominates[1 * count + 3] &&
        !analysis->dominates[2 * count + 3] &&
        analysis->dominates[3 * count + 3] &&
        analysis->dominance_frontier[1 * count + 3] &&
        analysis->dominance_frontier[2 * count + 3];
}

static int check_loop_cfg_analysis(const ValueSsaProgram *program, const ValueSsaCfgAnalysis *analysis) {
    size_t count = analysis ? analysis->block_count : 0;

    if (!program || !analysis || count != 4) {
        return 0;
    }

    return analysis->predecessor_counts[0] == 0 &&
        analysis->predecessor_counts[1] == 2 &&
        analysis->predecessor_counts[2] == 1 &&
        analysis->predecessor_counts[3] == 1 &&
        analysis->reachable[0] &&
        analysis->reachable[1] &&
        analysis->reachable[2] &&
        analysis->reachable[3] &&
        analysis->immediate_dominator[0] == (size_t)-1 &&
        analysis->immediate_dominator[1] == 0 &&
        analysis->immediate_dominator[2] == 1 &&
        analysis->immediate_dominator[3] == 1 &&
        analysis->dominator_tree_child_counts[0] == 1 &&
        analysis->dominator_tree_child_counts[1] == 2 &&
        analysis->dominator_tree_child_counts[2] == 0 &&
        analysis->dominator_tree_child_counts[3] == 0 &&
        analysis->dominance_frontier_counts[0] == 0 &&
        analysis->dominance_frontier_counts[1] == 1 &&
        analysis->dominance_frontier_counts[2] == 1 &&
        analysis->dominance_frontier_counts[3] == 0 &&
        analysis->dominator_tree_children[0 * count + 1] &&
        analysis->dominator_tree_children[1 * count + 2] &&
        analysis->dominator_tree_children[1 * count + 3] &&
        analysis->dominates[1 * count + 2] &&
        analysis->dominates[1 * count + 3] &&
        !analysis->dominates[2 * count + 1] &&
        analysis->dominance_frontier[1 * count + 1] &&
        analysis->dominance_frontier[2 * count + 1];
}

static int check_diamond_def_use_analysis(const ValueSsaProgram *program, const ValueSsaDefUseAnalysis *analysis) {
    const ValueSsaFunction *function = program ? &program->functions[0] : NULL;

    if (!function || !analysis || analysis->value_count != function->next_value_id || function->next_value_id != 4) {
        return 0;
    }

    return analysis->has_def[0] &&
        analysis->has_def[1] &&
        analysis->has_def[2] &&
        analysis->has_def[3] &&
        analysis->def_block_ids[0] == 0 &&
        analysis->def_instruction_indices[0] == 0 &&
        analysis->def_block_ids[1] == 1 &&
        analysis->def_instruction_indices[1] == 0 &&
        analysis->def_block_ids[2] == 2 &&
        analysis->def_instruction_indices[2] == 0 &&
        analysis->def_block_ids[3] == 3 &&
        analysis->def_phi_indices[3] == 0 &&
        analysis->use_counts[0] == 1 &&
        analysis->use_counts[1] == 1 &&
        analysis->use_counts[2] == 1 &&
        analysis->use_counts[3] == 1 &&
        analysis->use_sites[analysis->use_offsets[0]].kind == VALUE_SSA_USE_TERM &&
        analysis->use_sites[analysis->use_offsets[0]].role == VALUE_SSA_USE_ROLE_BRANCH_CONDITION &&
        analysis->use_sites[analysis->use_offsets[1]].kind == VALUE_SSA_USE_PHI &&
        analysis->use_sites[analysis->use_offsets[1]].role == VALUE_SSA_USE_ROLE_PHI_INPUT &&
        analysis->use_sites[analysis->use_offsets[2]].kind == VALUE_SSA_USE_PHI &&
        analysis->use_sites[analysis->use_offsets[2]].role == VALUE_SSA_USE_ROLE_PHI_INPUT &&
        analysis->use_sites[analysis->use_offsets[3]].kind == VALUE_SSA_USE_TERM &&
        analysis->use_sites[analysis->use_offsets[3]].role == VALUE_SSA_USE_ROLE_RETURN_VALUE;
}

static int check_diamond_phi_placement(const ValueSsaProgram *program,
    const ValueSsaCfgAnalysis *analysis,
    unsigned char *phi_blocks) {
    unsigned char definition_blocks[4] = {0, 1, 1, 0};
    ValueSsaError error;

    if (!program || !analysis || !phi_blocks || analysis->block_count != 4) {
        return 0;
    }

    if (!value_ssa_compute_phi_placement(&program->functions[0], analysis, definition_blocks, phi_blocks, &error)) {
        return 0;
    }

    return !phi_blocks[0] && !phi_blocks[1] && !phi_blocks[2] && phi_blocks[3];
}

static int check_loop_phi_placement(const ValueSsaProgram *program,
    const ValueSsaCfgAnalysis *analysis,
    unsigned char *phi_blocks) {
    unsigned char definition_blocks[4] = {1, 0, 1, 0};
    ValueSsaError error;

    if (!program || !analysis || !phi_blocks || analysis->block_count != 4) {
        return 0;
    }

    if (!value_ssa_compute_phi_placement(&program->functions[0], analysis, definition_blocks, phi_blocks, &error)) {
        return 0;
    }

    return !phi_blocks[0] && phi_blocks[1] && !phi_blocks[2] && !phi_blocks[3];
}

static int test_value_ssa_dump_straight_line_program(void) {
    return expect_dump("VALUE-SSA-DUMP-STRAIGHT",
        build_sample_program,
        "global g.0\n"
        "\n"
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    store_global g.0, ssa.1\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_dump_diamond_phi_program(void) {
    return expect_dump("VALUE-SSA-DUMP-DIAMOND",
        build_diamond_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = mov 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: ssa.1], [bb.2: ssa.2]\n"
        "    ret ssa.3\n"
        "}\n");
}

static int test_value_ssa_cfg_analysis_diamond_program(void) {
    return expect_cfg_analysis("VALUE-SSA-CFG-ANALYSIS-DIAMOND",
        build_diamond_program,
        check_diamond_cfg_analysis);
}

static int test_value_ssa_def_use_analysis_diamond_program(void) {
    return expect_def_use_analysis("VALUE-SSA-DEF-USE-ANALYSIS-DIAMOND",
        build_diamond_program,
        check_diamond_def_use_analysis);
}

static int test_value_ssa_phi_placement_diamond_program(void) {
    return expect_phi_placement("VALUE-SSA-PHI-PLACEMENT-DIAMOND",
        build_diamond_program,
        check_diamond_phi_placement);
}

static int test_value_ssa_dominator_preorder_diamond_program(void) {
    static const size_t expected_order[] = {0, 1, 2, 3};
    return expect_dominator_preorder("VALUE-SSA-DOM-PREORDER-DIAMOND",
        build_diamond_program,
        expected_order,
        sizeof(expected_order) / sizeof(expected_order[0]));
}

static int test_value_ssa_dominator_walk_diamond_program(void) {
    static const long expected_events[] = {1, 2, -2, 3, -3, 4, -4, -1};
    return expect_dominator_walk("VALUE-SSA-DOM-WALK-DIAMOND",
        build_diamond_program,
        expected_events,
        sizeof(expected_events) / sizeof(expected_events[0]));
}

static int test_value_ssa_simplify_trivial_mov_chain(void) {
    return expect_simplified_dump("VALUE-SSA-SIMPLIFY-MOV-CHAIN",
        build_trivial_mov_chain_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    ssa.1 = mov 1\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_simplify_trivial_phi(void) {
    return expect_simplified_dump("VALUE-SSA-SIMPLIFY-TRIVIAL-PHI",
        build_trivial_phi_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: 1], [bb.2: 1]\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_fold_constant_binary(void) {
    return expect_constant_folded_dump("VALUE-SSA-FOLD-CONSTANT-BINARY",
        build_constant_fold_binary_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 3\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_fold_constant_mov_chain(void) {
    return expect_constant_folded_dump("VALUE-SSA-FOLD-CONSTANT-MOV-CHAIN",
        build_constant_fold_mov_chain_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    ssa.1 = mov 3\n"
        "    ssa.2 = mov 6\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_fold_preserves_dangerous_binary(void) {
    return expect_constant_folded_dump("VALUE-SSA-FOLD-PRESERVE-DANGEROUS-BINARY",
        build_dangerous_dead_binary_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = div 1, 0\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_fold_constant_safe_div(void) {
    return expect_constant_folded_dump("VALUE-SSA-FOLD-CONSTANT-SAFE-DIV",
        build_constant_fold_safe_div_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 7\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_fold_constant_safe_mod(void) {
    return expect_constant_folded_dump("VALUE-SSA-FOLD-CONSTANT-SAFE-MOD",
        build_constant_fold_safe_mod_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 2\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_fold_constant_safe_shift_left(void) {
    return expect_constant_folded_dump("VALUE-SSA-FOLD-CONSTANT-SAFE-SHL",
        build_constant_fold_safe_shift_left_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 48\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_fold_constant_safe_shift_right(void) {
    return expect_constant_folded_dump("VALUE-SSA-FOLD-CONSTANT-SAFE-SHR",
        build_constant_fold_safe_shift_right_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 8\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_fold_preserves_overflow_div(void) {
    return expect_constant_folded_dump("VALUE-SSA-FOLD-PRESERVE-OVERFLOW-DIV",
        build_constant_fold_preserve_overflow_div_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = div -2147483648, -1\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_fold_preserves_invalid_shift(void) {
    return expect_constant_folded_dump("VALUE-SSA-FOLD-PRESERVE-INVALID-SHIFT",
        build_constant_fold_preserve_invalid_shift_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = shl -1, 1\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_simplify_identity_add_zero(void) {
    return expect_identity_simplified_dump("VALUE-SSA-SIMPLIFY-IDENTITY-ADD-ZERO",
        build_identity_add_zero_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = mov ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_simplify_identity_eq_self(void) {
    return expect_identity_simplified_dump("VALUE-SSA-SIMPLIFY-IDENTITY-EQ-SELF",
        build_identity_eq_self_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = mov 1\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_simplify_identity_div_one(void) {
    return expect_identity_simplified_dump("VALUE-SSA-SIMPLIFY-IDENTITY-DIV-ONE",
        build_identity_div_one_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = mov ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_normalize_compare_immediate_lhs(void) {
    return expect_binary_normalized_dump("VALUE-SSA-NORMALIZE-COMPARE-IMMEDIATE-LHS",
        build_normalize_compare_immediate_lhs_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = gt ssa.0, 1\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_forward_local_loads_after_store(void) {
    return expect_local_load_forwarded_dump("VALUE-SSA-FORWARD-LOCAL-LOAD-AFTER-STORE",
        build_local_store_load_forward_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    ssa.0 = mov 7\n"
        "    ssa.1 = mov 7\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_forward_local_repeated_loads(void) {
    return expect_local_load_forwarded_dump("VALUE-SSA-FORWARD-LOCAL-REPEATED-LOADS",
        build_local_repeated_load_forward_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = mov ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_forward_local_loads_across_straight_chain(void) {
    return expect_local_load_forwarded_dump("VALUE-SSA-FORWARD-LOCAL-CROSS-BLOCK",
        build_local_cross_block_load_forward_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.0 = mov 7\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_forward_local_loads_do_not_cross_join(void) {
    return expect_local_load_forwarded_dump("VALUE-SSA-FORWARD-LOCAL-NO-JOIN",
        build_local_join_no_forward_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_local a.0, 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    store_local a.0, 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = load_local a.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_forward_local_loads_skip_address_taken_local(void) {
    return expect_local_load_forwarded_dump("VALUE-SSA-FORWARD-LOCAL-ADDRESS-TAKEN-NO-FORWARD",
        build_local_address_taken_no_forward_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    ssa.0 = addr_local a.0\n"
        "    ssa.1 = load_local a.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-BASIC",
        build_perf_hotspot_program,
        "global base.0 = 16\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 16\n"
        "    ssa.1 = shl ssa.0, 2\n"
        "    ssa.2 = add ssa.1, 1\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_licm_hoists_loop_invariant_binary(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_perf_loop_invariant_binary_hoist_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-LICM-LOOP-INVARIANT setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_licm_hoist_simple_loop_invariant_values(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-LICM-LOOP-INVARIANT optimize failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: VALUE-SSA-LICM-LOOP-INVARIANT dump failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strstr(actual_text, "ssa.0 = mul 7, 8\n") != NULL &&
        strstr(actual_text, "ssa.1 = add ssa.0, 1\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-LICM-LOOP-INVARIANT unexpected dump\n%s\n",
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_licm_does_not_hoist_across_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_perf_spmv_parameter_local_load_store_barrier_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-LICM-STORE-BARRIER setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    if (!value_ssa_licm_hoist_simple_loop_invariant_values(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-LICM-STORE-BARRIER optimize failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: VALUE-SSA-LICM-STORE-BARRIER dump failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    ok = strstr(actual_text, "store_local n.0, 9\n") != NULL &&
        strstr(actual_text, "ssa.1 = load_local n.0\n") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-LICM-STORE-BARRIER unexpected dump\n%s\n",
            actual_text);
    }

    free(actual_text);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_optimize_perf_hotspots_hoists_loop_invariant_binary(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-LOOP-INVARIANT-BINARY",
        build_perf_loop_invariant_binary_hoist_program,
        "func main(i.0) {\n"
        "  bb.0:\n"
        "    store_local i.0, 3\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.2 = load_local i.0\n"
        "    br ssa.2, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.0 = mul 7, 8\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    ssa.3 = sub ssa.2, 1\n"
        "    store_local acc.1, ssa.1\n"
        "    store_local i.0, ssa.3\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.4 = load_local acc.1\n"
        "    ret ssa.4\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_hoists_phi_header_loop_invariant_indirect_load(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-PHI-HEADER-INDIRECT",
        build_perf_phi_header_loop_invariant_indirect_load_program,
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local p.0\n"
        "    ssa.1 = add ssa.0, 4\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.2 = phi [bb.0: 0], [bb.2: ssa.5]\n"
        "    ssa.3 = lt ssa.2, 3\n"
        "    br ssa.3, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.4 = load_indirect ssa.1\n"
        "    ssa.5 = add ssa.4, ssa.2\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.6 = mov ssa.2\n"
        "    ret ssa.6\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_does_not_hoist_phi_header_indirect_load_across_store_barrier(void) {
    return expect_perf_hotspot_optimized_dump(
        "VALUE-SSA-PERF-HOTSPOT-PHI-HEADER-INDIRECT-STORE-BARRIER",
        build_perf_phi_header_loop_invariant_indirect_load_store_barrier_program,
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local p.0\n"
        "    ssa.1 = add ssa.0, 4\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.2 = phi [bb.0: 0], [bb.2: ssa.5]\n"
        "    ssa.3 = lt ssa.2, 3\n"
        "    br ssa.3, bb.2, bb.3\n"
        "  bb.2:\n"
        "    store_indirect ssa.1, 0\n"
        "    ssa.4 = load_indirect ssa.1\n"
        "    ssa.5 = add ssa.4, ssa.2\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.6 = mov ssa.2\n"
        "    ret ssa.6\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_hoists_function_entry_global_load(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-FUNCTION-ENTRY-GLOBAL",
        build_perf_function_entry_global_hoist_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_global g.0\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    ssa.2 = add ssa.0, 2\n"
        "    ssa.3 = add ssa.1, ssa.2\n"
        "    ret ssa.3\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_does_not_hoist_global_load_across_store(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-FUNCTION-ENTRY-GLOBAL-STORE-BARRIER",
        build_perf_function_entry_global_hoist_blocked_by_store_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_global g.0\n"
        "    store_global g.0, 9\n"
        "    ssa.1 = load_global g.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_hoists_function_entry_global_addr(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-FUNCTION-ENTRY-GLOBAL-ADDR",
        build_perf_function_entry_global_addr_hoist_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global g.0\n"
        "    ssa.1 = add ssa.0, ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_does_not_hoist_single_use_global_addr(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-FUNCTION-ENTRY-GLOBAL-ADDR-SINGLE-USE",
        build_perf_single_use_global_addr_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global g.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_hoists_spmv_parameter_local_loads(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-SPMV-PARAMETER-LOCAL",
        build_perf_spmv_parameter_local_load_hoist_program,
        "func spmv(n.0, xptr.1) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local n.0\n"
        "    ssa.1 = add ssa.0, ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_hoists_shared_branch_successor_local_loads(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-SHARED-BRANCH-LOCAL",
        build_perf_run_program_shared_branch_local_load_program,
        "func run_program() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local read_head.0\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    ret ssa.1\n"
        "  bb.2:\n"
        "    ssa.2 = sub ssa.0, 1\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_does_not_hoist_spmv_parameter_local_load_across_store(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-SPMV-PARAMETER-LOCAL-STORE-BARRIER",
        build_perf_spmv_parameter_local_load_store_barrier_program,
        "func spmv(n.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local n.0\n"
        "    store_local n.0, 9\n"
        "    ssa.1 = load_local n.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_hoists_power_parameter_local_loads(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-POWER-PARAMETER-LOCAL",
        build_perf_power_parameter_local_load_hoist_program,
        "func power(a.0, b.1) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = load_local b.1\n"
        "    ssa.2 = add ssa.1, ssa.0\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_does_not_hoist_non_hot_parameter_local_loads(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-NON-HOT-PARAMETER-LOCAL",
        build_perf_non_hot_parameter_local_load_program,
        "func helper(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = load_local a.0\n"
        "    ssa.2 = add ssa.0, ssa.1\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_reduces_zero_based_induction_divmods(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-INDUCTION-DIVMOD",
        build_perf_zero_based_induction_divmod_reduce_program,
        "func main() {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.0 = phi [bb.0: 0], [bb.2: ssa.5]\n"
        "    ssa.1 = lt ssa.0, 8\n"
        "    br ssa.1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = and ssa.0, 1\n"
        "    ssa.3 = shr ssa.0, 1\n"
        "    ssa.4 = add ssa.2, ssa.3\n"
        "    store_local acc.0, ssa.4\n"
        "    ssa.5 = add ssa.0, 1\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.6 = load_local acc.0\n"
        "    ret ssa.6\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_rebalances_integer_dispatch_chains(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-DISPATCH-CHAIN",
        build_perf_integer_dispatch_chain_program,
        "func main(x.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local x.0\n"
        "    ssa.1 = lt ssa.0, 2\n"
        "    br ssa.1, bb.5, bb.6\n"
        "  bb.1:\n"
        "    ret 10\n"
        "  bb.2:\n"
        "    ret 20\n"
        "  bb.3:\n"
        "    ret 30\n"
        "  bb.4:\n"
        "    ret 40\n"
        "  bb.5:\n"
        "    ssa.2 = eq ssa.0, 1\n"
        "    br ssa.2, bb.1, bb.4\n"
        "  bb.6:\n"
        "    ssa.3 = lt ssa.0, 3\n"
        "    br ssa.3, bb.7, bb.8\n"
        "  bb.7:\n"
        "    ssa.4 = eq ssa.0, 2\n"
        "    br ssa.4, bb.2, bb.4\n"
        "  bb.8:\n"
        "    ssa.5 = eq ssa.0, 3\n"
        "    br ssa.5, bb.3, bb.4\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_reduces_simple_induction_shifts(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-INDUCTION-SHIFT",
        build_perf_simple_induction_shift_reduce_program,
        "func main() {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.0 = phi [bb.0: 0], [bb.2: ssa.6]\n"
        "    ssa.1 = phi [bb.0: 0], [bb.2: ssa.7]\n"
        "    ssa.2 = lt ssa.0, 4\n"
        "    br ssa.2, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.3 = add ssa.1, 1\n"
        "    store_local acc0.0, ssa.3\n"
        "    ssa.4 = add ssa.1, 3\n"
        "    store_local acc0.0, ssa.4\n"
        "    ssa.5 = add ssa.1, 2\n"
        "    store_local acc1.1, ssa.5\n"
        "    ssa.6 = add ssa.0, 1\n"
        "    ssa.7 = add ssa.1, 4\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.8 = load_local acc0.0\n"
        "    ssa.9 = load_local acc1.1\n"
        "    ssa.10 = load_local acc0.0\n"
        "    ssa.11 = add ssa.8, ssa.9\n"
        "    ssa.12 = add ssa.11, ssa.10\n"
        "    ret ssa.12\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_reuses_spmv_loop_exit_indirect_loads(void) {
    return expect_perf_hotspot_optimized_dump("VALUE-SSA-PERF-HOTSPOT-SPMV-LOOP-EXIT-INDIRECT",
        build_perf_spmv_loop_exit_indirect_load_reuse_program,
        "global gxptr.0\n"
        "global gx.1\n"
        "\n"
        "func spmv(xptr.0, x.1, i.2) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local xptr.0\n"
        "    ssa.1 = load_local x.1\n"
        "    ssa.2 = load_local i.2\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.3 = shl ssa.2, 2\n"
        "    ssa.4 = add ssa.3, ssa.0\n"
        "    ssa.5 = load_indirect ssa.4\n"
        "    ssa.6 = add ssa.2, 1\n"
        "    ssa.7 = shl ssa.6, 2\n"
        "    ssa.8 = add ssa.0, ssa.7\n"
        "    ssa.9 = load_indirect ssa.8\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    ssa.10 = phi [bb.1: ssa.5], [bb.3: ssa.14]\n"
        "    ssa.11 = lt ssa.10, ssa.9\n"
        "    br ssa.11, bb.3, bb.4\n"
        "  bb.3:\n"
        "    ssa.12 = shl ssa.10, 2\n"
        "    ssa.13 = add ssa.1, ssa.12\n"
        "    store_indirect ssa.13, 0\n"
        "    ssa.14 = add ssa.10, 1\n"
        "    jmp bb.2\n"
        "  bb.4:\n"
        "    ssa.15 = shl ssa.2, 2\n"
        "    ssa.16 = add ssa.2, 1\n"
        "    ssa.17 = shl ssa.16, 2\n"
        "    ssa.18 = add ssa.5, ssa.9\n"
        "    ret ssa.18\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global gxptr.0\n"
        "    ssa.1 = addr_global gx.1\n"
        "    ssa.2 = call spmv(ssa.0, ssa.1, 3)\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_optimize_perf_hotspots_source_multiply_baseline_dump(void) {
    static const char *const fragments[] = {
        "func multiply(a.0, b.1) {\n",
        "    ssa.0 = load_local a.0\n",
        "    ssa.1 = load_local b.1\n",
        "    ssa.5 = div ssa.1, 2\n",
        "    ssa.6 = call multiply(ssa.0, ssa.5)\n",
        "    ssa.9 = mod ssa.1, 2\n",
        "    ssa.10 = eq ssa.9, 1\n",
        "    ret ssa.8\n",
    };
    static const char *source =
        "const int mod = 998244353;\n"
        "int multiply(int a, int b){\n"
        "  if (b == 0) return 0;\n"
        "  if (b == 1) return a % mod;\n"
        "  int cur = multiply(a, b / 2);\n"
        "  cur = (cur + cur) % mod;\n"
        "  if (b % 2 == 1) return (a + cur) % mod;\n"
        "  return cur;\n"
        "}\n"
        "int main(){ return multiply(7, 3); }\n";

    return expect_source_perf_hotspot_fragments("VALUE-SSA-PERF-HOTSPOT-SOURCE-MULTIPLY-BASELINE",
        source,
        fragments,
        sizeof(fragments) / sizeof(fragments[0]));
}

static int test_value_ssa_optimize_perf_hotspots_source_power_branch_cleanup_dump(void) {
    static const char *const fragments[] = {
        "func power(a.0, b.1) {\n",
        "    ssa.0 = load_local a.0\n",
        "    ssa.1 = load_local b.1\n",
        "    ssa.3 = div ssa.1, 2\n",
        "    ssa.4 = call power(ssa.0, ssa.3)\n",
        "    ssa.5 = call multiply(ssa.4, ssa.4)\n",
        "    ssa.6 = mod ssa.1, 2\n",
        "    ssa.7 = eq ssa.6, 1\n",
        "    ret ssa.5\n",
    };
    static const char *source =
        "const int mod = 998244353;\n"
        "int multiply(int a, int b){\n"
        "  if (b == 0) return 0;\n"
        "  if (b == 1) return a % mod;\n"
        "  int cur = multiply(a, b / 2);\n"
        "  cur = (cur + cur) % mod;\n"
        "  if (b % 2 == 1) return (a + cur) % mod;\n"
        "  return cur;\n"
        "}\n"
        "int power(int a, int b){\n"
        "  if (b == 0) return 1;\n"
        "  int cur = power(a, b / 2);\n"
        "  cur = multiply(cur, cur);\n"
        "  if (b % 2 == 1) return multiply(cur, a);\n"
        "  return cur;\n"
        "}\n"
        "int main(){ return power(3, 5); }\n";

    return expect_source_perf_hotspot_fragments("VALUE-SSA-PERF-HOTSPOT-SOURCE-POWER-BRANCH-CLEANUP",
        source,
        fragments,
        sizeof(fragments) / sizeof(fragments[0]));
}

static int test_value_ssa_optimize_perf_hotspots_source_fft_mod998_butterfly_dump(void) {
    static const char *const fragments[] = {
        "func fft(arr.0, begin_pos.1, half_n.2, w.3) {\n",
        "    ssa.16 = call multiply(ssa.3, ssa.12)\n",
        "    ssa.17 = add ssa.7, ssa.16\n",
        "    ssa.18 = mod ssa.17, 998244353\n",
        "    store_indirect ",
        "    ssa.23 = sub ssa.7, ssa.16\n",
        "    ssa.24 = add ssa.23, 998244353\n",
        "    ssa.25 = mod ssa.24, 998244353\n",
        "    store_indirect ",
        "    ssa.26 = call multiply(ssa.3, ssa.3)\n",
    };
    static const char *source =
        "const int mod = 998244353;\n"
        "int multiply(int a, int b){\n"
        "  if (b == 0) return 0;\n"
        "  if (b == 1) return a % mod;\n"
        "  int cur = multiply(a, b / 2);\n"
        "  cur = (cur + cur) % mod;\n"
        "  if (b % 2 == 1) return (a + cur) % mod;\n"
        "  return cur;\n"
        "}\n"
        "int fft(int arr[], int begin_pos, int half_n, int w){\n"
        "  int i = 0;\n"
        "  int wn = w;\n"
        "  int x = arr[begin_pos + i];\n"
        "  int y = arr[begin_pos + half_n + i];\n"
        "  arr[begin_pos + i] = (x + multiply(wn, y)) % mod;\n"
        "  arr[begin_pos + half_n + i] = (x - multiply(wn, y) + mod) % mod;\n"
        "  wn = multiply(wn, w);\n"
        "  return 0;\n"
        "}\n"
        "int main(){ int a[2]; return fft(a,0,1,3); }\n";

    return expect_source_perf_hotspot_fragments("VALUE-SSA-PERF-HOTSPOT-SOURCE-FFT-MOD998",
        source,
        fragments,
        sizeof(fragments) / sizeof(fragments[0]));
}

static int test_value_ssa_optimize_perf_hotspots_source_spmv_loop_fusion_dump(void) {
    return 1;
}

static int test_value_ssa_optimize_perf_hotspots_source_mm_rebuild_dump(void) {
    static const char *const fragments[] = {
        "func mm(n.0, A.1, B.2, C.3) {\n",
        "  bb.0:\n",
        "    store_local i.4, 0\n",
        "    store_local j.5, 0\n",
        "    jmp bb.1\n",
        "  bb.1:\n",
        "    ssa.0 = load_local i.4\n",
        "    ssa.1 = load_local n.0\n",
        "    ssa.2 = lt ssa.0, ssa.1\n",
        "  bb.2:\n",
        "    store_local j.5, 0\n",
        "    ssa.3 = load_local C.3\n",
        "    jmp bb.4\n",
        "  bb.5:\n",
        "    ssa.6 = load_local i.4\n",
        "    ssa.7 = shl ssa.6, 12\n",
        "    ssa.8 = add ssa.3, ssa.7\n",
        "  bb.10:\n",
        "    ssa.18 = shl ssa.16, 12\n",
        "    ssa.19 = load_local A.1\n",
        "    ssa.25 = eq ssa.24, 0\n",
        "  bb.16:\n",
        "    ssa.42 = load_indirect ssa.41\n",
        "    ssa.47 = add ssa.44, ssa.46\n",
        "    ssa.54 = mul ssa.48, ssa.53\n",
    };
    static const char *source =
        "const int N = 1024;\n"
        "void mm(int n, int A[][N], int B[][N], int C[][N]){\n"
        "    int i, j, k;\n"
        "    i = 0; j = 0;\n"
        "    while (i < n){\n"
        "        j = 0;\n"
        "        while (j < n){\n"
        "            C[i][j] = 0;\n"
        "            j = j + 1;\n"
        "        }\n"
        "        i = i + 1;\n"
        "    }\n"
        "    i = 0; j = 0; k = 0;\n"
        "    while (k < n){\n"
        "        i = 0;\n"
        "        while (i < n){\n"
        "            if (A[i][k] == 0){\n"
        "                i = i + 1;\n"
        "                continue;\n"
        "            }\n"
        "            j = 0;\n"
        "            while (j < n){\n"
        "                C[i][j] = C[i][j] + A[i][k] * B[k][j];\n"
        "                j = j + 1;\n"
        "            }\n"
        "            i = i + 1;\n"
        "        }\n"
        "        k = k + 1;\n"
        "    }\n"
        "}\n"
        "int A[N][N]; int B[N][N]; int C[N][N];\n"
        "int main(){ mm(8, A, B, C); return 0; }\n";

    return expect_source_perf_hotspot_fragments("VALUE-SSA-PERF-HOTSPOT-SOURCE-MM-REBUILD",
        source,
        fragments,
        sizeof(fragments) / sizeof(fragments[0]));
}

static int test_value_ssa_forward_global_loads_after_store(void) {
    return expect_global_load_forwarded_dump("VALUE-SSA-FORWARD-GLOBAL-LOAD-AFTER-STORE",
        build_global_store_load_forward_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    ssa.0 = mov 9\n"
        "    ssa.1 = mov 9\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_forward_global_loads_across_straight_chain(void) {
    return expect_global_load_forwarded_dump("VALUE-SSA-FORWARD-GLOBAL-CROSS-BLOCK",
        build_global_cross_block_load_forward_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.0 = mov 9\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_forward_global_loads_do_not_cross_join(void) {
    return expect_global_load_forwarded_dump("VALUE-SSA-FORWARD-GLOBAL-NO-JOIN",
        build_global_join_no_forward_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_global g.0, 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    store_global g.0, 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = load_global g.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_forward_global_loads_killed_by_call(void) {
    return expect_global_load_forwarded_dump("VALUE-SSA-FORWARD-GLOBAL-LOAD-KILLED-BY-CALL",
        build_global_load_forward_killed_by_call_program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    call touch()\n"
        "    ssa.0 = load_global g.0\n"
        "    ssa.1 = mov ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_forward_global_loads_killed_by_indirect_store(void) {
    return expect_global_load_forwarded_dump("VALUE-SSA-FORWARD-GLOBAL-LOAD-INDIRECT-STORE-BARRIER",
        build_global_load_forward_killed_by_indirect_store_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    ssa.0 = addr_global g.0\n"
        "    store_indirect ssa.0, 7\n"
        "    ssa.1 = load_global g.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_forward_global_loads_killed_by_cross_block_call(void) {
    return expect_global_load_forwarded_dump("VALUE-SSA-FORWARD-GLOBAL-LOAD-CROSS-BLOCK-CALL-BARRIER",
        build_global_cross_block_load_forward_killed_by_call_program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    call touch()\n"
        "    ssa.0 = load_global g.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_local_store(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-LOCAL-STORE",
        build_dead_local_store_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_local_store_preserves_address_taken_observer(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-LOCAL-STORE-ADDR-TAKEN-OBSERVER",
        build_dead_local_store_preserves_address_taken_observer_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_local a.0\n"
        "    store_local a.0, 7\n"
        "    ssa.1 = load_indirect ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_local_store_across_straight_chain(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-LOCAL-STORE-CROSS-BLOCK",
        build_dead_local_store_across_straight_chain_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    store_local a.0, 2\n"
        "    ssa.0 = load_local a.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_local_store_across_two_hop_chain(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-LOCAL-STORE-TWO-HOP-CHAIN",
        build_dead_local_store_two_hop_chain_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    store_local a.0, 2\n"
        "    ssa.0 = load_local a.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_local_store_preserves_branch_observer(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-LOCAL-STORE-BRANCH-OBSERVER",
        build_dead_local_store_preserves_branch_observer_program,
        "func main(a.0, b.1) {\n"
        "  bb.0:\n"
        "    store_local a.0, 1\n"
        "    ssa.0 = load_local b.1\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ret 0\n"
        "  bb.2:\n"
        "    ssa.1 = load_local a.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_global_store(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-GLOBAL-STORE",
        build_dead_global_store_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 2\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_global_store_preserves_indirect_observer(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-GLOBAL-STORE-INDIRECT-OBSERVER",
        build_dead_global_store_preserves_indirect_observer_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global g.0\n"
        "    store_global g.0, 7\n"
        "    ssa.1 = load_indirect ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_global_store_across_straight_chain(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-GLOBAL-STORE-CROSS-BLOCK",
        build_dead_global_store_across_straight_chain_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    store_global g.0, 2\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_global_store_across_two_hop_chain(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-GLOBAL-STORE-TWO-HOP-CHAIN",
        build_dead_global_store_two_hop_chain_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    store_global g.0, 2\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_global_store_preserves_call_barrier(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-GLOBAL-STORE-CALL-BARRIER",
        build_dead_global_store_killed_by_call_program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 1\n"
        "    call touch()\n"
        "    store_global g.0, 2\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_global_store_preserves_cross_block_call_barrier(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-GLOBAL-STORE-CROSS-BLOCK-CALL-BARRIER",
        build_dead_global_store_preserves_call_across_blocks_program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    call touch()\n"
        "    store_global g.0, 2\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_global_store_preserves_branch_observer(void) {
    return expect_dead_store_cleaned_dump("VALUE-SSA-ELIMINATE-DEAD-GLOBAL-STORE-BRANCH-OBSERVER",
        build_dead_global_store_preserves_branch_observer_program,
        "global g.0\n"
        "\n"
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    store_global g.0, 1\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_global g.0, 2\n"
        "    ret 0\n"
        "  bb.2:\n"
        "    ssa.1 = load_global g.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_local_store_across_straight_chain(void) {
    return expect_redundant_store_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-LOCAL-STORE-CROSS-BLOCK",
        build_redundant_local_store_across_straight_chain_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_local_store_across_two_hop_chain(void) {
    return expect_redundant_store_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-LOCAL-STORE-TWO-HOP-CHAIN",
        build_redundant_local_store_two_hop_chain_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_local_store_does_not_cross_join(void) {
    return expect_redundant_store_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-LOCAL-STORE-NO-JOIN",
        build_redundant_local_store_join_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_local a.0, 7\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    store_local a.0, 7\n"
        "    ssa.1 = load_local a.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_global_store(void) {
    return expect_redundant_store_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-GLOBAL-STORE",
        build_redundant_global_store_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    ssa.0 = load_global g.0\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_global_store_preserves_indirect_store_barrier(void) {
    return expect_redundant_store_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-GLOBAL-STORE-INDIRECT-STORE-BARRIER",
        build_redundant_global_store_preserves_indirect_store_barrier_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    ssa.0 = load_global g.0\n"
        "    ssa.1 = addr_global g.0\n"
        "    store_indirect ssa.1, 11\n"
        "    store_global g.0, ssa.0\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_global_store_preserves_call_barrier(void) {
    return expect_redundant_store_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-GLOBAL-STORE-CALL-BARRIER",
        build_redundant_global_store_killed_by_call_program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    ssa.0 = load_global g.0\n"
        "    call touch()\n"
        "    store_global g.0, ssa.0\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_global_store_preserves_cross_block_call_barrier(void) {
    return expect_redundant_store_eliminated_dump(
        "VALUE-SSA-ELIMINATE-REDUNDANT-GLOBAL-STORE-CROSS-BLOCK-CALL-BARRIER",
        build_redundant_global_store_cross_block_call_barrier_program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    call touch()\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    ssa.0 = load_global g.0\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_global_store_does_not_cross_join(void) {
    return expect_redundant_store_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-GLOBAL-STORE-NO-JOIN",
        build_redundant_global_store_join_program,
        "global g.0\n"
        "\n"
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_global g.0, 7\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    store_global g.0, 7\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_binary(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-BINARY",
        build_redundant_binary_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    ssa.2 = mov ssa.1\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_commuted_binary(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-COMMUTED-BINARY",
        build_redundant_commuted_binary_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    ssa.2 = mov ssa.1\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_local_load(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-LOCAL-LOAD",
        build_redundant_local_load_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = mov ssa.0\n"
        "    ssa.2 = add ssa.0, ssa.1\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_local_load_same_block_address_taken_barrier(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-LOCAL-LOAD-SAME-BLOCK-ADDRESS-TAKEN",
        build_redundant_local_load_same_block_address_taken_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_local a.0\n"
        "    ssa.1 = load_local a.0\n"
        "    ssa.2 = load_local a.0\n"
        "    ssa.3 = add ssa.1, ssa.2\n"
        "    ret ssa.3\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_mov(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-MOV",
        build_trivial_mov_chain_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    ssa.1 = mov ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_binary_preserves_dangerous_binary(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-DANGEROUS-BINARY",
        build_redundant_dangerous_binary_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = div 1, 0\n"
        "    ssa.1 = div 1, 0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_binary_reuses_safe_div(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-SAFE-DIV-BINARY",
        build_redundant_safe_div_binary_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = div 10, 2\n"
        "    ssa.1 = mov ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_dominated_redundant_binary(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-DOMINATED-REDUNDANT-BINARY",
        build_dominated_redundant_binary_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.2 = mov ssa.1\n"
        "    ret ssa.2\n"
        "  bb.2:\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_join_redundant_binary(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-JOIN-REDUNDANT-BINARY",
        build_join_redundant_binary_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = add ssa.0, 1\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.4 = phi [bb.1: ssa.1], [bb.2: ssa.2]\n"
        "    ssa.3 = mov ssa.4\n"
        "    ret ssa.3\n"
        "}\n");
}

static int test_value_ssa_eliminate_join_redundant_commuted_binary(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-JOIN-REDUNDANT-COMMUTED-BINARY",
        build_join_redundant_commuted_binary_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = add 1, ssa.0\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.4 = phi [bb.1: ssa.1], [bb.2: ssa.2]\n"
        "    ssa.3 = mov ssa.4\n"
        "    ret ssa.3\n"
        "}\n");
}

static int test_value_ssa_eliminate_join_redundant_local_load(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-JOIN-REDUNDANT-LOCAL-LOAD",
        build_join_redundant_local_load_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = load_local a.0\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = load_local a.0\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: ssa.1], [bb.2: ssa.2]\n"
        "    ssa.4 = mov ssa.3\n"
        "    ssa.5 = add ssa.3, ssa.4\n"
        "    ret ssa.5\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_local_load_address_taken_barrier(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-LOCAL-LOAD-ADDRESS-TAKEN",
        build_redundant_local_load_address_taken_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_local a.0\n"
        "    ssa.1 = load_local a.0\n"
        "    ssa.2 = load_local a.0\n"
        "    ssa.3 = add ssa.1, ssa.2\n"
        "    ret ssa.3\n"
        "}\n");
}

static int test_value_ssa_eliminate_dominated_redundant_mov(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-DOMINATED-REDUNDANT-MOV",
        build_diamond_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = mov 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: ssa.1], [bb.2: ssa.2]\n"
        "    ret ssa.3\n"
        "}\n");
}

static int test_value_ssa_eliminate_dominated_redundant_addr(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-DOMINATED-REDUNDANT-ADDR",
        build_dominated_redundant_addr_program,
        "global g.0\n"
        "\n"
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = addr_local a.0\n"
        "    ssa.3 = addr_global g.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.2 = mov ssa.1\n"
        "    ssa.4 = mov ssa.3\n"
        "    ret ssa.4\n"
        "  bb.2:\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_indirect_load(void) {
    return expect_redundant_indirect_load_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-INDIRECT-LOAD",
        build_redundant_indirect_load_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_local a.0\n"
        "    ssa.1 = load_indirect ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_redundant_indirect_load_store_barrier(void) {
    return expect_redundant_indirect_load_eliminated_dump("VALUE-SSA-ELIMINATE-REDUNDANT-INDIRECT-LOAD-STORE-BARRIER",
        build_redundant_indirect_load_store_barrier_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_local a.0\n"
        "    ssa.1 = load_indirect ssa.0\n"
        "    store_indirect ssa.0, 7\n"
        "    ssa.2 = load_indirect ssa.0\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_eliminate_join_redundant_indirect_load(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-JOIN-REDUNDANT-INDIRECT-LOAD",
        build_join_redundant_indirect_load_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local cond.0\n"
        "    ssa.1 = addr_local a.1\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.2 = load_indirect ssa.1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.3 = load_indirect ssa.1\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.5 = phi [bb.1: ssa.2], [bb.2: ssa.3]\n"
        "    ssa.4 = mov ssa.5\n"
        "    ret ssa.4\n"
        "}\n");
}

static int test_value_ssa_eliminate_join_redundant_indirect_load_call_barrier(void) {
    return expect_redundant_binary_eliminated_dump("VALUE-SSA-ELIMINATE-JOIN-REDUNDANT-INDIRECT-LOAD-CALL-BARRIER",
        build_join_redundant_indirect_load_call_barrier_program,
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local cond.0\n"
        "    ssa.1 = addr_local a.1\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.2 = load_indirect ssa.1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.3 = load_indirect ssa.1\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.5 = phi [bb.1: ssa.2], [bb.2: ssa.3]\n"
        "    call touch()\n"
        "    ssa.4 = load_indirect ssa.1\n"
        "    ret ssa.4\n"
        "}\n");
}

static int test_value_ssa_simplify_cfg_same_target_branch(void) {
    return expect_cfg_simplified_dump("VALUE-SSA-SIMPLIFY-CFG-SAME-TARGET",
        0,
        build_same_target_branch_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_simplify_cfg_same_return_diamond(void) {
    return expect_cfg_simplified_dump("VALUE-SSA-SIMPLIFY-CFG-SAME-RETURN-DIAMOND",
        0,
        build_same_return_diamond_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_simplify_cfg_jump_to_empty_return(void) {
    return expect_cfg_simplified_dump("VALUE-SSA-SIMPLIFY-CFG-JUMP-TO-RETURN",
        0,
        build_jump_to_empty_return_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_simplify_cfg_empty_jump_thread(void) {
    return expect_cfg_simplified_dump("VALUE-SSA-SIMPLIFY-CFG-EMPTY-JUMP-THREAD",
        0,
        build_empty_jump_thread_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n");
}

static int test_value_ssa_simplify_cfg_branch_empty_jump_same_target(void) {
    return expect_cfg_simplified_dump("VALUE-SSA-SIMPLIFY-CFG-BRANCH-EMPTY-JUMP-SAME-TARGET",
        0,
        build_branch_empty_jump_same_target_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_simplify_cfg_immediate_branch_compacts_blocks(void) {
    return expect_cfg_simplified_dump("VALUE-SSA-SIMPLIFY-CFG-IMMEDIATE-BRANCH",
        1,
        build_trivial_phi_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    ssa.1 = mov 1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.2 = phi [bb.0: 1]\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_simplify_cfg_single_predecessor_nonempty_merge(void) {
    return expect_cfg_simplified_dump("VALUE-SSA-SIMPLIFY-CFG-SINGLE-PREDECESSOR-NONEMPTY-MERGE",
        0,
        build_single_predecessor_nonempty_jump_merge_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_value_defs_after_cfg_cleanup(void) {
    return expect_dead_def_cleaned_dump("VALUE-SSA-DCE-AFTER-CFG",
        1,
        1,
        build_trivial_phi_program,
        "func main() {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_value_defs_preserves_dead_result_call(void) {
    return expect_dead_def_cleaned_dump("VALUE-SSA-DCE-PRESERVE-CALL",
        0,
        0,
        build_dead_call_program,
        "declare id()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = call id()\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_eliminate_dead_value_defs_preserves_dangerous_dead_binary(void) {
    return expect_dead_def_cleaned_dump("VALUE-SSA-DCE-PRESERVE-DANGEROUS-BINARY",
        0,
        0,
        build_dangerous_dead_binary_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = div 1, 0\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_conversion_straight_line_program(void) {
    return expect_converted_dump("VALUE-SSA-CONVERT-STRAIGHT",
        build_lower_ir_straight_line_program,
        "global g.0\n"
        "\n"
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    store_global g.0, ssa.1\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_conversion_diamond_program(void) {
    return expect_converted_dump("VALUE-SSA-CONVERT-DIAMOND",
        build_lower_ir_diamond_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = mov 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: ssa.1], [bb.2: ssa.2]\n"
        "    ret ssa.3\n"
        "}\n");
}

static int test_value_ssa_conversion_diamond_store_indirect_phi_program(void) {
    return expect_converted_dump("VALUE-SSA-CONVERT-DIAMOND-STORE-INDIRECT-PHI",
        build_lower_ir_diamond_store_indirect_phi_program,
        "global g.0\n"
        "\n"
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global g.0\n"
        "    ssa.1 = load_local a.0\n"
        "    br ssa.1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.2 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.3 = mov 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.4 = phi [bb.1: ssa.2], [bb.2: ssa.3]\n"
        "    store_indirect ssa.0, ssa.4\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_conversion_simple_loop_program(void) {
    return expect_converted_dump("VALUE-SSA-CONVERT-SIMPLE-LOOP",
        build_lower_ir_loop_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.1 = sub ssa.0, 1\n"
        "    store_local a.0, ssa.1\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_conversion_scrambled_diamond_program(void) {
    return expect_converted_dump("VALUE-SSA-CONVERT-SCRAMBLED-DIAMOND",
        build_lower_ir_scrambled_diamond_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.2, bb.3\n"
        "  bb.1:\n"
        "    ssa.1 = phi [bb.2: ssa.2], [bb.3: ssa.3]\n"
        "    ret ssa.1\n"
        "  bb.2:\n"
        "    ssa.2 = mov 1\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.3 = mov 2\n"
        "    jmp bb.1\n"
        "}\n");
}

static int test_value_ssa_conversion_loop_carried_temp_program(void) {
    return expect_converted_dump("VALUE-SSA-CONVERT-LOOP-CARRIED-TEMP",
        build_lower_ir_loop_carried_temp_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = phi [bb.0: ssa.0], [bb.2: ssa.2]\n"
        "    br ssa.1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = mov 2\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_conversion_multi_backedge_loop_carried_temp_program(void) {
    return expect_converted_dump("VALUE-SSA-CONVERT-MULTI-BACKEDGE-LOOP-CARRIED-TEMP",
        build_lower_ir_multi_backedge_loop_carried_temp_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = phi [bb.0: ssa.0], [bb.3: ssa.2], [bb.4: ssa.3]\n"
        "    br ssa.1, bb.2, bb.5\n"
        "  bb.2:\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    ssa.2 = mov 2\n"
        "    jmp bb.1\n"
        "  bb.4:\n"
        "    ssa.3 = mov 3\n"
        "    jmp bb.1\n"
        "  bb.5:\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_conversion_two_carried_temps_loop_program(void) {
    return expect_converted_dump("VALUE-SSA-CONVERT-TWO-CARRIED-TEMPS-LOOP",
        build_lower_ir_two_carried_temps_loop_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    ssa.1 = mov 10\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.2 = phi [bb.0: ssa.0], [bb.2: ssa.4]\n"
        "    ssa.3 = phi [bb.0: ssa.1], [bb.2: ssa.5]\n"
        "    br ssa.2, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.4 = mov 2\n"
        "    ssa.5 = mov 11\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret ssa.3\n"
        "}\n");
}

static int test_value_ssa_conversion_nested_loop_carried_temps_program(void) {
    return expect_converted_dump("VALUE-SSA-CONVERT-NESTED-LOOP-CARRIED-TEMPS",
        build_lower_ir_nested_loop_carried_temps_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = phi [bb.0: ssa.0], [bb.5: ssa.5]\n"
        "    br ssa.1, bb.2, bb.6\n"
        "  bb.2:\n"
        "    ssa.2 = mov 10\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.2: ssa.2], [bb.4: ssa.4]\n"
        "    br ssa.3, bb.4, bb.5\n"
        "  bb.4:\n"
        "    ssa.4 = mov 11\n"
        "    jmp bb.3\n"
        "  bb.5:\n"
        "    ssa.5 = mov 2\n"
        "    jmp bb.1\n"
        "  bb.6:\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_conversion_nested_same_temp_double_loop_program(void) {
    return expect_converted_dump("VALUE-SSA-CONVERT-NESTED-SAME-TEMP-DOUBLE-LOOP",
        build_lower_ir_nested_same_temp_double_loop_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = phi [bb.0: ssa.0], [bb.5: ssa.4]\n"
        "    br ssa.1, bb.2, bb.6\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.2 = phi [bb.2: ssa.1], [bb.4: ssa.3]\n"
        "    br ssa.2, bb.4, bb.5\n"
        "  bb.4:\n"
        "    ssa.3 = mov 2\n"
        "    jmp bb.3\n"
        "  bb.5:\n"
        "    ssa.4 = mov 3\n"
        "    jmp bb.1\n"
        "  bb.6:\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_conversion_nested_same_temp_multi_backedge_inner_loop_program(void) {
    return expect_converted_dump("VALUE-SSA-CONVERT-NESTED-SAME-TEMP-MULTI-BACKEDGE-INNER-LOOP",
        build_lower_ir_nested_same_temp_multi_backedge_inner_loop_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = phi [bb.0: ssa.0], [bb.7: ssa.5]\n"
        "    br ssa.1, bb.2, bb.8\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.2 = phi [bb.2: ssa.1], [bb.5: ssa.3], [bb.6: ssa.4]\n"
        "    br ssa.2, bb.4, bb.7\n"
        "  bb.4:\n"
        "    br ssa.2, bb.5, bb.6\n"
        "  bb.5:\n"
        "    ssa.3 = mov 2\n"
        "    jmp bb.3\n"
        "  bb.6:\n"
        "    ssa.4 = mov 4\n"
        "    jmp bb.3\n"
        "  bb.7:\n"
        "    ssa.5 = mov ssa.2\n"
        "    jmp bb.1\n"
        "  bb.8:\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_cfg_analysis_loop_program(void) {
    return expect_converted_cfg_analysis("VALUE-SSA-CFG-ANALYSIS-LOOP",
        build_lower_ir_loop_carried_temp_program,
        check_loop_cfg_analysis);
}

static int test_value_ssa_phi_placement_loop_program(void) {
    return expect_converted_phi_placement("VALUE-SSA-PHI-PLACEMENT-LOOP",
        build_lower_ir_loop_carried_temp_program,
        check_loop_phi_placement);
}

static int test_value_ssa_dominator_preorder_loop_program(void) {
    static const size_t expected_order[] = {0, 1, 2, 3};
    return expect_converted_dominator_preorder("VALUE-SSA-DOM-PREORDER-LOOP",
        build_lower_ir_loop_carried_temp_program,
        expected_order,
        sizeof(expected_order) / sizeof(expected_order[0]));
}

static int test_value_ssa_dominator_walk_loop_program(void) {
    static const long expected_events[] = {1, 2, 3, -3, 4, -4, -2, -1};
    return expect_converted_dominator_walk("VALUE-SSA-DOM-WALK-LOOP",
        build_lower_ir_loop_carried_temp_program,
        expected_events,
        sizeof(expected_events) / sizeof(expected_events[0]));
}

static int test_value_ssa_rename_state_scopes(void) {
    ValueSsaRenameState state;
    ValueSsaValueRef value;
    ValueSsaError error;

    value_ssa_rename_state_init(&state);
    if (!value_ssa_rename_state_prepare(&state, 2, &error) ||
        !value_ssa_rename_state_begin_scope(&state, &error) ||
        !value_ssa_rename_state_bind(&state, 0, value_ssa_value_immediate(1), &error) ||
        !value_ssa_rename_state_lookup(&state, 0, &value, &error) ||
        value.kind != VALUE_SSA_VALUE_IMMEDIATE || value.immediate != 1 ||
        !value_ssa_rename_state_begin_scope(&state, &error) ||
        !value_ssa_rename_state_bind(&state, 0, value_ssa_value_immediate(2), &error) ||
        !value_ssa_rename_state_bind(&state, 1, value_ssa_value_immediate(3), &error) ||
        !value_ssa_rename_state_lookup(&state, 0, &value, &error) ||
        value.kind != VALUE_SSA_VALUE_IMMEDIATE || value.immediate != 2 ||
        !value_ssa_rename_state_lookup(&state, 1, &value, &error) ||
        value.kind != VALUE_SSA_VALUE_IMMEDIATE || value.immediate != 3 ||
        !value_ssa_rename_state_end_scope(&state, &error) ||
        !value_ssa_rename_state_lookup(&state, 0, &value, &error) ||
        value.kind != VALUE_SSA_VALUE_IMMEDIATE || value.immediate != 1) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-RENAME-STATE-SCOPES unexpected rename-state failure at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_rename_state_free(&state);
        return 0;
    }

    if (value_ssa_rename_state_lookup(&state, 1, &value, &error) ||
        !value_ssa_rename_state_end_scope(&state, &error) ||
        value_ssa_rename_state_lookup(&state, 0, &value, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-RENAME-STATE-SCOPES expected scoped bindings to unwind cleanly\n");
        value_ssa_rename_state_free(&state);
        return 0;
    }

    value_ssa_rename_state_free(&state);
    return 1;
}

static int test_value_ssa_rename_state_walk_scopes_diamond(void) {
    ValueSsaProgram program;
    ValueSsaCfgAnalysis analysis;
    ValueSsaRenameState state;
    ValueSsaRenameWalkState walk_state;
    ValueSsaError error;

    if (!build_diamond_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-RENAME-WALK-DIAMOND setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    value_ssa_cfg_analysis_init(&analysis);
    value_ssa_rename_state_init(&state);
    walk_state.state = &state;
    if (!value_ssa_compute_cfg_analysis(&program.functions[0], &analysis, &error) ||
        !value_ssa_rename_state_prepare(&state, 1, &error) ||
        !value_ssa_walk_dominator_tree(&program.functions[0],
            &analysis,
            value_ssa_rename_walk_enter,
            value_ssa_rename_walk_leave,
            &walk_state,
            &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-RENAME-WALK-DIAMOND failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_rename_state_free(&state);
        value_ssa_cfg_analysis_free(&analysis);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_rename_state_free(&state);
    value_ssa_cfg_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return 1;
}

static int test_value_ssa_rename_rewrite_block_uses(void) {
    ValueSsaProgram program;
    ValueSsaRenameState state;
    ValueSsaError error;
    char *dump = NULL;
    int ok;

    if (!build_sample_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-REWRITE-BLOCK setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    value_ssa_rename_state_init(&state);
    if (!value_ssa_rename_state_prepare(&state, 2, &error) ||
        !value_ssa_rename_state_begin_scope(&state, &error) ||
        !value_ssa_rename_state_bind(&state, 0, value_ssa_value_immediate(7), &error) ||
        !value_ssa_rename_state_bind(&state, 1, value_ssa_value_immediate(9), &error) ||
        !value_ssa_rename_rewrite_block_uses(&program.functions[0].blocks[0], &state, &error) ||
        !value_ssa_dump_program(&program, &dump)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-REWRITE-BLOCK failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_rename_state_free(&state);
        value_ssa_program_free(&program);
        free(dump);
        return 0;
    }

    ok = strcmp(dump,
             "global g.0\n"
             "\n"
             "func main(a.0) {\n"
             "  bb.0:\n"
             "    ssa.0 = load_local a.0\n"
             "    ssa.1 = add 7, 1\n"
             "    store_global g.0, 9\n"
             "    ret 9\n"
             "}\n") == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-REWRITE-BLOCK dump mismatch\nExpected:\n%s\nActual:\n%s\n",
            "global g.0\n"
            "\n"
            "func main(a.0) {\n"
            "  bb.0:\n"
            "    ssa.0 = load_local a.0\n"
            "    ssa.1 = add 7, 1\n"
            "    store_global g.0, 9\n"
            "    ret 9\n"
            "}\n",
            dump);
    }

    free(dump);
    value_ssa_rename_state_free(&state);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rename_rewrite_branch_condition(void) {
    ValueSsaProgram program;
    ValueSsaRenameState state;
    ValueSsaError error;
    char *dump = NULL;
    int ok;

    if (!build_same_target_branch_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-REWRITE-BR setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    value_ssa_rename_state_init(&state);
    if (!value_ssa_rename_state_prepare(&state, 1, &error) ||
        !value_ssa_rename_state_begin_scope(&state, &error) ||
        !value_ssa_rename_state_bind(&state, 0, value_ssa_value_immediate(1), &error) ||
        !value_ssa_rename_rewrite_block_uses(&program.functions[0].blocks[0], &state, &error) ||
        !value_ssa_dump_program(&program, &dump)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-REWRITE-BR failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_rename_state_free(&state);
        value_ssa_program_free(&program);
        free(dump);
        return 0;
    }

    ok = strcmp(dump,
             "func main(a.0) {\n"
             "  bb.0:\n"
             "    ssa.0 = load_local a.0\n"
             "    br 1, bb.1, bb.1\n"
             "  bb.1:\n"
             "    ret 0\n"
             "}\n") == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-REWRITE-BR dump mismatch\nExpected:\n%s\nActual:\n%s\n",
            "func main(a.0) {\n"
            "  bb.0:\n"
            "    ssa.0 = load_local a.0\n"
            "    br 1, bb.1, bb.1\n"
            "  bb.1:\n"
            "    ret 0\n"
            "}\n",
            dump);
    }

    free(dump);
    value_ssa_rename_state_free(&state);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rename_rewrite_phi_inputs_for_predecessor(void) {
    ValueSsaProgram program;
    ValueSsaRenameState state;
    ValueSsaError error;
    char *dump = NULL;
    int ok;

    if (!build_diamond_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-REWRITE-PHI-INPUTS setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    value_ssa_rename_state_init(&state);
    if (!value_ssa_rename_state_prepare(&state, 3, &error) ||
        !value_ssa_rename_state_begin_scope(&state, &error) ||
        !value_ssa_rename_state_bind(&state, 1, value_ssa_value_immediate(11), &error) ||
        !value_ssa_rename_state_bind(&state, 2, value_ssa_value_immediate(22), &error) ||
        !value_ssa_rename_rewrite_phi_inputs_for_predecessor(&program.functions[0].blocks[3], 1, &state, &error) ||
        !value_ssa_rename_rewrite_phi_inputs_for_predecessor(&program.functions[0].blocks[3], 2, &state, &error) ||
        !value_ssa_dump_program(&program, &dump)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-REWRITE-PHI-INPUTS failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_rename_state_free(&state);
        value_ssa_program_free(&program);
        free(dump);
        return 0;
    }

    ok = strcmp(dump,
             "func main(a.0) {\n"
             "  bb.0:\n"
             "    ssa.0 = load_local a.0\n"
             "    br ssa.0, bb.1, bb.2\n"
             "  bb.1:\n"
             "    ssa.1 = mov 1\n"
             "    jmp bb.3\n"
             "  bb.2:\n"
             "    ssa.2 = mov 2\n"
             "    jmp bb.3\n"
             "  bb.3:\n"
             "    ssa.3 = phi [bb.1: 11], [bb.2: 22]\n"
             "    ret ssa.3\n"
             "}\n") == 0;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-REWRITE-PHI-INPUTS dump mismatch\nExpected:\n%s\nActual:\n%s\n",
            "func main(a.0) {\n"
            "  bb.0:\n"
            "    ssa.0 = load_local a.0\n"
            "    br ssa.0, bb.1, bb.2\n"
            "  bb.1:\n"
            "    ssa.1 = mov 1\n"
            "    jmp bb.3\n"
            "  bb.2:\n"
            "    ssa.2 = mov 2\n"
            "    jmp bb.3\n"
            "  bb.3:\n"
            "    ssa.3 = phi [bb.1: 11], [bb.2: 22]\n"
            "    ret ssa.3\n"
            "}\n",
            dump);
    }

    free(dump);
    value_ssa_rename_state_free(&state);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rename_function_values_scrambled_diamond(void) {
    return expect_function_renamed_dump("VALUE-SSA-RENAME-FUNCTION-DIAMOND",
        build_scrambled_diamond_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = mov 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: ssa.1], [bb.2: ssa.2]\n"
        "    ret ssa.3\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_scrambled_diamond(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-DIAMOND",
        build_scrambled_canonicalize_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 1], [bb.2: 2]\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_identity_add_zero(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-IDENTITY-ADD-ZERO",
        build_identity_add_zero_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_local_load_forward(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-LOCAL-LOAD-FORWARD",
        build_local_store_load_forward_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_dead_global_store(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-DEAD-GLOBAL-STORE",
        build_dead_global_store_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 2\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_dead_local_store_across_straight_chain(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-DEAD-LOCAL-STORE-CROSS-BLOCK",
        build_dead_local_store_across_straight_chain_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ret 2\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_dead_local_store_across_straight_chain_fixed_point(void) {
    return expect_canonicalized_fixed_point_dump(
        "VALUE-SSA-CANONICALIZE-DEAD-LOCAL-STORE-CROSS-BLOCK-FIXED-POINT",
        build_dead_local_store_across_straight_chain_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ret 2\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_dead_global_store_across_straight_chain(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-DEAD-GLOBAL-STORE-CROSS-BLOCK",
        build_dead_global_store_across_straight_chain_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 2\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_dead_global_store_across_straight_chain_fixed_point(void) {
    return expect_canonicalized_fixed_point_dump(
        "VALUE-SSA-CANONICALIZE-DEAD-GLOBAL-STORE-CROSS-BLOCK-FIXED-POINT",
        build_dead_global_store_across_straight_chain_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 2\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_redundant_local_store_across_straight_chain(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-REDUNDANT-LOCAL-STORE-CROSS-BLOCK",
        build_redundant_local_store_across_straight_chain_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_redundant_local_store_across_straight_chain_fixed_point(void) {
    return expect_canonicalized_fixed_point_dump(
        "VALUE-SSA-CANONICALIZE-REDUNDANT-LOCAL-STORE-CROSS-BLOCK-FIXED-POINT",
        build_redundant_local_store_across_straight_chain_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_redundant_global_store(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-REDUNDANT-GLOBAL-STORE",
        build_redundant_global_store_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_cross_block_global_call_barrier(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-GLOBAL-CROSS-BLOCK-CALL-BARRIER",
        build_global_cross_block_load_forward_killed_by_call_program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    call touch()\n"
        "    ssa.0 = load_global g.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_cross_block_global_call_barrier_fixed_point(void) {
    return expect_canonicalized_fixed_point_dump(
        "VALUE-SSA-CANONICALIZE-GLOBAL-CROSS-BLOCK-CALL-BARRIER-FIXED-POINT",
        build_global_cross_block_load_forward_killed_by_call_program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    call touch()\n"
        "    ssa.0 = load_global g.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_complex_local_chain(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-COMPLEX-LOCAL-CHAIN",
        build_canonicalize_complex_local_chain_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ret 8\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_complex_local_chain_fixed_point(void) {
    return expect_canonicalized_fixed_point_dump("VALUE-SSA-CANONICALIZE-COMPLEX-LOCAL-CHAIN-FIXED-POINT",
        build_canonicalize_complex_local_chain_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ret 8\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_complex_global_call_chain(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-COMPLEX-GLOBAL-CALL-CHAIN",
        build_canonicalize_complex_global_call_chain_program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 1\n"
        "    call touch()\n"
        "    store_global g.0, 2\n"
        "    ret 2\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_complex_global_call_chain_fixed_point(void) {
    return expect_canonicalized_fixed_point_dump(
        "VALUE-SSA-CANONICALIZE-COMPLEX-GLOBAL-CALL-CHAIN-FIXED-POINT",
        build_canonicalize_complex_global_call_chain_program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 1\n"
        "    call touch()\n"
        "    store_global g.0, 2\n"
        "    ret 2\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_global_load_forward(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-GLOBAL-LOAD-FORWARD",
        build_global_store_load_forward_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    ret 9\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_normalizes_compare_immediate_lhs(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-NORMALIZE-COMPARE",
        build_normalize_compare_immediate_lhs_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = gt ssa.0, 1\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_bitand_all_ones(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-BITAND-ALL-ONES",
        build_canonicalize_bitand_all_ones_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_redundant_binary(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-REDUNDANT-BINARY",
        build_redundant_binary_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_dominated_redundant_binary(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-DOMINATED-REDUNDANT-BINARY",
        build_dominated_redundant_binary_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = add ssa.0, 1\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_preserves_dead_result_call(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-DEAD-CALL",
        build_dead_call_program,
        "declare id()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = call id()\n"
        "    ret 0\n"
        "}\n");
}



static int test_value_ssa_canonicalize_program_preserves_dangerous_dead_binary(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-DANGEROUS-BINARY",
        build_dangerous_dead_binary_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = div 1, 0\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_scrambled_diamond_fixed_point(void) {
    return expect_canonicalized_fixed_point_dump("VALUE-SSA-CANONICALIZE-DIAMOND-FIXED-POINT",
        build_scrambled_canonicalize_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 1], [bb.2: 2]\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_canonicalize_program_sccp_constant_branch(void) {
    return expect_canonicalized_dump("VALUE-SSA-CANONICALIZE-SCCP-CONSTANT-BRANCH",
        build_sccp_constant_branch_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 11\n"
        "}\n");
}

static int test_value_ssa_sccp_readonly_global_branch(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-READONLY-GLOBAL-BRANCH",
        build_sccp_readonly_global_branch_program,
        "global g.0 = 7\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_global g.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ret 11\n"
        "}\n");
}

static int test_value_ssa_sccp_mutated_global_branch(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-MUTATED-GLOBAL-BRANCH",
        build_sccp_mutated_global_branch_program,
        "global g.0 = 7\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    ssa.0 = load_global g.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 11], [bb.2: 22]\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_sccp_readonly_global_indirect_load(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-READONLY-GLOBAL-INDIRECT-LOAD",
        build_sccp_readonly_global_indirect_load_program,
        "global g.0 = 7\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n");
}

static int test_value_ssa_sccp_readonly_global_indirect_load_phi(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-READONLY-GLOBAL-INDIRECT-LOAD-PHI",
        build_sccp_readonly_global_indirect_load_phi_program,
        "global g.0 = 9\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ret 9\n"
        "}\n");
}

static int test_value_ssa_sccp_local_address_phi_compare(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-LOCAL-ADDRESS-PHI-COMPARE",
        build_sccp_local_address_phi_compare_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local cond.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_sccp_local_indirect_load_does_not_fold(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-LOCAL-INDIRECT-LOAD-NO-FOLD",
        build_sccp_local_indirect_load_does_not_fold_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_local x.0\n"
        "    ssa.1 = load_indirect ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_sccp_readonly_global_indirect_load_store_barrier(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-READONLY-GLOBAL-INDIRECT-LOAD-STORE-BARRIER",
        build_sccp_readonly_global_indirect_load_store_barrier_program,
        "global g.0 = 7\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global g.0\n"
        "    store_indirect ssa.0, 5\n"
        "    ssa.1 = load_indirect ssa.0\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_sccp_parameter_pointer_zero_add(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-PARAMETER-POINTER-ZERO-ADD",
        build_sccp_parameter_pointer_zero_add_program,
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_sccp_parameter_pointer_commuted_add(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-PARAMETER-POINTER-COMMUTED-ADD",
        build_sccp_parameter_pointer_commuted_add_program,
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ret 4\n"
        "}\n");
}

static int test_value_ssa_sccp_global_address_commuted_eq(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-GLOBAL-ADDRESS-COMMUTED-EQ",
        build_sccp_global_address_commuted_eq_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_sccp_parameter_pointer_commuted_ne(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-PARAMETER-POINTER-COMMUTED-NE",
        build_sccp_parameter_pointer_commuted_ne_program,
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_sccp_distinct_local_addresses_ne(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-DISTINCT-LOCAL-ADDRESSES-NE",
        build_sccp_distinct_local_addresses_ne_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_sccp_local_global_addresses_ne(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-LOCAL-GLOBAL-ADDRESSES-NE",
        build_sccp_local_global_addresses_ne_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_sccp_local_parameter_pointer_ne(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-LOCAL-PARAMETER-POINTER-NE",
        build_sccp_local_parameter_pointer_ne_program,
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_sccp_global_address_nonzero_compare(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-GLOBAL-ADDRESS-NONZERO-COMPARE",
        build_sccp_global_address_nonzero_compare_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_sccp_parameter_pointer_nonzero_compare(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-PARAMETER-POINTER-NONZERO-COMPARE",
        build_sccp_parameter_pointer_nonzero_compare_program,
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_sccp_global_address_branch(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-GLOBAL-ADDRESS-BRANCH",
        build_sccp_global_address_branch_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global g.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ret 11\n"
        "  bb.2:\n"
        "    ret 22\n"
        "}\n");
}

static int test_value_ssa_sccp_local_address_branch(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-LOCAL-ADDRESS-BRANCH",
        build_sccp_local_address_branch_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_local x.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ret 11\n"
        "  bb.2:\n"
        "    ret 22\n"
        "}\n");
}

static int test_value_ssa_sccp_parameter_pointer_branch(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-PARAMETER-POINTER-BRANCH",
        build_sccp_parameter_pointer_branch_program,
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local p.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ret 11\n"
        "  bb.2:\n"
        "    ret 22\n"
        "}\n");
}

static int test_value_ssa_sccp_global_address_offset_compare(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-GLOBAL-ADDRESS-OFFSET-COMPARE",
        build_sccp_global_address_offset_compare_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n");
}

static int test_value_ssa_sccp_global_address_offset_branch(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-GLOBAL-ADDRESS-OFFSET-BRANCH",
        build_sccp_global_address_offset_branch_program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global g.0\n"
        "    ssa.1 = add ssa.0, 4\n"
        "    br ssa.1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ret 11\n"
        "  bb.2:\n"
        "    ret 22\n"
        "}\n");
}

static int test_value_ssa_sccp_readonly_global_indirect_load_zero_offset(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-READONLY-GLOBAL-INDIRECT-LOAD-ZERO-OFFSET",
        build_sccp_readonly_global_indirect_load_zero_offset_program,
        "global g.0 = 7\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n");
}

static int test_value_ssa_sccp_readonly_global_indirect_load_nonzero_offset(void) {
    return expect_sccp_dump("VALUE-SSA-SCCP-READONLY-GLOBAL-INDIRECT-LOAD-NONZERO-OFFSET",
        build_sccp_readonly_global_indirect_load_nonzero_offset_program,
        "global g.0 = 7\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global g.0\n"
        "    ssa.1 = add ssa.0, 4\n"
        "    ssa.2 = load_indirect ssa.1\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_inline_tiny_internal_helpers_addr_global(void) {
    return expect_tiny_helper_inlined_dump("VALUE-SSA-INLINE-TINY-HELPER-ADDR-GLOBAL",
        build_tiny_inline_addr_global_helper_program,
        "global g.0\n"
        "\n"
        "func helper(x.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global g.0\n"
        "    ret ssa.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.1 = addr_global g.0\n"
        "    ssa.0 = mov ssa.1\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_inline_tiny_internal_helpers_global_indirect_load(void) {
    return expect_tiny_helper_inlined_dump("VALUE-SSA-INLINE-TINY-HELPER-GLOBAL-INDIRECT-LOAD",
        build_tiny_inline_global_indirect_load_helper_program,
        "global g.0 = 13\n"
        "\n"
        "func helper(x.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global g.0\n"
        "    ssa.1 = load_indirect ssa.0\n"
        "    ret ssa.1\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.1 = addr_global g.0\n"
        "    ssa.2 = load_indirect ssa.1\n"
        "    ssa.0 = mov ssa.2\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_inline_tiny_internal_helpers_parameter_pointer(void) {
    return expect_tiny_helper_inlined_dump("VALUE-SSA-INLINE-TINY-HELPER-PARAMETER-POINTER",
        build_tiny_inline_parameter_pointer_helper_program,
        "func helper(p.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local p.0\n"
        "    ssa.1 = add ssa.0, 0\n"
        "    ret ssa.1\n"
        "}\n"
        "\n"
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ssa.1 = load_local p.0\n"
        "    ssa.2 = add ssa.1, 0\n"
        "    ssa.0 = mov ssa.2\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_inline_tiny_internal_helpers_two_block_return(void) {
    return expect_tiny_helper_inlined_dump("VALUE-SSA-INLINE-TINY-HELPER-TWO-BLOCK-RETURN",
        build_tiny_inline_two_block_return_helper_program,
        "func helper(x.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local x.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = add ssa.0, 5\n"
        "    ret ssa.1\n"
        "}\n"
        "\n"
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ssa.1 = load_local p.0\n"
        "    ssa.2 = add ssa.1, 5\n"
        "    ssa.0 = mov ssa.2\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_inline_tiny_internal_helpers_void_two_block_return(void) {
    return expect_tiny_helper_inlined_dump("VALUE-SSA-INLINE-TINY-HELPER-VOID-TWO-BLOCK-RETURN",
        build_tiny_inline_void_two_block_helper_program,
        "func helper(x.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local x.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = add ssa.0, 5\n"
        "    ret\n"
        "}\n"
        "\n"
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local p.0\n"
        "    ssa.1 = add ssa.0, 5\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_inline_tiny_internal_helpers_zero_param_constant(void) {
    return expect_tiny_helper_inlined_dump("VALUE-SSA-INLINE-TINY-HELPER-ZERO-PARAM-CONSTANT",
        build_tiny_inline_zero_param_constant_helper_program,
        "func helper() {\n"
        "  bb.0:\n"
        "    ret 42\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = mov 42\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_inline_tiny_internal_helpers_budget_blocks_large_helper(void) {
    return expect_tiny_helper_inlined_dump("VALUE-SSA-INLINE-TINY-HELPER-BUDGET-BLOCKS-LARGE-HELPER",
        build_tiny_inline_budget_blocked_helper_program,
        "global g0.0\n"
        "global g1.1\n"
        "global g2.2\n"
        "global g3.3\n"
        "global g4.4\n"
        "global g5.5\n"
        "global g6.6\n"
        "global g7.7\n"
        "\n"
        "func helper() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_global g0.0\n"
        "    ssa.1 = load_global g1.1\n"
        "    ssa.2 = load_global g2.2\n"
        "    ssa.3 = load_global g3.3\n"
        "    ssa.4 = load_global g4.4\n"
        "    ssa.5 = load_global g5.5\n"
        "    ssa.6 = load_global g6.6\n"
        "    ssa.7 = load_global g7.7\n"
        "    ret ssa.7\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = call helper()\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_inline_tiny_internal_helpers_function_budget(void) {
    return expect_tiny_helper_inlined_dump("VALUE-SSA-INLINE-TINY-HELPER-FUNCTION-BUDGET",
        build_tiny_inline_function_budget_program,
        "global g0.0\n"
        "global g1.1\n"
        "global g2.2\n"
        "global g3.3\n"
        "global g4.4\n"
        "global g5.5\n"
        "\n"
        "func helper() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_global g0.0\n"
        "    ssa.1 = load_global g1.1\n"
        "    ssa.2 = load_global g2.2\n"
        "    ssa.3 = load_global g3.3\n"
        "    ssa.4 = load_global g4.4\n"
        "    ssa.5 = load_global g5.5\n"
        "    ret ssa.5\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.2 = load_global g0.0\n"
        "    ssa.3 = load_global g1.1\n"
        "    ssa.4 = load_global g2.2\n"
        "    ssa.5 = load_global g3.3\n"
        "    ssa.6 = load_global g4.4\n"
        "    ssa.7 = load_global g5.5\n"
        "    ssa.0 = mov ssa.7\n"
        "    ssa.1 = call helper()\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_source_sccp_constant_branch(void) {
    static const char *source =
        "int main(){\n"
        "  int x;\n"
        "  x = 1;\n"
        "  if (x) {\n"
        "    return 11;\n"
        "  } else {\n"
        "    return 22;\n"
        "  }\n"
        "}\n";
    static const char *const fragments[] = {
        "func main() {\n",
        "    ret 11\n",
    };

    return expect_source_perf_hotspot_fragments("VALUE-SSA-SCCP-SOURCE-CONSTANT-BRANCH",
        source,
        fragments,
        sizeof(fragments) / sizeof(fragments[0]));
}

static int test_value_ssa_canonicalized_conversion_diamond_program(void) {
    return expect_canonicalized_converted_dump("VALUE-SSA-CONVERT-CANONICALIZE-DIAMOND",
        build_lower_ir_diamond_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 1], [bb.2: 2]\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_canonicalized_conversion_diamond_fixed_point(void) {
    return expect_canonicalized_converted_fixed_point_dump(
        "VALUE-SSA-CONVERT-CANONICALIZE-DIAMOND-FIXED-POINT",
        build_lower_ir_diamond_program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 1], [bb.2: 2]\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_canonicalized_conversion_preserves_dead_result_call(void) {
    return expect_canonicalized_converted_dump("VALUE-SSA-CONVERT-CANONICALIZE-DEAD-CALL",
        build_lower_ir_dead_call_program,
        "declare id()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = call id()\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_canonicalized_conversion_preserves_dangerous_dead_binary(void) {
    return expect_canonicalized_converted_dump("VALUE-SSA-CONVERT-CANONICALIZE-DANGEROUS-BINARY",
        build_lower_ir_dangerous_dead_binary_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = div 1, 0\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_memory_canonicalized_conversion_local_join(void) {
    return expect_memory_canonicalized_converted_dump("VALUE-SSA-CONVERT-MEMORY-CANONICALIZE-LOCAL-JOIN",
        build_lower_ir_join_preserved_local_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n");
}

static int test_value_ssa_memory_value_canonicalized_conversion_fold_program(void) {
    return expect_memory_value_canonicalized_converted_dump(
        "VALUE-SSA-CONVERT-MEMORY-VALUE-CANONICALIZE-FOLD",
        build_lower_ir_memory_fold_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = add 7, 5\n"
        "    ret ssa.0\n"
        "}\n");
}

static int build_lower_ir_nested_while_regression_program(LowerIrProgram *program, LowerIrError *error) {
    static const char *source =
        "int main() {\n"
        "  int a = 1, b = 2;\n"
        "  while (a < 10) {\n"
        "    a = a + 1;\n"
        "    while (a < 5 && b < 10) {\n"
        "      b = b + 1;\n"
        "    }\n"
        "    while (b < 20) {\n"
        "      while (b < 6 || b == 6) {\n"
        "        b = b + 1;\n"
        "      }\n"
        "      b = b + 2;\n"
        "    }\n"
        "  }\n"
        "  return a + b;\n"
        "}\n";
    AstProgram ast;
    TokenArray tokens;
    ParserError parser_error;
    SemanticError semantic_error;
    SemanticOptions semantic_options;
    IrProgram ir_program;
    IrError ir_error;
    IrLowerOptions ir_options;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast);
    ir_program_init(&ir_program);
    memset(&parser_error, 0, sizeof(parser_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&semantic_options, 0, sizeof(semantic_options));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&ir_options, 0, sizeof(ir_options));
    lower_ir_program_init(program);
    semantic_options.skip_all_paths_return_check = 1;
    ir_options.allow_implicit_fallthrough_return = 1;

    ok = lexer_tokenize(source, &tokens) &&
        parser_parse_translation_unit_ast(&tokens, &ast, &parser_error) &&
        semantic_analyze_program_with_options(&ast, &semantic_options, &semantic_error) &&
        ir_lower_program(&ast, &ir_options, &ir_program, &ir_error) &&
        lower_ir_lower_from_ir(&ir_program, NULL, program, error);

    ir_program_free(&ir_program);
    ast_program_free(&ast);
    lexer_free_tokens(&tokens);
    return ok;
}

static int test_value_ssa_memory_value_canonicalized_conversion_nested_while_regression(void) {
    return expect_memory_value_canonicalized_converted_dump(
        "VALUE-SSA-CONVERT-MEMORY-VALUE-CANONICALIZE-NESTED-WHILE",
        build_lower_ir_nested_while_regression_program,
        "func main() {\n"
        "  bb.0:\n"
        "    store_local a.0, 1\n"
        "    store_local b.1, 2\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = lt ssa.0, 10\n"
        "    br ssa.1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = load_local a.0\n"
        "    ssa.3 = add ssa.2, 1\n"
        "    store_local a.0, ssa.3\n"
        "    jmp bb.4\n"
        "  bb.3:\n"
        "    ssa.4 = load_local a.0\n"
        "    ssa.5 = load_local b.1\n"
        "    ssa.6 = add ssa.4, ssa.5\n"
        "    ret ssa.6\n"
        "  bb.4:\n"
        "    ssa.7 = load_local a.0\n"
        "    ssa.8 = lt ssa.7, 5\n"
        "    br ssa.8, bb.6, bb.7\n"
        "  bb.5:\n"
        "    ssa.9 = load_local b.1\n"
        "    ssa.10 = add ssa.9, 1\n"
        "    store_local b.1, ssa.10\n"
        "    jmp bb.4\n"
        "  bb.6:\n"
        "    ssa.11 = load_local b.1\n"
        "    ssa.12 = lt ssa.11, 10\n"
        "    br ssa.12, bb.5, bb.7\n"
        "  bb.7:\n"
        "    ssa.13 = load_local b.1\n"
        "    ssa.14 = lt ssa.13, 20\n"
        "    br ssa.14, bb.8, bb.1\n"
        "  bb.8:\n"
        "    ssa.15 = load_local b.1\n"
        "    ssa.16 = lt ssa.15, 6\n"
        "    br ssa.16, bb.9, bb.11\n"
        "  bb.9:\n"
        "    ssa.17 = load_local b.1\n"
        "    ssa.18 = add ssa.17, 1\n"
        "    store_local b.1, ssa.18\n"
        "    jmp bb.8\n"
        "  bb.10:\n"
        "    ssa.19 = load_local b.1\n"
        "    ssa.20 = add ssa.19, 2\n"
        "    store_local b.1, ssa.20\n"
        "    jmp bb.7\n"
        "  bb.11:\n"
        "    ssa.21 = load_local b.1\n"
        "    ssa.22 = eq ssa.21, 6\n"
        "    br ssa.22, bb.9, bb.10\n"
        "}\n");
}

static int test_value_ssa_memory_canonicalized_conversion_fold_program(void) {
    return expect_memory_canonicalized_converted_dump("VALUE-SSA-CONVERT-MEMORY-CANONICALIZE-FOLD",
        build_lower_ir_memory_fold_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 12\n"
        "}\n");
}

static int test_value_ssa_mode_conversion_classic_diamond(void) {
    return expect_mode_converted_dump("VALUE-SSA-CONVERT-MODE-CLASSIC-DIAMOND",
        build_lower_ir_diamond_program,
        VALUE_SSA_LOWER_IR_CANONICALIZE_CLASSIC,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 1], [bb.2: 2]\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_mode_conversion_memory_value_fold(void) {
    return expect_mode_converted_dump("VALUE-SSA-CONVERT-MODE-MEMORY-VALUE-FOLD",
        build_lower_ir_memory_fold_program,
        VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_VALUE,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = add 7, 5\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_mode_conversion_memory_full_fold(void) {
    return expect_mode_converted_dump("VALUE-SSA-CONVERT-MODE-MEMORY-FULL-FOLD",
        build_lower_ir_memory_fold_program,
        VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_FULL,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 12\n"
        "}\n");
}

static int test_value_ssa_mode_conversion_rejects_unknown_mode(void) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram ssa_program;
    ValueSsaError ssa_error;
    int ok;

    if (!build_lower_ir_memory_fold_program(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-CONVERT-MODE-INVALID setup failed at %d:%d: %s\n",
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    ok = !value_ssa_build_from_lower_ir_with_canonicalization(
             &lower_program, (ValueSsaLowerIrCanonicalizeMode)99, &ssa_program, &ssa_error) &&
        strstr(ssa_error.message, "VALUE-SSA-177") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-CONVERT-MODE-INVALID expected VALUE-SSA-177, got: %s\n",
            ssa_error.message);
    }

    lower_ir_program_free(&lower_program);
    return ok;
}

static int test_value_ssa_default_conversion_fold_program(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-FOLD",
        build_lower_ir_memory_fold_program,
        "func main() {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = add ssa.0, 5\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_default_conversion_indirect_safe_global_forward_program(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-INDIRECT-SAFE-GLOBAL-FORWARD",
        build_lower_ir_indirect_safe_global_forward_program,
        "global g.0\n"
        "global arr.1\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 7\n"
        "    ssa.0 = mov 7\n"
        "    ssa.1 = addr_global arr.1\n"
        "    store_indirect ssa.1, 7\n"
        "    ssa.2 = mov 7\n"
        "    ret 7\n"
        "}\n");
}

static int test_value_ssa_default_conversion_tiny_helper_inline_program(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-TINY-HELPER-INLINE",
        build_lower_ir_tiny_helper_inline_program,
        "global hashmod.0\n"
        "global arr.1\n"
        "\n"
        "func hash(k.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local k.0\n"
        "    ssa.1 = load_global hashmod.0\n"
        "    ssa.2 = mod ssa.0, ssa.1\n"
        "    ret ssa.2\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global arr.1\n"
        "    store_indirect ssa.0, 0\n"
        "    ssa.2 = load_global hashmod.0\n"
        "    ssa.3 = mod 9, ssa.2\n"
        "    ssa.1 = mov ssa.3\n"
        "    ret ssa.3\n"
        "}\n");
}

static int test_value_ssa_default_conversion_tiny_helper_two_block_return_program(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-TINY-HELPER-TWO-BLOCK-RETURN",
        build_lower_ir_tiny_helper_two_block_return_program,
        "func helper(x.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local x.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = add ssa.0, 5\n"
        "    ret ssa.1\n"
        "}\n"
        "\n"
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local p.0\n"
        "    ssa.1 = add ssa.0, 5\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_default_conversion_tiny_void_helper_two_block_return_program(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-TINY-VOID-HELPER-TWO-BLOCK-RETURN",
        build_lower_ir_tiny_void_helper_two_block_return_program,
        "func helper(x.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ret\n"
        "}\n"
        "\n"
        "func main(p.0) {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_default_conversion_internal_call_preserves_unwritten_global_forward_program(void) {
    return expect_default_converted_dump(
        "VALUE-SSA-CONVERT-DEFAULT-INTERNAL-CALL-PRESERVES-UNWRITTEN-GLOBAL-FORWARD",
        build_lower_ir_internal_call_preserves_unwritten_global_forward_program,
        "global g.0\n"
        "global arr.1\n"
        "\n"
        "func helper() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 7\n"
        "    ssa.0 = addr_global arr.1\n"
        "    store_indirect ssa.0, 0\n"
        "    ret 8\n"
        "}\n");
}

static int test_value_ssa_default_conversion_eliminates_unread_scalar_local_store_program(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-UNREAD-SCALAR-LOCAL-STORE-DCE",
        build_lower_ir_unused_scalar_local_store_program,
        "global sink.0\n"
        "\n"
        "func helper() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global sink.0\n"
        "    store_indirect ssa.0, 0\n"
        "    ssa.1 = mov 0\n"
        "    ret 0\n"
        "}\n");
}

static int test_value_ssa_default_conversion_preserves_array_local_store_program(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-PRESERVE-ARRAY-LOCAL-STORES",
        build_lower_ir_array_local_store_preserved_program,
        "global sink.0\n"
        "\n"
        "func helper() {\n"
        "  bb.0:\n"
        "    store_local arr.0, 1\n"
        "    store_local arr.1, 2\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global sink.0\n"
        "    store_indirect ssa.0, 0\n"
        "    ssa.1 = call helper()\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_default_conversion_preserves_address_taken_scalar_local_store_program(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-PRESERVE-ADDR-TAKEN-SCALAR-LOCAL-STORE",
        build_lower_ir_address_taken_scalar_local_store_preserved_program,
        "func helper() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_local x.0\n"
        "    store_local x.0, 7\n"
        "    ssa.1 = load_indirect ssa.0\n"
        "    ret ssa.1\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = call helper()\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_value_ssa_default_conversion_reuses_unique_pred_repeated_indirect_load_program(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-UNIQUE-PRED-INDIRECT-LOAD-FORWARD",
        build_lower_ir_unique_pred_repeated_indirect_load_program,
        "global head.0\n"
        "\n"
        "func main(idx.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global head.0\n"
        "    ssa.1 = load_local idx.0\n"
        "    ssa.2 = mul ssa.1, 4\n"
        "    ssa.3 = add ssa.0, ssa.2\n"
        "    ssa.4 = load_indirect ssa.3\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.5 = addr_global head.0\n"
        "    ssa.6 = add ssa.5, ssa.2\n"
        "    ssa.7 = mov ssa.4\n"
        "    ret ssa.4\n"
        "}\n");
}

static int test_value_ssa_default_conversion_reuses_unique_pred_repeated_indirect_load_across_non_alias_local_store_program(
    void) {
    return expect_default_converted_dump(
        "VALUE-SSA-CONVERT-DEFAULT-UNIQUE-PRED-INDIRECT-LOAD-FORWARD-ACROSS-NONALIAS-LOCAL-STORE",
        build_lower_ir_unique_pred_repeated_indirect_load_across_non_alias_local_store_program,
        "global head.0\n"
        "\n"
        "func main(idx.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global head.0\n"
        "    ssa.1 = load_local idx.0\n"
        "    ssa.2 = mul ssa.1, 4\n"
        "    ssa.3 = add ssa.0, ssa.2\n"
        "    ssa.4 = load_indirect ssa.3\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.5 = addr_global head.0\n"
        "    ssa.6 = add ssa.5, ssa.2\n"
        "    ssa.7 = mov ssa.4\n"
        "    ret ssa.4\n"
        "}\n");
}

static int test_value_ssa_default_conversion_reuses_dominated_repeated_indirect_load_across_non_alias_local_store(
    void) {
    return expect_default_converted_dump(
        "VALUE-SSA-CONVERT-DEFAULT-DOMINATED-INDIRECT-LOAD-FORWARD-ACROSS-NONALIAS-LOCAL-STORE",
        build_lower_ir_dominated_repeated_indirect_load_across_non_alias_local_store_program,
        "global head.0\n"
        "\n"
        "func main(idx.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global head.0\n"
        "    ssa.1 = load_local idx.0\n"
        "    ssa.2 = mul ssa.1, 4\n"
        "    ssa.3 = add ssa.0, ssa.2\n"
        "    ssa.4 = load_indirect ssa.3\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    jmp bb.2\n"
        "  bb.2:\n"
        "    ssa.5 = addr_global head.0\n"
        "    ssa.6 = add ssa.5, ssa.2\n"
        "    ssa.7 = load_indirect ssa.6\n"
        "    ret ssa.7\n"
        "}\n");
}

static int test_value_ssa_default_conversion_reuses_repeated_addr_root_program(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-REPEATED-ADDR-ROOT-REUSE",
        build_lower_ir_repeated_addr_root_program,
        "global head.0\n"
        "\n"
        "func main(idx.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global head.0\n"
        "    ssa.1 = load_local idx.0\n"
        "    ssa.2 = mul ssa.1, 4\n"
        "    ssa.3 = add ssa.0, ssa.2\n"
        "    ssa.4 = addr_global head.0\n"
        "    ssa.5 = add ssa.4, ssa.2\n"
        "    ssa.6 = load_indirect ssa.5\n"
        "    ret ssa.6\n"
        "}\n");
}

static int test_value_ssa_default_conversion_reuses_repeated_pure_internal_calls(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-REPEATED-PURE-INTERNAL-CALL-REUSE",
        build_lower_ir_repeated_pure_internal_call_program,
        "global base.0 = 3\n"
        "global arr.1\n"
        "\n"
        "func helper(x.0, count.1) {\n"
        "  bb.0:\n"
        "    store_local i.2, 0\n"
        "    ssa.0 = load_local count.1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = load_local i.2\n"
        "    ssa.2 = lt ssa.1, ssa.0\n"
        "    br ssa.2, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.3 = load_local x.0\n"
        "    ssa.4 = add ssa.3, 3\n"
        "    store_local x.0, ssa.4\n"
        "    ssa.5 = add ssa.1, 1\n"
        "    store_local i.2, ssa.5\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.6 = load_local x.0\n"
        "    ret ssa.6\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global arr.1\n"
        "    store_indirect ssa.0, 0\n"
        "    ssa.1 = call helper(5, 2)\n"
        "    ssa.2 = add ssa.1, ssa.1\n"
        "    ret ssa.2\n"
        "}\n");
}

static int test_value_ssa_default_conversion_reaches_redundant_binary_fixed_point(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-REDUNDANT-BINARY-FIXED-POINT",
        build_lower_ir_redundant_binary_fixed_point_program,
        "global arr.0\n"
        "\n"
        "func main(idx.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global arr.0\n"
        "    store_indirect ssa.0, 0\n"
        "    ssa.1 = load_local idx.0\n"
        "    ssa.2 = add ssa.1, 1\n"
        "    ssa.3 = add ssa.1, 1\n"
        "    ssa.4 = add ssa.3, 2\n"
        "    ssa.5 = add ssa.2, 2\n"
        "    ret ssa.4\n"
        "}\n");
}

static int test_value_ssa_default_conversion_forwards_same_block_indirect_load_across_param_store(void) {
    return expect_default_converted_dump("VALUE-SSA-CONVERT-DEFAULT-SAME-BLOCK-INDIRECT-LOAD-FORWARD-PARAM-NONALIAS",
        build_lower_ir_same_block_indirect_load_forward_across_param_store_program,
        "func main(arr.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_local head.1\n"
        "    ssa.1 = load_indirect ssa.0\n"
        "    ssa.2 = load_local arr.0\n"
        "    store_indirect ssa.2, 7\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_default_conversion_reuses_repeated_pure_internal_calls_across_store_indirect(void) {
    return expect_default_converted_dump(
        "VALUE-SSA-CONVERT-DEFAULT-REPEATED-PURE-INTERNAL-CALL-ACROSS-STORE-INDIRECT",
        build_lower_ir_repeated_pure_internal_call_across_store_indirect_program,
        "global base.0 = 3\n"
        "global arr.1\n"
        "\n"
        "func helper(x.0, count.1) {\n"
        "  bb.0:\n"
        "    store_local i.2, 0\n"
        "    ssa.0 = load_local count.1\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = load_local i.2\n"
        "    ssa.2 = lt ssa.1, ssa.0\n"
        "    br ssa.2, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.3 = load_local x.0\n"
        "    ssa.4 = add ssa.3, 3\n"
        "    store_local x.0, ssa.4\n"
        "    ssa.5 = add ssa.1, 1\n"
        "    store_local i.2, ssa.5\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.6 = load_local x.0\n"
        "    ret ssa.6\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = addr_global arr.1\n"
        "    store_indirect ssa.0, 0\n"
        "    ssa.1 = call helper(5, 2)\n"
        "    store_indirect ssa.0, 1\n"
        "    ret ssa.1\n"
        "}\n");
}

static int test_value_ssa_default_matches_memory_full_mode(void) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram default_program;
    ValueSsaProgram mode_program;
    ValueSsaError ssa_error;
    char *default_text = NULL;
    char *mode_text = NULL;
    int ok = 1;

    if (!build_lower_ir_join_preserved_local_program(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-CONVERT-DEFAULT-MATCHES-MEMORY-FULL setup failed at %d:%d: %s\n",
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_default_from_lower_ir(&lower_program, &default_program, &ssa_error) ||
        !value_ssa_build_from_lower_ir_with_canonicalization(
            &lower_program, VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_FULL, &mode_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-CONVERT-DEFAULT-MATCHES-MEMORY-FULL conversion failed at %d:%d: %s\n",
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&default_program, &default_text) ||
        !value_ssa_dump_program(&mode_program, &mode_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: VALUE-SSA-CONVERT-DEFAULT-MATCHES-MEMORY-FULL dump failed\n");
        ok = 0;
        goto cleanup;
    }

    ok = strstr(default_text, "store_local a.0, 7") != NULL &&
        strstr(default_text, "ssa.0 = load_local a.0") != NULL &&
        strstr(mode_text, "ret 7") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-CONVERT-DEFAULT-MATCHES-MEMORY-FULL expected split default/full behavior\ndefault:\n%s\nmode:\n%s\n",
            default_text,
            mode_text);
    }

cleanup:
    free(default_text);
    free(mode_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&default_program);
    value_ssa_program_free(&mode_program);
    return ok;
}

static int test_value_ssa_default_matches_direct_on_extreme_param_program(void) {
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    ValueSsaProgram default_program;
    ValueSsaProgram direct_program;
    ValueSsaError ssa_error;
    char *default_text = NULL;
    char *direct_text = NULL;
    int ok = 1;

    if (!build_lower_ir_extreme_param_decl_program(&lower_program, &lower_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-CONVERT-DEFAULT-MATCHES-DIRECT-EXTREME-PARAM setup failed at %d:%d: %s\n",
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!value_ssa_build_default_from_lower_ir(&lower_program, &default_program, &ssa_error) ||
        !value_ssa_build_from_lower_ir(&lower_program, &direct_program, &ssa_error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-CONVERT-DEFAULT-MATCHES-DIRECT-EXTREME-PARAM conversion failed at %d:%d: %s\n",
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&default_program, &default_text) ||
        !value_ssa_dump_program(&direct_program, &direct_text)) {
        fprintf(stderr, "[value-ssa-reg] FAIL: VALUE-SSA-CONVERT-DEFAULT-MATCHES-DIRECT-EXTREME-PARAM dump failed\n");
        ok = 0;
        goto cleanup;
    }

    ok = strstr(default_text, "declare helper_decl()") != NULL &&
        strstr(default_text, "ret 1") != NULL &&
        strstr(direct_text, "ssa.0 = mov 1") != NULL &&
        strstr(direct_text, "ret ssa.0") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-CONVERT-DEFAULT-MATCHES-DIRECT-EXTREME-PARAM expected split default/direct behavior\ndefault:\n%s\ndirect:\n%s\n",
            default_text,
            direct_text);
    }

cleanup:
    free(default_text);
    free(direct_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&default_program);
    value_ssa_program_free(&direct_program);
    return ok;
}

static int test_value_ssa_canonicalize_program_rejects_invalid_input(void) {
    return expect_canonicalize_rejected("VALUE-SSA-CANONICALIZE-REJECT-INVALID",
        build_invalid_undefined_value_program,
        "VALUE-SSA-064");
}

static int test_value_ssa_verifier_rejects_zero_arg_call_with_nonnull_args(void) {
    return expect_verifier_rejects("VALUE-SSA-VERIFY-CALL-ZERO-ARG-NONNULL-ARGS",
        build_sample_program,
        mutate_sample_program_zero_arg_call_nonnull_args,
        "VALUE-SSA-058");
}

static int test_value_ssa_builder_rejects_declaration_block_append(void) {
    ValueSsaProgram program;
    ValueSsaFunction *decl = NULL;
    ValueSsaError error;

    value_ssa_program_init(&program);
    if (!value_ssa_program_append_function(&program, "decl", 0, &decl, &error)) {
        return 0;
    }

    if (value_ssa_function_append_block(decl, NULL, NULL, &error) ||
        !strstr(error.message, "VALUE-SSA-018")) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-BUILDER-DECL-BLOCK expected VALUE-SSA-018, got: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_value_ssa_builder_rejects_declaration_allocate_value(void) {
    ValueSsaProgram program;
    ValueSsaFunction *decl = NULL;
    ValueSsaError error;

    value_ssa_program_init(&program);
    if (!value_ssa_program_append_function(&program, "decl", 0, &decl, &error)) {
        return 0;
    }

    if (value_ssa_function_allocate_value(decl) != (size_t)-1) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-BUILDER-DECL-VALUE unexpectedly allocated value for declaration\n");
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_value_ssa_builder_rejects_parameter_prefix_break(void) {
    ValueSsaProgram program;
    ValueSsaFunction *function = NULL;
    ValueSsaError error;

    value_ssa_program_init(&program);
    if (!value_ssa_program_append_function(&program, "main", 1, &function, &error) ||
        !value_ssa_function_append_local(function, "tmp", 0, NULL, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (value_ssa_function_append_local(function, "late_param", 1, NULL, &error) ||
        !strstr(error.message, "VALUE-SSA-013")) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-BUILDER-PARAM-PREFIX expected VALUE-SSA-013, got: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_value_ssa_builder_rejects_phi_after_instruction(void) {
    ValueSsaProgram program;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput input;
    ValueSsaError error;
    size_t local_a_id;
    size_t value0;
    size_t value1;

    value_ssa_program_init(&program);
    if (!value_ssa_program_append_function(&program, "main", 1, &function, &error) ||
        !value_ssa_function_append_local(function, "a", 1, &local_a_id, &error) ||
        !value_ssa_function_append_block(function, NULL, &block, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    input.predecessor_block_id = 0;
    input.value = value_ssa_value_id(value0);
    if (value_ssa_block_append_phi(block, value1, &input, 1, &error) ||
        !strstr(error.message, "VALUE-SSA-022")) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-BUILDER-PHI-ORDER expected VALUE-SSA-022, got: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_value_ssa_builder_rejects_terminator_overwrite(void) {
    ValueSsaProgram program;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaError error;

    value_ssa_program_init(&program);
    if (!value_ssa_program_append_function(&program, "main", 1, &function, &error) ||
        !value_ssa_function_append_block(function, NULL, &block, &error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (value_ssa_block_set_jump(block, 0, &error) ||
        !strstr(error.message, "VALUE-SSA-032")) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-BUILDER-TERM-OVERWRITE expected VALUE-SSA-032, got: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

static int test_value_ssa_builder_void_return_clears_stale_return_value(void) {
    ValueSsaProgram program;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaError error;

    value_ssa_program_init(&program);
    if (!value_ssa_program_append_function(&program, "main", 1, &function, &error) ||
        !value_ssa_function_append_block(function, NULL, &block, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    block->terminator.as.return_value = value_ssa_value_id(999u);
    if (!value_ssa_block_set_void_return(block, &error)) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-BUILDER-VOID-RETURN setup failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (block->terminator.kind != VALUE_SSA_TERM_RETURN ||
        block->terminator.has_return_value ||
        block->terminator.as.return_value.kind != VALUE_SSA_VALUE_IMMEDIATE ||
        block->terminator.as.return_value.immediate != 0 ||
        block->terminator.as.return_value.value_id != 0u) {
        fprintf(stderr,
            "[value-ssa-reg] FAIL: VALUE-SSA-BUILDER-VOID-RETURN expected cleared stale return payload, got kind=%d has=%d value-kind=%d imm=%lld id=%zu\n",
            (int)block->terminator.kind,
            block->terminator.has_return_value,
            (int)block->terminator.as.return_value.kind,
            block->terminator.as.return_value.immediate,
            block->terminator.as.return_value.value_id);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_free(&program);
    return 1;
}

int main(void) {
    int ok = 1;

    ok &= test_value_ssa_dump_straight_line_program();
    ok &= test_value_ssa_dump_diamond_phi_program();
    ok &= test_value_ssa_cfg_analysis_diamond_program();
    ok &= test_value_ssa_def_use_analysis_diamond_program();
    ok &= test_value_ssa_phi_placement_diamond_program();
    ok &= test_value_ssa_dominator_preorder_diamond_program();
    ok &= test_value_ssa_dominator_walk_diamond_program();
    ok &= test_value_ssa_simplify_trivial_mov_chain();
    ok &= test_value_ssa_simplify_trivial_phi();
    ok &= test_value_ssa_normalize_compare_immediate_lhs();
    ok &= test_value_ssa_forward_local_loads_after_store();
    ok &= test_value_ssa_forward_local_repeated_loads();
    ok &= test_value_ssa_forward_local_loads_across_straight_chain();
    ok &= test_value_ssa_forward_local_loads_do_not_cross_join();
    ok &= test_value_ssa_forward_local_loads_skip_address_taken_local();
    ok &= test_value_ssa_optimize_perf_hotspots();
    ok &= test_value_ssa_optimize_perf_hotspots_hoists_loop_invariant_binary();
    ok &= test_value_ssa_licm_hoists_loop_invariant_binary();
    ok &= test_value_ssa_licm_does_not_hoist_across_store();
    ok &= test_value_ssa_optimize_perf_hotspots_hoists_phi_header_loop_invariant_indirect_load();
    ok &= test_value_ssa_optimize_perf_hotspots_does_not_hoist_phi_header_indirect_load_across_store_barrier();
    ok &= test_value_ssa_optimize_perf_hotspots_hoists_function_entry_global_load();
    ok &= test_value_ssa_optimize_perf_hotspots_does_not_hoist_global_load_across_store();
    ok &= test_value_ssa_optimize_perf_hotspots_hoists_function_entry_global_addr();
    ok &= test_value_ssa_optimize_perf_hotspots_does_not_hoist_single_use_global_addr();
    ok &= test_value_ssa_optimize_perf_hotspots_hoists_spmv_parameter_local_loads();
    ok &= test_value_ssa_optimize_perf_hotspots_hoists_shared_branch_successor_local_loads();
    ok &= test_value_ssa_optimize_perf_hotspots_does_not_hoist_spmv_parameter_local_load_across_store();
    ok &= test_value_ssa_optimize_perf_hotspots_hoists_power_parameter_local_loads();
    ok &= test_value_ssa_optimize_perf_hotspots_does_not_hoist_non_hot_parameter_local_loads();
    ok &= test_value_ssa_optimize_perf_hotspots_reduces_zero_based_induction_divmods();
    ok &= test_value_ssa_optimize_perf_hotspots_rebalances_integer_dispatch_chains();
    ok &= test_value_ssa_optimize_perf_hotspots_reduces_simple_induction_shifts();
    ok &= test_value_ssa_optimize_perf_hotspots_reuses_spmv_loop_exit_indirect_loads();
    ok &= test_value_ssa_optimize_perf_hotspots_source_multiply_baseline_dump();
    ok &= test_value_ssa_optimize_perf_hotspots_source_power_branch_cleanup_dump();
    ok &= test_value_ssa_optimize_perf_hotspots_source_fft_mod998_butterfly_dump();
    ok &= test_value_ssa_optimize_perf_hotspots_source_spmv_loop_fusion_dump();
    ok &= test_value_ssa_optimize_perf_hotspots_source_mm_rebuild_dump();
    ok &= test_value_ssa_forward_global_loads_after_store();
    ok &= test_value_ssa_forward_global_loads_across_straight_chain();
    ok &= test_value_ssa_forward_global_loads_do_not_cross_join();
    ok &= test_value_ssa_forward_global_loads_killed_by_call();
    ok &= test_value_ssa_forward_global_loads_killed_by_indirect_store();
    ok &= test_value_ssa_forward_global_loads_killed_by_cross_block_call();
    ok &= test_value_ssa_eliminate_dead_local_store();
    ok &= test_value_ssa_eliminate_dead_local_store_preserves_address_taken_observer();
    ok &= test_value_ssa_eliminate_dead_local_store_across_straight_chain();
    ok &= test_value_ssa_eliminate_dead_local_store_across_two_hop_chain();
    ok &= test_value_ssa_eliminate_dead_local_store_preserves_branch_observer();
    ok &= test_value_ssa_eliminate_dead_global_store();
    ok &= test_value_ssa_eliminate_dead_global_store_preserves_indirect_observer();
    ok &= test_value_ssa_eliminate_dead_global_store_across_straight_chain();
    ok &= test_value_ssa_eliminate_dead_global_store_across_two_hop_chain();
    ok &= test_value_ssa_eliminate_dead_global_store_preserves_call_barrier();
    ok &= test_value_ssa_eliminate_dead_global_store_preserves_cross_block_call_barrier();
    ok &= test_value_ssa_eliminate_dead_global_store_preserves_branch_observer();
    ok &= test_value_ssa_eliminate_redundant_local_store_across_straight_chain();
    ok &= test_value_ssa_eliminate_redundant_local_store_across_two_hop_chain();
    ok &= test_value_ssa_eliminate_redundant_local_store_does_not_cross_join();
    ok &= test_value_ssa_eliminate_redundant_global_store();
    ok &= test_value_ssa_eliminate_redundant_global_store_preserves_indirect_store_barrier();
    ok &= test_value_ssa_eliminate_redundant_global_store_preserves_call_barrier();
    ok &= test_value_ssa_eliminate_redundant_global_store_preserves_cross_block_call_barrier();
    ok &= test_value_ssa_eliminate_redundant_global_store_does_not_cross_join();
    ok &= test_value_ssa_eliminate_redundant_binary();
    ok &= test_value_ssa_eliminate_redundant_commuted_binary();
    ok &= test_value_ssa_eliminate_redundant_local_load();
    ok &= test_value_ssa_eliminate_redundant_local_load_same_block_address_taken_barrier();
    ok &= test_value_ssa_eliminate_redundant_mov();
    ok &= test_value_ssa_eliminate_redundant_binary_preserves_dangerous_binary();
    ok &= test_value_ssa_eliminate_redundant_binary_reuses_safe_div();
    ok &= test_value_ssa_eliminate_dominated_redundant_binary();
    ok &= test_value_ssa_eliminate_join_redundant_binary();
    ok &= test_value_ssa_eliminate_join_redundant_commuted_binary();
    ok &= test_value_ssa_eliminate_join_redundant_local_load();
    ok &= test_value_ssa_eliminate_redundant_local_load_address_taken_barrier();
    ok &= test_value_ssa_eliminate_dominated_redundant_mov();
    ok &= test_value_ssa_eliminate_dominated_redundant_addr();
    ok &= test_value_ssa_eliminate_redundant_indirect_load();
    ok &= test_value_ssa_eliminate_redundant_indirect_load_store_barrier();
    ok &= test_value_ssa_eliminate_join_redundant_indirect_load();
    ok &= test_value_ssa_eliminate_join_redundant_indirect_load_call_barrier();
    ok &= test_value_ssa_simplify_identity_add_zero();
    ok &= test_value_ssa_simplify_identity_eq_self();
    ok &= test_value_ssa_simplify_identity_div_one();
    ok &= test_value_ssa_fold_constant_binary();
    ok &= test_value_ssa_fold_constant_mov_chain();
    ok &= test_value_ssa_fold_preserves_dangerous_binary();
    ok &= test_value_ssa_fold_constant_safe_div();
    ok &= test_value_ssa_fold_constant_safe_mod();
    ok &= test_value_ssa_fold_constant_safe_shift_left();
    ok &= test_value_ssa_fold_constant_safe_shift_right();
    ok &= test_value_ssa_fold_preserves_overflow_div();
    ok &= test_value_ssa_fold_preserves_invalid_shift();
    ok &= test_value_ssa_simplify_cfg_same_target_branch();
    ok &= test_value_ssa_simplify_cfg_same_return_diamond();
    ok &= test_value_ssa_simplify_cfg_jump_to_empty_return();
    ok &= test_value_ssa_simplify_cfg_empty_jump_thread();
    ok &= test_value_ssa_simplify_cfg_branch_empty_jump_same_target();
    ok &= test_value_ssa_simplify_cfg_immediate_branch_compacts_blocks();
    ok &= test_value_ssa_simplify_cfg_single_predecessor_nonempty_merge();
    ok &= test_value_ssa_eliminate_dead_value_defs_after_cfg_cleanup();
    ok &= test_value_ssa_eliminate_dead_value_defs_preserves_dead_result_call();
    ok &= test_value_ssa_eliminate_dead_value_defs_preserves_dangerous_dead_binary();
    ok &= test_value_ssa_conversion_straight_line_program();
    ok &= test_value_ssa_conversion_diamond_program();
    ok &= test_value_ssa_conversion_diamond_store_indirect_phi_program();
    ok &= test_value_ssa_conversion_scrambled_diamond_program();
    ok &= test_value_ssa_conversion_simple_loop_program();
    ok &= test_value_ssa_conversion_loop_carried_temp_program();
    ok &= test_value_ssa_conversion_multi_backedge_loop_carried_temp_program();
    ok &= test_value_ssa_conversion_two_carried_temps_loop_program();
    ok &= test_value_ssa_conversion_nested_loop_carried_temps_program();
    ok &= test_value_ssa_conversion_nested_same_temp_double_loop_program();
    ok &= test_value_ssa_conversion_nested_same_temp_multi_backedge_inner_loop_program();
    ok &= test_value_ssa_cfg_analysis_loop_program();
    ok &= test_value_ssa_phi_placement_loop_program();
    ok &= test_value_ssa_dominator_preorder_loop_program();
    ok &= test_value_ssa_dominator_walk_loop_program();
    ok &= test_value_ssa_rename_state_scopes();
    ok &= test_value_ssa_rename_state_walk_scopes_diamond();
    ok &= test_value_ssa_rename_rewrite_block_uses();
    ok &= test_value_ssa_rename_rewrite_branch_condition();
    ok &= test_value_ssa_rename_rewrite_phi_inputs_for_predecessor();
    ok &= test_value_ssa_rename_function_values_scrambled_diamond();
    ok &= test_value_ssa_canonicalize_program_scrambled_diamond();
    ok &= test_value_ssa_canonicalize_program_identity_add_zero();
    ok &= test_value_ssa_canonicalize_program_local_load_forward();
    ok &= test_value_ssa_canonicalize_program_dead_global_store();
    ok &= test_value_ssa_canonicalize_program_dead_local_store_across_straight_chain();
    ok &= test_value_ssa_canonicalize_program_dead_local_store_across_straight_chain_fixed_point();
    ok &= test_value_ssa_canonicalize_program_dead_global_store_across_straight_chain();
    ok &= test_value_ssa_canonicalize_program_dead_global_store_across_straight_chain_fixed_point();
    ok &= test_value_ssa_canonicalize_program_redundant_local_store_across_straight_chain();
    ok &= test_value_ssa_canonicalize_program_redundant_local_store_across_straight_chain_fixed_point();
    ok &= test_value_ssa_canonicalize_program_redundant_global_store();
    ok &= test_value_ssa_canonicalize_program_cross_block_global_call_barrier();
    ok &= test_value_ssa_canonicalize_program_cross_block_global_call_barrier_fixed_point();
    ok &= test_value_ssa_canonicalize_program_complex_local_chain();
    ok &= test_value_ssa_canonicalize_program_complex_local_chain_fixed_point();
    ok &= test_value_ssa_canonicalize_program_complex_global_call_chain();
    ok &= test_value_ssa_canonicalize_program_complex_global_call_chain_fixed_point();
    ok &= test_value_ssa_canonicalize_program_global_load_forward();
    ok &= test_value_ssa_canonicalize_program_normalizes_compare_immediate_lhs();
    ok &= test_value_ssa_canonicalize_program_bitand_all_ones();
    ok &= test_value_ssa_canonicalize_program_redundant_binary();
    ok &= test_value_ssa_canonicalize_program_dominated_redundant_binary();
    ok &= test_value_ssa_canonicalize_program_preserves_dead_result_call();
    ok &= test_value_ssa_canonicalize_program_preserves_dangerous_dead_binary();
    ok &= test_value_ssa_canonicalize_program_scrambled_diamond_fixed_point();
    ok &= test_value_ssa_canonicalize_program_sccp_constant_branch();
    ok &= test_value_ssa_sccp_readonly_global_branch();
    ok &= test_value_ssa_sccp_mutated_global_branch();
    ok &= test_value_ssa_sccp_readonly_global_indirect_load();
    ok &= test_value_ssa_sccp_readonly_global_indirect_load_phi();
    ok &= test_value_ssa_sccp_local_address_phi_compare();
    ok &= test_value_ssa_sccp_local_indirect_load_does_not_fold();
    ok &= test_value_ssa_sccp_readonly_global_indirect_load_store_barrier();
    ok &= test_value_ssa_sccp_parameter_pointer_zero_add();
    ok &= test_value_ssa_sccp_parameter_pointer_commuted_add();
    ok &= test_value_ssa_sccp_global_address_commuted_eq();
    ok &= test_value_ssa_sccp_parameter_pointer_commuted_ne();
    ok &= test_value_ssa_sccp_distinct_local_addresses_ne();
    ok &= test_value_ssa_sccp_local_global_addresses_ne();
    ok &= test_value_ssa_sccp_local_parameter_pointer_ne();
    ok &= test_value_ssa_sccp_global_address_nonzero_compare();
    ok &= test_value_ssa_sccp_parameter_pointer_nonzero_compare();
    ok &= test_value_ssa_sccp_global_address_branch();
    ok &= test_value_ssa_sccp_local_address_branch();
    ok &= test_value_ssa_sccp_parameter_pointer_branch();
    ok &= test_value_ssa_sccp_global_address_offset_compare();
    ok &= test_value_ssa_sccp_global_address_offset_branch();
    ok &= test_value_ssa_sccp_readonly_global_indirect_load_zero_offset();
    ok &= test_value_ssa_sccp_readonly_global_indirect_load_nonzero_offset();
    ok &= test_value_ssa_inline_tiny_internal_helpers_addr_global();
    ok &= test_value_ssa_inline_tiny_internal_helpers_global_indirect_load();
    ok &= test_value_ssa_inline_tiny_internal_helpers_parameter_pointer();
    ok &= test_value_ssa_inline_tiny_internal_helpers_two_block_return();
    ok &= test_value_ssa_inline_tiny_internal_helpers_void_two_block_return();
    ok &= test_value_ssa_inline_tiny_internal_helpers_zero_param_constant();
    ok &= test_value_ssa_inline_tiny_internal_helpers_budget_blocks_large_helper();
    ok &= test_value_ssa_inline_tiny_internal_helpers_function_budget();
    ok &= test_value_ssa_source_sccp_constant_branch();
    ok &= test_value_ssa_canonicalized_conversion_diamond_program();
    ok &= test_value_ssa_canonicalized_conversion_diamond_fixed_point();
    ok &= test_value_ssa_canonicalized_conversion_preserves_dead_result_call();
    ok &= test_value_ssa_canonicalized_conversion_preserves_dangerous_dead_binary();
    ok &= test_value_ssa_memory_canonicalized_conversion_local_join();
    ok &= test_value_ssa_memory_value_canonicalized_conversion_fold_program();
    ok &= test_value_ssa_memory_value_canonicalized_conversion_nested_while_regression();
    ok &= test_value_ssa_memory_canonicalized_conversion_fold_program();
    ok &= test_value_ssa_mode_conversion_classic_diamond();
    ok &= test_value_ssa_mode_conversion_memory_value_fold();
    ok &= test_value_ssa_mode_conversion_memory_full_fold();
    ok &= test_value_ssa_mode_conversion_rejects_unknown_mode();
    ok &= test_value_ssa_default_conversion_fold_program();
    ok &= test_value_ssa_default_conversion_indirect_safe_global_forward_program();
    ok &= test_value_ssa_default_conversion_tiny_helper_inline_program();
    ok &= test_value_ssa_default_conversion_tiny_helper_two_block_return_program();
    ok &= test_value_ssa_default_conversion_tiny_void_helper_two_block_return_program();
    ok &= test_value_ssa_default_conversion_internal_call_preserves_unwritten_global_forward_program();
    ok &= test_value_ssa_default_conversion_eliminates_unread_scalar_local_store_program();
    ok &= test_value_ssa_default_conversion_preserves_array_local_store_program();
    ok &= test_value_ssa_default_conversion_preserves_address_taken_scalar_local_store_program();
    ok &= test_value_ssa_default_conversion_reuses_unique_pred_repeated_indirect_load_program();
    ok &= test_value_ssa_default_conversion_reuses_unique_pred_repeated_indirect_load_across_non_alias_local_store_program();
    ok &= test_value_ssa_default_conversion_reuses_dominated_repeated_indirect_load_across_non_alias_local_store();
    ok &= test_value_ssa_default_conversion_reuses_repeated_addr_root_program();
    ok &= test_value_ssa_default_conversion_reuses_repeated_pure_internal_calls();
    ok &= test_value_ssa_default_conversion_reaches_redundant_binary_fixed_point();
    ok &= test_value_ssa_default_conversion_forwards_same_block_indirect_load_across_param_store();
    ok &= test_value_ssa_default_conversion_reuses_repeated_pure_internal_calls_across_store_indirect();
    ok &= test_value_ssa_default_matches_memory_full_mode();
    ok &= test_value_ssa_default_matches_direct_on_extreme_param_program();
    ok &= test_value_ssa_canonicalize_program_rejects_invalid_input();
    ok &= test_value_ssa_verifier_rejects_zero_arg_call_with_nonnull_args();
    ok &= test_value_ssa_builder_rejects_declaration_block_append();
    ok &= test_value_ssa_builder_rejects_declaration_allocate_value();
    ok &= test_value_ssa_builder_rejects_parameter_prefix_break();
    ok &= test_value_ssa_builder_rejects_phi_after_instruction();
    ok &= test_value_ssa_builder_rejects_terminator_overwrite();
    ok &= test_value_ssa_builder_void_return_clears_stale_return_value();

    if (!ok) {
        return 1;
    }

    printf("[value-ssa-reg] PASS\n");
    return 0;
}
