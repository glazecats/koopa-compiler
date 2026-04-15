#include "memory_ssa_pass.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_dump(const char *case_id, const ValueSsaProgram *program, const char *expected) {
    char *actual = NULL;
    int ok = 1;

    if (!value_ssa_dump_program(program, &actual)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: %s dump failed\n", case_id);
        return 0;
    }

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: %s dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected,
            actual);
        ok = 0;
    }

    free(actual);
    return ok;
}

static int expect_pipeline_rejects(const char *case_id,
    ValueSsaProgram *program,
    const char *expected_fragment) {
    ValueSsaError error;

    if (memory_ssa_pass_run_pipeline(program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: %s unexpectedly passed pipeline\n", case_id);
        return 0;
    }
    if (!strstr(error.message, expected_fragment)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: %s mismatch\nexpected fragment: %s\nactual: %s\n",
            case_id,
            expected_fragment,
            error.message);
        return 0;
    }

    return 1;
}

static int expect_memory_value_canonicalize_rejects(const char *case_id,
    ValueSsaProgram *program,
    const char *expected_fragment) {
    ValueSsaError error;

    if (memory_ssa_pass_canonicalize_memory_values(program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: %s unexpectedly passed memory-value canonicalizer\n", case_id);
        return 0;
    }
    if (!strstr(error.message, expected_fragment)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: %s mismatch\nexpected fragment: %s\nactual: %s\n",
            case_id,
            expected_fragment,
            error.message);
        return 0;
    }

    return 1;
}

