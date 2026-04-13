#include "value_ssa.h"
#include "value_ssa_pass.h"
#include "lower_ir.h"

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

static int test_value_ssa_fold_preserves_dangerous_binary(void) {
    return expect_constant_folded_dump("VALUE-SSA-FOLD-PRESERVE-DANGEROUS-BINARY",
        build_dangerous_dead_binary_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = div 1, 0\n"
        "    ret 0\n"
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

static int test_value_ssa_canonicalize_program_rejects_invalid_input(void) {
    return expect_canonicalize_rejected("VALUE-SSA-CANONICALIZE-REJECT-INVALID",
        build_invalid_undefined_value_program,
        "VALUE-SSA-064");
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
    ok &= test_value_ssa_forward_global_loads_after_store();
    ok &= test_value_ssa_forward_global_loads_across_straight_chain();
    ok &= test_value_ssa_forward_global_loads_do_not_cross_join();
    ok &= test_value_ssa_forward_global_loads_killed_by_call();
    ok &= test_value_ssa_forward_global_loads_killed_by_cross_block_call();
    ok &= test_value_ssa_eliminate_dead_local_store();
    ok &= test_value_ssa_eliminate_dead_local_store_across_straight_chain();
    ok &= test_value_ssa_eliminate_dead_local_store_across_two_hop_chain();
    ok &= test_value_ssa_eliminate_dead_local_store_preserves_branch_observer();
    ok &= test_value_ssa_eliminate_dead_global_store();
    ok &= test_value_ssa_eliminate_dead_global_store_across_straight_chain();
    ok &= test_value_ssa_eliminate_dead_global_store_across_two_hop_chain();
    ok &= test_value_ssa_eliminate_dead_global_store_preserves_call_barrier();
    ok &= test_value_ssa_eliminate_dead_global_store_preserves_cross_block_call_barrier();
    ok &= test_value_ssa_eliminate_dead_global_store_preserves_branch_observer();
    ok &= test_value_ssa_eliminate_redundant_local_store_across_straight_chain();
    ok &= test_value_ssa_eliminate_redundant_local_store_across_two_hop_chain();
    ok &= test_value_ssa_eliminate_redundant_local_store_does_not_cross_join();
    ok &= test_value_ssa_eliminate_redundant_global_store();
    ok &= test_value_ssa_eliminate_redundant_global_store_preserves_call_barrier();
    ok &= test_value_ssa_eliminate_redundant_global_store_preserves_cross_block_call_barrier();
    ok &= test_value_ssa_eliminate_redundant_global_store_does_not_cross_join();
    ok &= test_value_ssa_eliminate_redundant_binary();
    ok &= test_value_ssa_eliminate_redundant_binary_preserves_dangerous_binary();
    ok &= test_value_ssa_eliminate_dominated_redundant_binary();
    ok &= test_value_ssa_simplify_identity_add_zero();
    ok &= test_value_ssa_simplify_identity_eq_self();
    ok &= test_value_ssa_simplify_identity_div_one();
    ok &= test_value_ssa_fold_constant_binary();
    ok &= test_value_ssa_fold_preserves_dangerous_binary();
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
    ok &= test_value_ssa_canonicalized_conversion_diamond_program();
    ok &= test_value_ssa_canonicalized_conversion_diamond_fixed_point();
    ok &= test_value_ssa_canonicalized_conversion_preserves_dead_result_call();
    ok &= test_value_ssa_canonicalized_conversion_preserves_dangerous_dead_binary();
    ok &= test_value_ssa_canonicalize_program_rejects_invalid_input();
    ok &= test_value_ssa_builder_rejects_declaration_block_append();
    ok &= test_value_ssa_builder_rejects_declaration_allocate_value();
    ok &= test_value_ssa_builder_rejects_parameter_prefix_break();
    ok &= test_value_ssa_builder_rejects_phi_after_instruction();
    ok &= test_value_ssa_builder_rejects_terminator_overwrite();

    if (!ok) {
        return 1;
    }

    printf("[value-ssa-reg] PASS\n");
    return 0;
}