static int build_join_preserved_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
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
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_invalid_input_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(0);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_phi_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
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

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_same_value_phi_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
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
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_parameter_local_reuse_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
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
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_phi_forward_parameter_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
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
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value2);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_phi_forward_parameter_local_moved_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t then_load_value;
    size_t then_move_value;
    size_t else_value;
    size_t join_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    then_load_value = value_ssa_function_allocate_value(function);
    then_move_value = value_ssa_function_allocate_value(function);
    else_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (then_load_value == (size_t)-1 || then_move_value == (size_t)-1 || else_value == (size_t)-1 ||
        join_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_load_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_move_value);
    instruction.as.mov_value = value_ssa_value_id(then_load_value);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(else_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(join_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_join_preserved_global_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t value0;

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

    (void)global;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_same_value_phi_global_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t value0;

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

    (void)global;
    if (!value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_phi_forward_global_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t value0;
    size_t value1;
    size_t value2;

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

    (void)global;
    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value2);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_repeated_global_load_after_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = (char *)malloc(6);
    if (!instruction.as.call.callee_name) {
        value_ssa_program_free(program);
        return 0;
    }
    memcpy(instruction.as.call.callee_name, "touch", 6);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_internal_pure_call_global_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_function = NULL;
    ValueSsaBasicBlock *helper_block = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_function, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_block, error) ||
        !value_ssa_function_append_block(main_function, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    if (!value_ssa_block_set_return(helper_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(main_function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = (char *)malloc(7);
    if (!instruction.as.call.callee_name) {
        value_ssa_program_free(program);
        return 0;
    }
    memcpy(instruction.as.call.callee_name, "helper", 7);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_internal_other_global_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_function = NULL;
    ValueSsaBasicBlock *helper_block = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t helper_value;
    size_t main_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_global(program, "h", &global, error) ||
        !value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_function, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_block, error) ||
        !value_ssa_function_append_block(main_function, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    helper_value = value_ssa_function_allocate_value(helper);
    main_value = value_ssa_function_allocate_value(main_function);
    if (helper_value == (size_t)-1 || main_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(helper_value);
    instruction.as.load_slot = value_ssa_slot_global(1);
    if (!value_ssa_block_append_instruction(helper_block, &instruction, error) ||
        !value_ssa_block_set_return(helper_block, value_ssa_value_id(helper_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = (char *)malloc(7);
    if (!instruction.as.call.callee_name) {
        value_ssa_program_free(program);
        return 0;
    }
    memcpy(instruction.as.call.callee_name, "helper", 7);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(main_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(main_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_global_call_barrier_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = (char *)malloc(6);
    if (!instruction.as.call.callee_name) {
        value_ssa_program_free(program);
        return 0;
    }
    memcpy(instruction.as.call.callee_name, "touch", 6);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_local_join_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_set_jump(else_block, 3, error) ||
        !value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_parameter_local_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
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
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_id(value0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_parameter_local_phi_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t load_value;
    size_t phi_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    phi_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1 || phi_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(load_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(load_value);
    if (!value_ssa_block_append_phi(join_block, phi_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_id(phi_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_parameter_local_join_load_phi_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t then_value;
    size_t else_value;
    size_t phi_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    then_value = value_ssa_function_allocate_value(function);
    else_value = value_ssa_function_allocate_value(function);
    phi_value = value_ssa_function_allocate_value(function);
    if (then_value == (size_t)-1 || else_value == (size_t)-1 || phi_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].value = value_ssa_value_id(then_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_phi(join_block, phi_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_id(phi_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_parameter_local_loop_phi_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t entry_value;
    size_t header_phi_value;
    size_t body_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    entry_value = value_ssa_function_allocate_value(function);
    header_phi_value = value_ssa_function_allocate_value(function);
    body_value = value_ssa_function_allocate_value(function);
    if (entry_value == (size_t)-1 || header_phi_value == (size_t)-1 || body_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(entry_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(entry_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(body_value);
    if (!value_ssa_block_append_phi(header, header_phi_value, phi_inputs, 2, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_immediate(1), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(body_value);
    instruction.as.mov_value = value_ssa_value_id(header_phi_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_id(body_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_global_join_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;

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

    (void)global;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_set_jump(else_block, 3, error) ||
        !value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_global_store_after_load_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = (char *)malloc(6);
    if (!instruction.as.call.callee_name) {
        value_ssa_program_free(program);
        return 0;
    }
    memcpy(instruction.as.call.callee_name, "touch", 6);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_id(value0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_redundant_global_after_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = (char *)malloc(6);
    if (!instruction.as.call.callee_name) {
        value_ssa_program_free(program);
        return 0;
    }
    memcpy(instruction.as.call.callee_name, "touch", 6);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_global_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_overwritten_local_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
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

static int build_overwritten_global_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
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

static int build_pipeline_combo_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_set_jump(else_block, 3, error) ||
        !value_ssa_block_append_instruction(join_block, &instruction, error)) {
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
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_pipeline_fold_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t load_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    load_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (load_value == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(load_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_pipeline_cfg_revisit_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t load_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "flag", 0, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
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
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_pipeline_cfg_revisit_global_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t cond_value;
    size_t load_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "flag", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    load_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || load_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = (char *)malloc(6);
    if (!instruction.as.call.callee_name) {
        value_ssa_program_free(program);
        return 0;
    }
    memcpy(instruction.as.call.callee_name, "touch", 6);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(load_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(load_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_same_value_loop_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_immediate(1), 2, 3, error) ||
        !value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
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
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_loop_header_parameter_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t entry_value;
    size_t header_value;
    size_t body_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    entry_value = value_ssa_function_allocate_value(function);
    header_value = value_ssa_function_allocate_value(function);
    body_value = value_ssa_function_allocate_value(function);
    if (entry_value == (size_t)-1 || header_value == (size_t)-1 || body_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(entry_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(header_value);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_immediate(1), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(body_value);
    instruction.as.mov_value = value_ssa_value_id(header_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(header_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_loop_header_global_readonly_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_function = NULL;
    ValueSsaBasicBlock *helper_block = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t helper_value;
    size_t entry_value;
    size_t header_value;
    size_t body_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_global(program, "h", &global, error) ||
        !value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_function, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_block, error) ||
        !value_ssa_function_append_block(main_function, NULL, &entry, error) ||
        !value_ssa_function_append_block(main_function, NULL, &header, error) ||
        !value_ssa_function_append_block(main_function, NULL, &body, error) ||
        !value_ssa_function_append_block(main_function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    helper_value = value_ssa_function_allocate_value(helper);
    entry_value = value_ssa_function_allocate_value(main_function);
    header_value = value_ssa_function_allocate_value(main_function);
    body_value = value_ssa_function_allocate_value(main_function);
    if (helper_value == (size_t)-1 || entry_value == (size_t)-1 || header_value == (size_t)-1 ||
        body_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(helper_value);
    instruction.as.load_slot = value_ssa_slot_global(1);
    if (!value_ssa_block_append_instruction(helper_block, &instruction, error) ||
        !value_ssa_block_set_return(helper_block, value_ssa_value_id(helper_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(entry_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(header_value);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_immediate(1), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = (char *)malloc(7);
    if (!instruction.as.call.callee_name) {
        value_ssa_program_free(program);
        return 0;
    }
    memcpy(instruction.as.call.callee_name, "helper", 7);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(body_value);
    instruction.as.mov_value = value_ssa_value_id(header_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(header_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_differing_value_loop_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_immediate(1), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
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
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_same_value_loop_global_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_immediate(1), 2, 3, error) ||
        !value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_loop_global_call_barrier_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t value0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_immediate(1), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.as.call.callee_name = (char *)malloc(6);
    if (!instruction.as.call.callee_name) {
        value_ssa_program_free(program);
        return 0;
    }
    memcpy(instruction.as.call.callee_name, "touch", 6);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_store_branch_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_immediate(0), error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_immediate(1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_phi_overwritten_local_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
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

    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_phi_overwritten_global_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;

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

    (void)global;
    if (!value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
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

    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_loop_overwritten_local_store_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_immediate(1), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_live_store_join_program(ValueSsaProgram *program, ValueSsaError *error) {
    return build_join_preserved_local_program(program, error);
}

static int test_memory_ssa_pass_forwards_join_preserved_local_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_join_preserved_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: join-preserved setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: join-preserved forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-JOIN",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.0 = mov 7\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_same_value_phi_local_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_same_value_phi_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: same-value phi local setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: same-value phi local forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PHI-SAME-LOCAL",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_local a.0, 7\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    store_local a.0, 7\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.0 = mov 7\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_parameter_local_reuse_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_parameter_local_reuse_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: parameter-local reuse setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: parameter-local reuse forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PARAM-LOCAL-REUSE",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = mov ssa.0\n"
        "    ret ssa.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_phi_join_parameter_local_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_phi_forward_parameter_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi-join parameter-local setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi-join parameter-local forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PHI-JOIN-PARAM-LOCAL",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.0 = load_local a.0\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.1 = load_local a.0\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: ssa.0], [bb.2: ssa.1]\n"
        "    ssa.2 = mov ssa.3\n"
        "    ret ssa.2\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_phi_join_parameter_local_load_through_mov(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_phi_forward_parameter_local_moved_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi-join parameter-local moved setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi-join parameter-local moved forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PHI-JOIN-PARAM-LOCAL-MOV",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = mov ssa.0\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = load_local a.0\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.4 = phi [bb.1: ssa.1], [bb.2: ssa.2]\n"
        "    ssa.3 = mov ssa.4\n"
        "    ret ssa.3\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_same_value_loop_local_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_same_value_loop_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: same-value loop local setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: same-value loop local forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-LOOP-SAME-LOCAL",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    store_local a.0, 7\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.0 = mov 7\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_loop_header_parameter_local_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_loop_header_parameter_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: loop-header parameter-local setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: loop-header parameter-local forward failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-LOOP-HEADER-PARAM-LOCAL",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.3 = phi [bb.0: ssa.0], [bb.2: ssa.2]\n"
        "    ssa.1 = mov ssa.3\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = mov ssa.1\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret ssa.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_loop_header_global_load_through_readonly_call(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_loop_header_global_readonly_call_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: loop-header global readonly-call setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: loop-header global readonly-call forward failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-LOOP-HEADER-GLOBAL-READONLY-CALL",
        &program,
        "global g.0\n"
        "global h.1\n"
        "\n"
        "func helper() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_global h.1\n"
        "    ret ssa.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_global g.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.3 = phi [bb.0: ssa.0], [bb.2: ssa.2]\n"
        "    ssa.1 = mov ssa.3\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    call helper()\n"
        "    ssa.2 = mov ssa.1\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret ssa.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_keeps_differing_value_loop_local_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_differing_value_loop_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: differing-value loop local setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: differing-value loop local forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-LOOP-DIFF-LOCAL",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    store_local a.0, 9\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_keeps_phi_load_unforwarded(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_phi_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PHI",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_local a.0, 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    store_local a.0, 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_branch_dead_local_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_dead_store_branch_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: dead-branch setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_dead_local_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: dead-branch elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-DSE-BRANCH",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ret 0\n"
        "  bb.2:\n"
        "    ret 1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_join_preserved_global_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_join_preserved_global_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: global-join setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: global-join forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-GLOBAL-JOIN",
        &program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.0 = mov 9\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_same_value_phi_global_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_same_value_phi_global_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: same-value phi global setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: same-value phi global forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PHI-SAME-GLOBAL",
        &program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_global g.0, 9\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    store_global g.0, 9\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.0 = mov 9\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_same_value_loop_global_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_same_value_loop_global_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: same-value loop global setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: same-value loop global forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-LOOP-SAME-GLOBAL",
        &program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    store_global g.0, 9\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.0 = mov 9\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_repeated_global_load_after_call(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_repeated_global_load_after_call_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: repeated global-after-call setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: repeated global-after-call forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-GLOBAL-REUSE-CALL",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    call touch()\n"
        "    ssa.0 = load_global g.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = mov ssa.0\n"
        "    ret ssa.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_phi_join_global_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_phi_forward_global_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi-join global setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi-join global forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PHI-JOIN-GLOBAL",
        &program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.0 = load_global g.0\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.1 = load_global g.0\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: ssa.0], [bb.2: ssa.1]\n"
        "    ssa.2 = mov ssa.3\n"
        "    ret ssa.2\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_global_load_after_internal_pure_call(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_internal_pure_call_global_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: internal pure-call setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: internal pure-call forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-GLOBAL-INTERNAL-PURE",
        &program,
        "global g.0\n"
        "\n"
        "func helper() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    call helper()\n"
        "    ssa.0 = mov 9\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_forwards_global_load_after_other_global_call(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_internal_other_global_call_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: other-global call setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: other-global call forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-GLOBAL-OTHER-GLOBAL-CALL",
        &program,
        "global g.0\n"
        "global h.1\n"
        "\n"
        "func helper() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_global h.1\n"
        "    ret ssa.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    call helper()\n"
        "    ssa.0 = mov 9\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_keeps_loop_global_load_after_call(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_loop_global_call_barrier_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: loop global-call setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: loop global-call forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-LOOP-GLOBAL-CALL",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    call touch()\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.0 = load_global g.0\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_keeps_global_load_after_call(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_global_call_barrier_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: global-call setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: global-call forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-GLOBAL-CALL",
        &program,
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

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_local_join_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_local_join_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: redundant local join-store setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: redundant local join-store elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-REDUNDANT-LOCAL",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_parameter_local_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_parameter_local_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: redundant parameter-local store setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_stores(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant parameter-local store elimination failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-REDUNDANT-PARAM-LOCAL",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_parameter_local_phi_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_parameter_local_phi_store_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant parameter-local phi store setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_stores(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant parameter-local phi store elimination failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-REDUNDANT-PARAM-LOCAL-PHI",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: ssa.0], [bb.2: ssa.0]\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_parameter_local_join_load_phi_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_parameter_local_join_load_phi_store_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant parameter-local join-load phi store setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_stores(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant parameter-local join-load phi store elimination failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-REDUNDANT-PARAM-LOCAL-JOIN-LOAD-PHI",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.0 = load_local a.0\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.1 = load_local a.0\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.2 = phi [bb.1: ssa.0], [bb.2: ssa.1]\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_parameter_local_loop_phi_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_parameter_local_loop_phi_store_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant parameter-local loop phi store setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_stores(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant parameter-local loop phi store elimination failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-REDUNDANT-PARAM-LOCAL-LOOP-PHI",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = phi [bb.0: ssa.0], [bb.2: ssa.2]\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = mov ssa.1\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_global_join_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_global_join_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: redundant global join-store setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: redundant global join-store elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-REDUNDANT-GLOBAL",
        &program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_global_store_after_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_global_store_after_load_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: redundant global store-after-load setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_stores(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant global store-after-load elimination failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-REDUNDANT-GLOBAL-LOAD",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    call touch()\n"
        "    ssa.0 = load_global g.0\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_keeps_redundant_global_store_after_call(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_global_after_call_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: redundant global-after-call setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: redundant global-after-call elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-REDUNDANT-GLOBAL-CALL",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    store_global g.0, 9\n"
        "    call touch()\n"
        "    store_global g.0, 9\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_dead_global_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_dead_global_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: dead global-store setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_dead_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: dead global-store elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-DEAD-GLOBAL",
        &program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_keeps_global_store_before_call_live(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_global_call_barrier_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: dead global-before-call setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_dead_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: dead global-before-call elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-DEAD-GLOBAL-CALL",
        &program,
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

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_overwritten_stores(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_overwritten_local_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: overwritten local-store setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_dead_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: overwritten local-store elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-DEAD-OVERWRITE-LOCAL",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    if (!ok) {
        return 0;
    }

    if (!build_overwritten_global_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: overwritten global-store setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_dead_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: overwritten global-store elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-DEAD-OVERWRITE-GLOBAL",
        &program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_phi_and_loop_overwritten_stores(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_phi_overwritten_local_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi-overwritten local setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_dead_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi-overwritten local elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-DEAD-PHI-OVERWRITE-LOCAL",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    if (!ok) {
        return 0;
    }

    if (!build_phi_overwritten_global_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi-overwritten global setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_dead_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: phi-overwritten global elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-DEAD-PHI-OVERWRITE-GLOBAL",
        &program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    if (!ok) {
        return 0;
    }

    if (!build_loop_overwritten_local_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: loop-overwritten local setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_dead_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: loop-overwritten local elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-DEAD-LOOP-OVERWRITE-LOCAL",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_combo(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_pipeline_combo_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline combo setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline combo run failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-COMBO",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_canonicalize_memory_values_local_join(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_parameter_local_join_load_phi_store_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-value local-join setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_canonicalize_memory_values(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-value local-join canonicalize failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-MEMVAL-LOCAL-JOIN",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_canonicalize_memory_values_global_chain(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_global_store_after_load_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-value global-chain setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_canonicalize_memory_values(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-value global-chain canonicalize failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-MEMVAL-GLOBAL-CHAIN",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    call touch()\n"
        "    ssa.0 = load_global g.0\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_canonicalize_memory_values_rejects_invalid_input(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_invalid_input_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: memory-value invalid-input setup failed: %s\n", error.message);
        return 0;
    }

    ok = expect_memory_value_canonicalize_rejects(
        "MEMORY-SSA-PASS-MEMVAL-INVALID", &program, "VALUE-SSA");
    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_fold_exposed_constant(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_pipeline_fold_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline fold setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline fold run failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-FOLD",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 12\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_eliminates_redundant_global_store_after_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_global_store_after_load_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline redundant global store-after-load setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline redundant global store-after-load run failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-REDUNDANT-GLOBAL-LOAD",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    call touch()\n"
        "    ssa.0 = load_global g.0\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_eliminates_redundant_parameter_local_phi_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_parameter_local_phi_store_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline redundant parameter-local phi store setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline redundant parameter-local phi store run failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-REDUNDANT-PARAM-LOCAL-PHI",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_eliminates_redundant_parameter_local_join_load_phi_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_parameter_local_join_load_phi_store_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline redundant parameter-local join-load phi store setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline redundant parameter-local join-load phi store run failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-REDUNDANT-PARAM-LOCAL-JOIN-LOAD-PHI",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_eliminates_redundant_parameter_local_loop_phi_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_parameter_local_loop_phi_store_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline redundant parameter-local loop phi store setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline redundant parameter-local loop phi store run failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-REDUNDANT-PARAM-LOCAL-LOOP-PHI",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = phi [bb.0: ssa.0], [bb.1: ssa.1]\n"
        "    jmp bb.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_forwards_internal_pure_call_global(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_internal_pure_call_global_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline internal pure-call setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline internal pure-call run failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-GLOBAL-INTERNAL-PURE",
        &program,
        "global g.0\n"
        "\n"
        "func helper() {\n"
        "  bb.0:\n"
        "    ret 0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    call helper()\n"
        "    ret 9\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_reuses_repeated_global_load_after_call(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_repeated_global_load_after_call_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline repeated global-after-call setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline repeated global-after-call run failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-GLOBAL-REUSE-CALL",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    call touch()\n"
        "    ssa.0 = load_global g.0\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_cfg_revisit_local(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_pipeline_cfg_revisit_local_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline cfg-revisit local setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline cfg-revisit local run failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-CFG-REVISIT-LOCAL",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_cfg_revisit_global(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_pipeline_cfg_revisit_global_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline cfg-revisit global setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline cfg-revisit global run failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-CFG-REVISIT-GLOBAL",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_same_value_loop(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_same_value_loop_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline same-value loop setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline same-value loop run failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-LOOP",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    jmp bb.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_keeps_loop_global_call_barrier(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_loop_global_call_barrier_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline loop global-call setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline loop global-call run failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-LOOP-GLOBAL-CALL",
        &program,
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
        "    jmp bb.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_is_fixed_point(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    char *first = NULL;
    char *second = NULL;
    int ok = 1;

    if (!build_pipeline_combo_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: fixed-point setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error) ||
        !value_ssa_dump_program(&program, &first) ||
        !memory_ssa_pass_run_pipeline(&program, &error) ||
        !value_ssa_dump_program(&program, &second)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: fixed-point run failed: %s\n", error.message);
        free(first);
        free(second);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(first, second) != 0) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline fixed-point mismatch\nfirst:\n%s\nsecond:\n%s\n",
            first,
            second);
        ok = 0;
    }

    free(first);
    free(second);
    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_rejects_invalid_input(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_invalid_input_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: invalid-input setup failed: %s\n", error.message);
        return 0;
    }

    ok = expect_pipeline_rejects("MEMORY-SSA-PASS-PIPELINE-INVALID", &program, "MEMORY-SSA-PASS-019");
    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_keeps_live_join_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_live_store_join_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: live-join setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_dead_local_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: live-join elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-DSE-LIVE",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.0 = load_local a.0\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_memory_ssa_pass_forwards_join_preserved_local_load();
    ok &= test_memory_ssa_pass_forwards_same_value_phi_local_load();
    ok &= test_memory_ssa_pass_forwards_parameter_local_reuse_load();
    ok &= test_memory_ssa_pass_forwards_phi_join_parameter_local_load();
    ok &= test_memory_ssa_pass_forwards_phi_join_parameter_local_load_through_mov();
    ok &= test_memory_ssa_pass_forwards_same_value_loop_local_load();
    ok &= test_memory_ssa_pass_forwards_loop_header_parameter_local_load();
    ok &= test_memory_ssa_pass_forwards_loop_header_global_load_through_readonly_call();
    ok &= test_memory_ssa_pass_keeps_differing_value_loop_local_load();
    ok &= test_memory_ssa_pass_keeps_phi_load_unforwarded();
    ok &= test_memory_ssa_pass_eliminates_branch_dead_local_store();
    ok &= test_memory_ssa_pass_forwards_join_preserved_global_load();
    ok &= test_memory_ssa_pass_forwards_same_value_phi_global_load();
    ok &= test_memory_ssa_pass_forwards_same_value_loop_global_load();
    ok &= test_memory_ssa_pass_forwards_repeated_global_load_after_call();
    ok &= test_memory_ssa_pass_forwards_phi_join_global_load();
    ok &= test_memory_ssa_pass_forwards_global_load_after_internal_pure_call();
    ok &= test_memory_ssa_pass_forwards_global_load_after_other_global_call();
    ok &= test_memory_ssa_pass_keeps_loop_global_load_after_call();
    ok &= test_memory_ssa_pass_keeps_global_load_after_call();
    ok &= test_memory_ssa_pass_eliminates_redundant_local_join_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_parameter_local_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_parameter_local_phi_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_parameter_local_join_load_phi_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_parameter_local_loop_phi_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_global_join_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_global_store_after_load();
    ok &= test_memory_ssa_pass_keeps_redundant_global_store_after_call();
    ok &= test_memory_ssa_pass_eliminates_dead_global_store();
    ok &= test_memory_ssa_pass_keeps_global_store_before_call_live();
    ok &= test_memory_ssa_pass_eliminates_overwritten_stores();
    ok &= test_memory_ssa_pass_eliminates_phi_and_loop_overwritten_stores();
    ok &= test_memory_ssa_pass_canonicalize_memory_values_local_join();
    ok &= test_memory_ssa_pass_canonicalize_memory_values_global_chain();
    ok &= test_memory_ssa_pass_canonicalize_memory_values_rejects_invalid_input();
    ok &= test_memory_ssa_pass_pipeline_combo();
    ok &= test_memory_ssa_pass_pipeline_fold_exposed_constant();
    ok &= test_memory_ssa_pass_pipeline_eliminates_redundant_global_store_after_load();
    ok &= test_memory_ssa_pass_pipeline_eliminates_redundant_parameter_local_phi_store();
    ok &= test_memory_ssa_pass_pipeline_eliminates_redundant_parameter_local_join_load_phi_store();
    ok &= test_memory_ssa_pass_pipeline_eliminates_redundant_parameter_local_loop_phi_store();
    ok &= test_memory_ssa_pass_pipeline_forwards_internal_pure_call_global();
    ok &= test_memory_ssa_pass_pipeline_reuses_repeated_global_load_after_call();
    ok &= test_memory_ssa_pass_pipeline_cfg_revisit_local();
    ok &= test_memory_ssa_pass_pipeline_cfg_revisit_global();
    ok &= test_memory_ssa_pass_pipeline_same_value_loop();
    ok &= test_memory_ssa_pass_pipeline_keeps_loop_global_call_barrier();
    ok &= test_memory_ssa_pass_pipeline_is_fixed_point();
    ok &= test_memory_ssa_pass_pipeline_rejects_invalid_input();
    ok &= test_memory_ssa_pass_keeps_live_join_store();
    if (!ok) {
        return 1;
    }

    printf("[memory-ssa-pass] PASS\n");
    return 0;
}
