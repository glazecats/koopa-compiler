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

static int expect_built_canonicalized_dump(const char *case_id,
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
            "[memory-ssa-pass] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!memory_ssa_pass_build_canonicalized_from_lower_ir(&lower_program, &ssa_program, &ssa_error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: %s lower-ir canonicalized build failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&ssa_program, &actual_text)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: %s dump failed\n", case_id);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: %s lower-ir canonicalized dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
}

static int expect_built_memory_canonicalized_dump(const char *case_id,
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
            "[memory-ssa-pass] FAIL: %s lower-ir setup failed at %d:%d: %s\n",
            case_id,
            lower_error.line,
            lower_error.column,
            lower_error.message);
        return 0;
    }

    if (!memory_ssa_pass_build_memory_canonicalized_from_lower_ir(&lower_program,
            &ssa_program,
            &ssa_error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: %s lower-ir memory-value build failed at %d:%d: %s\n",
            case_id,
            ssa_error.line,
            ssa_error.column,
            ssa_error.message);
        lower_ir_program_free(&lower_program);
        return 0;
    }

    if (!value_ssa_dump_program(&ssa_program, &actual_text)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: %s dump failed\n", case_id);
        lower_ir_program_free(&lower_program);
        value_ssa_program_free(&ssa_program);
        return 0;
    }

    ok = strcmp(actual_text, expected_text) == 0;
    if (!ok) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: %s lower-ir memory-value dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
    }

    free(actual_text);
    lower_ir_program_free(&lower_program);
    value_ssa_program_free(&ssa_program);
    return ok;
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

static int build_partial_phi_forward_parameter_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t then_value;
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

    then_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (then_value == (size_t)-1 || join_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
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

    instruction.result = value_ssa_value_id(join_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_scrambled_partial_phi_forward_parameter_local_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t entry_id;
    size_t join_id;
    size_t then_id;
    size_t else_id;
    size_t then_value;
    size_t join_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, &entry, error) ||
        !value_ssa_function_append_block(function, &join_id, &join_block, error) ||
        !value_ssa_function_append_block(function, &then_id, &then_block, error) ||
        !value_ssa_function_append_block(function, &else_id, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    then_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (then_value == (size_t)-1 || join_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), then_id, else_id, error) ||
        !value_ssa_block_set_jump(else_block, join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, join_id, error)) {
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

static int build_partial_phi_forward_parameter_local_with_ancestor_value_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t entry_value;
    size_t then_move_value;
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

    entry_value = value_ssa_function_allocate_value(function);
    then_move_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (entry_value == (size_t)-1 || then_move_value == (size_t)-1 || join_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(entry_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_move_value);
    instruction.as.mov_value = value_ssa_value_id(entry_value);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_multi_pred_partial_phi_forward_parameter_local_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    size_t entry_id;
    size_t left_id;
    size_t split_id;
    size_t right_then_id;
    size_t right_else_id;
    size_t join_id;
    size_t left_load_value;
    size_t left_move_value;
    size_t join_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &left_id, NULL, error) ||
        !value_ssa_function_append_block(function, &split_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_then_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_else_id, NULL, error) ||
        !value_ssa_function_append_block(function, &join_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    left_load_value = value_ssa_function_allocate_value(function);
    left_move_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (left_load_value == (size_t)-1 || left_move_value == (size_t)-1 || join_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(&function->blocks[entry_id], value_ssa_value_immediate(1), left_id, split_id, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[split_id], value_ssa_value_immediate(1), right_then_id, right_else_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(left_load_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(left_move_value);
    instruction.as.mov_value = value_ssa_value_id(left_load_value);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[left_id], join_id, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_then_id], join_id, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_else_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[join_id], value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dominating_equivalent_phi_parameter_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t entry_id;
    size_t then_id;
    size_t else_id;
    size_t join_id;
    size_t cond_value;
    size_t then_value;
    size_t else_value;
    size_t join_phi_value;
    size_t join_move_value;
    size_t exit_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 2, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, &entry, error) ||
        !value_ssa_function_append_block(function, &then_id, &then_block, error) ||
        !value_ssa_function_append_block(function, &else_id, &else_block, error) ||
        !value_ssa_function_append_block(function, &join_id, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    then_value = value_ssa_function_allocate_value(function);
    else_value = value_ssa_function_allocate_value(function);
    join_phi_value = value_ssa_function_allocate_value(function);
    join_move_value = value_ssa_function_allocate_value(function);
    exit_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || then_value == (size_t)-1 || else_value == (size_t)-1 ||
        join_phi_value == (size_t)-1 || join_move_value == (size_t)-1 || exit_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), then_id, else_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = then_id;
    phi_inputs[0].value = value_ssa_value_id(then_value);
    phi_inputs[1].predecessor_block_id = else_id;
    phi_inputs[1].value = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_phi(join_block, join_phi_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_move_value);
    instruction.as.mov_value = value_ssa_value_id(join_phi_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(exit_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(exit_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_promote_parameter_local_with_ancestor_input_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t entry_value;
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

    entry_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (entry_value == (size_t)-1 || join_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(entry_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_id(entry_value);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
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

static int build_promote_global_with_ancestor_input_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_value;
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

    (void)global;
    entry_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (entry_value == (size_t)-1 || join_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(entry_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_id(entry_value);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_scalar_replace_global_join_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t cond_value;
    size_t join_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || join_value == (size_t)-1) {
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

    instruction.as.store.value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_scalar_replace_mixed_join_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t cond_value;
    size_t local_value;
    size_t global_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    local_value = value_ssa_function_allocate_value(function);
    global_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || local_value == (size_t)-1 || global_value == (size_t)-1 ||
        sum_value == (size_t)-1) {
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
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_value);
    instruction.as.binary.rhs = value_ssa_value_id(global_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_scalar_replace_mixed_multi_pred_join_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_id;
    size_t left_id;
    size_t split_id;
    size_t right_then_id;
    size_t right_else_id;
    size_t join_id;
    size_t cond_value;
    size_t split_cond_value;
    size_t local_value;
    size_t global_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "d", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &left_id, NULL, error) ||
        !value_ssa_function_append_block(function, &split_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_then_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_else_id, NULL, error) ||
        !value_ssa_function_append_block(function, &join_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    split_cond_value = value_ssa_function_allocate_value(function);
    local_value = value_ssa_function_allocate_value(function);
    global_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || split_cond_value == (size_t)-1 || local_value == (size_t)-1 ||
        global_value == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[entry_id], value_ssa_value_id(cond_value), left_id, split_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(split_cond_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(&function->blocks[split_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[split_id], value_ssa_value_id(split_cond_value), right_then_id, right_else_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[left_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_then_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(30);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_else_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_value);
    instruction.as.binary.rhs = value_ssa_value_id(global_value);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[join_id], value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_scalar_replace_mixed_loop_exit_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t cond_value;
    size_t local_value;
    size_t global_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    local_value = value_ssa_function_allocate_value(function);
    global_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || local_value == (size_t)-1 || global_value == (size_t)-1 ||
        sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_set_jump(entry, 1, error) ||
        !value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_value);
    instruction.as.binary.rhs = value_ssa_value_id(global_value);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_scalar_replace_mixed_global_barrier_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t cond_value;
    size_t local_value;
    size_t global_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    local_value = value_ssa_function_allocate_value(function);
    global_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || local_value == (size_t)-1 || global_value == (size_t)-1 ||
        sum_value == (size_t)-1) {
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
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error)) {
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
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_value);
    instruction.as.binary.rhs = value_ssa_value_id(global_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_scalar_replace_mixed_multi_pred_global_barrier_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_id;
    size_t left_id;
    size_t split_id;
    size_t right_then_id;
    size_t right_else_id;
    size_t join_id;
    size_t cond_value;
    size_t split_cond_value;
    size_t local_value;
    size_t global_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "d", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &left_id, NULL, error) ||
        !value_ssa_function_append_block(function, &split_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_then_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_else_id, NULL, error) ||
        !value_ssa_function_append_block(function, &join_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    split_cond_value = value_ssa_function_allocate_value(function);
    local_value = value_ssa_function_allocate_value(function);
    global_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || split_cond_value == (size_t)-1 || local_value == (size_t)-1 ||
        global_value == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[entry_id], value_ssa_value_id(cond_value), left_id, split_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(split_cond_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(&function->blocks[split_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[split_id], value_ssa_value_id(split_cond_value), right_then_id, right_else_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[left_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_then_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
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
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_else_id], join_id, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_value);
    instruction.as.load_slot = value_ssa_slot_local(2);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_value);
    instruction.as.binary.rhs = value_ssa_value_id(global_value);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[join_id], value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_scalar_replace_mixed_loop_global_barrier_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t cond_value;
    size_t local_value;
    size_t global_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    local_value = value_ssa_function_allocate_value(function);
    global_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || local_value == (size_t)-1 || global_value == (size_t)-1 ||
        sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
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

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_value);
    instruction.as.binary.rhs = value_ssa_value_id(global_value);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_pipeline_memory_aware_cse_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t cond_value;
    size_t local_value0;
    size_t global_value0;
    size_t sum0;
    size_t local_value1;
    size_t global_value1;
    size_t sum1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    local_value0 = value_ssa_function_allocate_value(function);
    global_value0 = value_ssa_function_allocate_value(function);
    sum0 = value_ssa_function_allocate_value(function);
    local_value1 = value_ssa_function_allocate_value(function);
    global_value1 = value_ssa_function_allocate_value(function);
    sum1 = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || local_value0 == (size_t)-1 || global_value0 == (size_t)-1 ||
        sum0 == (size_t)-1 || local_value1 == (size_t)-1 || global_value1 == (size_t)-1 ||
        sum1 == (size_t)-1) {
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
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_value0);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_value0);
    instruction.as.binary.rhs = value_ssa_value_id(global_value0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_value1);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_value1);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_value1);
    instruction.as.binary.rhs = value_ssa_value_id(global_value1);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_pipeline_memory_aware_nested_join_cse_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t cond_value;
    size_t local_value0;
    size_t global_value0;
    size_t sum0;
    size_t final_sum;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    local_value0 = value_ssa_function_allocate_value(function);
    global_value0 = value_ssa_function_allocate_value(function);
    sum0 = value_ssa_function_allocate_value(function);
    final_sum = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || local_value0 == (size_t)-1 || global_value0 == (size_t)-1 ||
        sum0 == (size_t)-1 || final_sum == (size_t)-1) {
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
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(1);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_value0);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_value0);
    instruction.as.binary.rhs = value_ssa_value_id(global_value0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(final_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(sum0);
    instruction.as.binary.rhs = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(final_sum), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_memory_aware_cse_mixed_multi_pred_global_barrier_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_id;
    size_t left_id;
    size_t split_id;
    size_t right_then_id;
    size_t right_else_id;
    size_t join_id;
    size_t cond_value;
    size_t split_cond_value;
    size_t local_value0;
    size_t global_value0;
    size_t sum0;
    size_t local_value1;
    size_t global_value1;
    size_t sum1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "d", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &left_id, NULL, error) ||
        !value_ssa_function_append_block(function, &split_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_then_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_else_id, NULL, error) ||
        !value_ssa_function_append_block(function, &join_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    split_cond_value = value_ssa_function_allocate_value(function);
    local_value0 = value_ssa_function_allocate_value(function);
    global_value0 = value_ssa_function_allocate_value(function);
    sum0 = value_ssa_function_allocate_value(function);
    local_value1 = value_ssa_function_allocate_value(function);
    global_value1 = value_ssa_function_allocate_value(function);
    sum1 = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || split_cond_value == (size_t)-1 || local_value0 == (size_t)-1 ||
        global_value0 == (size_t)-1 || sum0 == (size_t)-1 || local_value1 == (size_t)-1 ||
        global_value1 == (size_t)-1 || sum1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[entry_id], value_ssa_value_id(cond_value), left_id, split_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(split_cond_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(&function->blocks[split_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[split_id], value_ssa_value_id(split_cond_value), right_then_id, right_else_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[left_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_then_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
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
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_else_id], join_id, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_value0);
    instruction.as.load_slot = value_ssa_slot_local(2);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_value0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_value0);
    instruction.as.binary.rhs = value_ssa_value_id(global_value0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(local_value1);
    instruction.as.load_slot = value_ssa_slot_local(2);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(global_value1);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(local_value1);
    instruction.as.binary.rhs = value_ssa_value_id(global_value1);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[join_id], value_ssa_value_id(sum1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_memory_aware_cse_partial_unknown_predecessor_binary_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_id;
    size_t left_id;
    size_t split_id;
    size_t right_then_id;
    size_t right_else_id;
    size_t join_id;
    size_t cond_value;
    size_t split_cond_value;
    size_t barrier_global_value;
    size_t barrier_sum;
    size_t join_local_value;
    size_t join_global_value;
    size_t join_sum;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "d", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &left_id, NULL, error) ||
        !value_ssa_function_append_block(function, &split_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_then_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_else_id, NULL, error) ||
        !value_ssa_function_append_block(function, &join_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    split_cond_value = value_ssa_function_allocate_value(function);
    barrier_global_value = value_ssa_function_allocate_value(function);
    barrier_sum = value_ssa_function_allocate_value(function);
    join_local_value = value_ssa_function_allocate_value(function);
    join_global_value = value_ssa_function_allocate_value(function);
    join_sum = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || split_cond_value == (size_t)-1 || barrier_global_value == (size_t)-1 ||
        barrier_sum == (size_t)-1 || join_local_value == (size_t)-1 || join_global_value == (size_t)-1 ||
        join_sum == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[entry_id], value_ssa_value_id(cond_value), left_id, split_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(split_cond_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(&function->blocks[split_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[split_id], value_ssa_value_id(split_cond_value), right_then_id, right_else_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[left_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_then_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
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
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(barrier_global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(barrier_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_immediate(3);
    instruction.as.binary.rhs = value_ssa_value_id(barrier_global_value);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_else_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_local_value);
    instruction.as.load_slot = value_ssa_slot_local(2);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(join_local_value);
    instruction.as.binary.rhs = value_ssa_value_id(join_global_value);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[join_id], value_ssa_value_id(join_sum), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_memory_aware_cse_partial_unknown_materialized_binary_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_id;
    size_t left_id;
    size_t split_id;
    size_t right_then_id;
    size_t right_else_id;
    size_t join_id;
    size_t cond_value;
    size_t split_cond_value;
    size_t join_local_value;
    size_t join_global_value;
    size_t join_sum;
    size_t join_final_sum;
    size_t barrier_global_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "d", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &left_id, NULL, error) ||
        !value_ssa_function_append_block(function, &split_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_then_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_else_id, NULL, error) ||
        !value_ssa_function_append_block(function, &join_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    split_cond_value = value_ssa_function_allocate_value(function);
    join_local_value = value_ssa_function_allocate_value(function);
    join_global_value = value_ssa_function_allocate_value(function);
    join_sum = value_ssa_function_allocate_value(function);
    join_final_sum = value_ssa_function_allocate_value(function);
    barrier_global_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || split_cond_value == (size_t)-1 || join_local_value == (size_t)-1 ||
        join_global_value == (size_t)-1 || join_sum == (size_t)-1 || join_final_sum == (size_t)-1 ||
        barrier_global_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[entry_id], value_ssa_value_id(cond_value), left_id, split_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(split_cond_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(&function->blocks[split_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[split_id], value_ssa_value_id(split_cond_value), right_then_id, right_else_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[left_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_then_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(2);
    instruction.as.store.value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
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
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(barrier_global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_else_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_local_value);
    instruction.as.load_slot = value_ssa_slot_local(2);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(join_local_value);
    instruction.as.binary.rhs = value_ssa_value_id(join_global_value);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_final_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(join_sum);
    instruction.as.binary.rhs = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[join_id], value_ssa_value_id(join_final_sum), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_memory_aware_cse_branch_materialized_partial_unknown_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_id;
    size_t left_id;
    size_t split_id;
    size_t right_then_id;
    size_t right_else_id;
    size_t join_id;
    size_t exit_id;
    size_t cond_value;
    size_t split_cond_value;
    size_t branch_cond_value;
    size_t join_local_value;
    size_t join_global_value;
    size_t join_sum;
    size_t barrier_global_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "d", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "e", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &left_id, NULL, error) ||
        !value_ssa_function_append_block(function, &split_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_then_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_else_id, NULL, error) ||
        !value_ssa_function_append_block(function, &join_id, NULL, error) ||
        !value_ssa_function_append_block(function, &exit_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    split_cond_value = value_ssa_function_allocate_value(function);
    branch_cond_value = value_ssa_function_allocate_value(function);
    join_local_value = value_ssa_function_allocate_value(function);
    join_global_value = value_ssa_function_allocate_value(function);
    join_sum = value_ssa_function_allocate_value(function);
    barrier_global_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || split_cond_value == (size_t)-1 || branch_cond_value == (size_t)-1 ||
        join_local_value == (size_t)-1 || join_global_value == (size_t)-1 || join_sum == (size_t)-1 ||
        barrier_global_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[entry_id], value_ssa_value_id(cond_value), left_id, split_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(split_cond_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(&function->blocks[split_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[split_id], value_ssa_value_id(split_cond_value), right_then_id, right_else_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(3);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[left_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(3);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_then_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(3);
    instruction.as.store.value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
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
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(barrier_global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(branch_cond_value);
    instruction.as.load_slot = value_ssa_slot_local(2);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[right_else_id], value_ssa_value_id(branch_cond_value), join_id, exit_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_local_value);
    instruction.as.load_slot = value_ssa_slot_local(3);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(join_local_value);
    instruction.as.binary.rhs = value_ssa_value_id(join_global_value);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[join_id], value_ssa_value_id(join_sum), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_return(&function->blocks[exit_id], value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_memory_aware_cse_branch_materialized_nested_partial_unknown_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_id;
    size_t left_id;
    size_t split_id;
    size_t right_then_id;
    size_t right_else_id;
    size_t join_id;
    size_t exit_id;
    size_t cond_value;
    size_t split_cond_value;
    size_t branch_cond_value;
    size_t join_local_value;
    size_t join_global_value;
    size_t join_sum;
    size_t join_final_sum;
    size_t barrier_global_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "d", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "e", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &left_id, NULL, error) ||
        !value_ssa_function_append_block(function, &split_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_then_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_else_id, NULL, error) ||
        !value_ssa_function_append_block(function, &join_id, NULL, error) ||
        !value_ssa_function_append_block(function, &exit_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    split_cond_value = value_ssa_function_allocate_value(function);
    branch_cond_value = value_ssa_function_allocate_value(function);
    join_local_value = value_ssa_function_allocate_value(function);
    join_global_value = value_ssa_function_allocate_value(function);
    join_sum = value_ssa_function_allocate_value(function);
    join_final_sum = value_ssa_function_allocate_value(function);
    barrier_global_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || split_cond_value == (size_t)-1 || branch_cond_value == (size_t)-1 ||
        join_local_value == (size_t)-1 || join_global_value == (size_t)-1 || join_sum == (size_t)-1 ||
        join_final_sum == (size_t)-1 || barrier_global_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[entry_id], value_ssa_value_id(cond_value), left_id, split_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(split_cond_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(&function->blocks[split_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[split_id], value_ssa_value_id(split_cond_value), right_then_id, right_else_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(3);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[left_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(3);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_then_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(3);
    instruction.as.store.value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
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
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(barrier_global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(branch_cond_value);
    instruction.as.load_slot = value_ssa_slot_local(2);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[right_else_id], value_ssa_value_id(branch_cond_value), join_id, exit_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_local_value);
    instruction.as.load_slot = value_ssa_slot_local(3);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(join_local_value);
    instruction.as.binary.rhs = value_ssa_value_id(join_global_value);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_final_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(join_sum);
    instruction.as.binary.rhs = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[join_id], value_ssa_value_id(join_final_sum), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_return(&function->blocks[exit_id], value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_memory_aware_cse_branch_materialized_deep_partial_unknown_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    static const long long tail_constants[] = {5, 7, 9, 11, 13};
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_id;
    size_t left_id;
    size_t split_id;
    size_t right_then_id;
    size_t right_else_id;
    size_t join_id;
    size_t exit_id;
    size_t cond_value;
    size_t split_cond_value;
    size_t branch_cond_value;
    size_t join_local_value;
    size_t join_global_value;
    size_t join_sum;
    size_t barrier_global_value;
    size_t tail_results[sizeof(tail_constants) / sizeof(tail_constants[0])];
    size_t current_value;
    size_t index;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "d", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "e", 1, NULL, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &left_id, NULL, error) ||
        !value_ssa_function_append_block(function, &split_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_then_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_else_id, NULL, error) ||
        !value_ssa_function_append_block(function, &join_id, NULL, error) ||
        !value_ssa_function_append_block(function, &exit_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    split_cond_value = value_ssa_function_allocate_value(function);
    branch_cond_value = value_ssa_function_allocate_value(function);
    join_local_value = value_ssa_function_allocate_value(function);
    join_global_value = value_ssa_function_allocate_value(function);
    join_sum = value_ssa_function_allocate_value(function);
    for (index = 0; index < (sizeof(tail_constants) / sizeof(tail_constants[0])); ++index) {
        tail_results[index] = value_ssa_function_allocate_value(function);
    }
    barrier_global_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || split_cond_value == (size_t)-1 || branch_cond_value == (size_t)-1 ||
        join_local_value == (size_t)-1 || join_global_value == (size_t)-1 || join_sum == (size_t)-1 ||
        barrier_global_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }
    for (index = 0; index < (sizeof(tail_constants) / sizeof(tail_constants[0])); ++index) {
        if (tail_results[index] == (size_t)-1) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[entry_id], value_ssa_value_id(cond_value), left_id, split_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(split_cond_value);
    instruction.as.load_slot = value_ssa_slot_local(1);
    if (!value_ssa_block_append_instruction(&function->blocks[split_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[split_id], value_ssa_value_id(split_cond_value), right_then_id, right_else_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(3);
    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[left_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(3);
    instruction.as.store.value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.as.store.value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(&function->blocks[right_then_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_then_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(3);
    instruction.as.store.value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
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
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(barrier_global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(branch_cond_value);
    instruction.as.load_slot = value_ssa_slot_local(2);
    if (!value_ssa_block_append_instruction(&function->blocks[right_else_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[right_else_id], value_ssa_value_id(branch_cond_value), join_id, exit_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_local_value);
    instruction.as.load_slot = value_ssa_slot_local(3);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_global_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(join_local_value);
    instruction.as.binary.rhs = value_ssa_value_id(join_global_value);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    current_value = join_sum;
    for (index = 0; index < (sizeof(tail_constants) / sizeof(tail_constants[0])); ++index) {
        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_BINARY;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(tail_results[index]);
        instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
        instruction.as.binary.lhs = value_ssa_value_id(current_value);
        instruction.as.binary.rhs = value_ssa_value_immediate(tail_constants[index]);
        if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
        current_value = tail_results[index];
    }

    if (!value_ssa_block_set_return(&function->blocks[join_id], value_ssa_value_id(current_value), error) ||
        !value_ssa_block_set_return(&function->blocks[exit_id], value_ssa_value_immediate(0), error)) {
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

static int build_dominating_equivalent_phi_global_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    ValueSsaGlobal *global = NULL;
    size_t entry_id;
    size_t then_id;
    size_t else_id;
    size_t join_id;
    size_t cond_value;
    size_t then_value;
    size_t else_value;
    size_t join_phi_value;
    size_t join_move_value;
    size_t exit_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "c", 1, NULL, error) ||
        !value_ssa_function_append_block(function, &entry_id, &entry, error) ||
        !value_ssa_function_append_block(function, &then_id, &then_block, error) ||
        !value_ssa_function_append_block(function, &else_id, &else_block, error) ||
        !value_ssa_function_append_block(function, &join_id, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    cond_value = value_ssa_function_allocate_value(function);
    then_value = value_ssa_function_allocate_value(function);
    else_value = value_ssa_function_allocate_value(function);
    join_phi_value = value_ssa_function_allocate_value(function);
    join_move_value = value_ssa_function_allocate_value(function);
    exit_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || then_value == (size_t)-1 || else_value == (size_t)-1 ||
        join_phi_value == (size_t)-1 || join_move_value == (size_t)-1 || exit_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), then_id, else_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = then_id;
    phi_inputs[0].value = value_ssa_value_id(then_value);
    phi_inputs[1].predecessor_block_id = else_id;
    phi_inputs[1].value = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_phi(join_block, join_phi_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_move_value);
    instruction.as.mov_value = value_ssa_value_id(join_phi_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(exit_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(exit_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_partial_phi_forward_global_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t then_value;
    size_t join_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    then_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (then_value == (size_t)-1 || join_value == (size_t)-1) {
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
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
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

static int build_scrambled_partial_phi_forward_global_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_id;
    size_t join_id;
    size_t then_id;
    size_t else_id;
    size_t then_value;
    size_t join_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, &entry_id, &entry, error) ||
        !value_ssa_function_append_block(function, &join_id, &join_block, error) ||
        !value_ssa_function_append_block(function, &then_id, &then_block, error) ||
        !value_ssa_function_append_block(function, &else_id, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    then_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (then_value == (size_t)-1 || join_value == (size_t)-1) {
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
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), then_id, else_id, error) ||
        !value_ssa_block_set_jump(else_block, join_id, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, join_id, error)) {
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

static int build_partial_phi_forward_global_with_ancestor_value_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_value;
    size_t then_move_value;
    size_t join_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    entry_value = value_ssa_function_allocate_value(function);
    then_move_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (entry_value == (size_t)-1 || then_move_value == (size_t)-1 || join_value == (size_t)-1) {
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
    instruction.result = value_ssa_value_id(entry_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(then_move_value);
    instruction.as.mov_value = value_ssa_value_id(entry_value);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_multi_pred_partial_phi_forward_global_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *declare_function = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaInstruction instruction;
    ValueSsaGlobal *global = NULL;
    size_t entry_id;
    size_t left_id;
    size_t split_id;
    size_t right_then_id;
    size_t right_else_id;
    size_t join_id;
    size_t left_load_value;
    size_t left_move_value;
    size_t join_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", &global, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &left_id, NULL, error) ||
        !value_ssa_function_append_block(function, &split_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_then_id, NULL, error) ||
        !value_ssa_function_append_block(function, &right_else_id, NULL, error) ||
        !value_ssa_function_append_block(function, &join_id, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    (void)global;
    left_load_value = value_ssa_function_allocate_value(function);
    left_move_value = value_ssa_function_allocate_value(function);
    join_value = value_ssa_function_allocate_value(function);
    if (left_load_value == (size_t)-1 || left_move_value == (size_t)-1 || join_value == (size_t)-1) {
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
    if (!value_ssa_block_append_instruction(&function->blocks[entry_id], &instruction, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[entry_id], value_ssa_value_immediate(1), left_id, split_id, error) ||
        !value_ssa_block_set_branch(
            &function->blocks[split_id], value_ssa_value_immediate(1), right_then_id, right_else_id, error)) {
        free(instruction.as.call.callee_name);
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(left_load_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(left_move_value);
    instruction.as.mov_value = value_ssa_value_id(left_load_value);
    if (!value_ssa_block_append_instruction(&function->blocks[left_id], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[left_id], join_id, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_then_id], join_id, error) ||
        !value_ssa_block_set_jump(&function->blocks[right_else_id], join_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_value);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(&function->blocks[join_id], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[join_id], value_ssa_value_id(join_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_loop_header_partial_parameter_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
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

    header_value = value_ssa_function_allocate_value(function);
    body_value = value_ssa_function_allocate_value(function);
    if (header_value == (size_t)-1 || body_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(header_value);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_immediate(1), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(body_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(header_value), error)) {
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

static int build_nonredundant_parameter_local_loop_phi_store_program(ValueSsaProgram *program, ValueSsaError *error) {
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
    instruction.as.mov_value = value_ssa_value_immediate(99);
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

static int test_memory_ssa_pass_reuses_dominating_equivalent_phi_parameter_local_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_dominating_equivalent_phi_parameter_local_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: dominating-equivalent parameter-local setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: dominating-equivalent parameter-local forward failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-DOM-EQUIV-PARAM-LOCAL",
        &program,
        "func main(a.0, c.1) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.1\n"
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
        "    ssa.5 = mov ssa.4\n"
        "    ret ssa.5\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_partially_redundant_parameter_local_join_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_partial_phi_forward_parameter_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: partial phi-forward parameter-local setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: partial phi-forward parameter-local forward failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PARTIAL-PHI-PARAM-LOCAL",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.0 = load_local a.0\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = load_local a.0\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: ssa.0], [bb.2: ssa.2]\n"
        "    ssa.1 = mov ssa.3\n"
        "    ret ssa.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_partially_redundant_scrambled_parameter_local_join_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_scrambled_partial_phi_forward_parameter_local_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: scrambled partial phi-forward parameter-local setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: scrambled partial phi-forward parameter-local forward failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PARTIAL-PHI-PARAM-LOCAL-SCRAMBLED",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.1:\n"
        "    ssa.3 = phi [bb.2: ssa.0], [bb.3: ssa.2]\n"
        "    ssa.1 = mov ssa.3\n"
        "    ret ssa.1\n"
        "  bb.2:\n"
        "    ssa.0 = load_local a.0\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.2 = load_local a.0\n"
        "    jmp bb.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_partially_redundant_parameter_local_join_prefers_ancestor_value(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_partial_phi_forward_parameter_local_with_ancestor_value_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: partial phi-forward parameter-local ancestor setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: partial phi-forward parameter-local ancestor forward failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PARTIAL-PHI-PARAM-LOCAL-ANCESTOR",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = mov ssa.0\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.2 = mov ssa.0\n"
        "    ret ssa.2\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_partially_redundant_multi_pred_parameter_local_join_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_multi_pred_partial_phi_forward_parameter_local_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: multi-pred partial phi-forward parameter-local setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: multi-pred partial phi-forward parameter-local forward failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PARTIAL-PHI-PARAM-LOCAL-MULTI",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.0 = load_local a.0\n"
        "    ssa.1 = mov ssa.0\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    br 1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    ssa.3 = load_local a.0\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    ssa.4 = load_local a.0\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    ssa.5 = phi [bb.1: ssa.1], [bb.3: ssa.3], [bb.4: ssa.4]\n"
        "    ssa.2 = mov ssa.5\n"
        "    ret ssa.2\n"
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

static int test_memory_ssa_pass_promotes_same_value_loop_global_slot(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_same_value_loop_global_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: promote same-value loop global setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_promote_global_slots(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: promote same-value loop global pass failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PROMOTE-SAME-VALUE-LOOP-GLOBAL",
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
        "    ssa.1 = phi [bb.0: 7], [bb.2: 9]\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    store_local a.0, 9\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.0 = mov ssa.1\n"
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
        "    ssa.1 = mov 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    store_local a.0, 2\n"
        "    ssa.2 = mov 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: ssa.1], [bb.2: ssa.2]\n"
        "    ssa.0 = mov ssa.3\n"
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

static int test_memory_ssa_pass_promotes_same_value_phi_global_slot(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_same_value_phi_global_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: promote same-value phi global setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_promote_global_slots(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: promote same-value phi global pass failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PROMOTE-SAME-VALUE-PHI-GLOBAL",
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

static int test_memory_ssa_pass_promotes_global_slot_using_ancestor_input_value(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_promote_global_with_ancestor_input_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: promote global ancestor-input setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_promote_global_slots(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: promote global ancestor-input pass failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PROMOTE-GLOBAL-ANCESTOR-INPUT",
        &program,
        "global g.0\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = load_global g.0\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_global g.0, ssa.0\n"
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

static int test_memory_ssa_pass_reuses_dominating_equivalent_phi_global_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_dominating_equivalent_phi_global_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: dominating-equivalent global setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: dominating-equivalent global forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-DOM-EQUIV-GLOBAL",
        &program,
        "global g.0\n"
        "\n"
        "func main(c.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = load_global g.0\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = load_global g.0\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: ssa.1], [bb.2: ssa.2]\n"
        "    ssa.4 = mov ssa.3\n"
        "    ssa.5 = mov ssa.4\n"
        "    ret ssa.5\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_partially_redundant_global_join_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_partial_phi_forward_global_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: partial phi-forward global setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: partial phi-forward global forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PARTIAL-PHI-GLOBAL",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    call touch()\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.0 = load_global g.0\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = load_global g.0\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.3 = phi [bb.1: ssa.0], [bb.2: ssa.2]\n"
        "    ssa.1 = mov ssa.3\n"
        "    ret ssa.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_partially_redundant_scrambled_global_join_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_scrambled_partial_phi_forward_global_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scrambled partial phi-forward global setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scrambled partial phi-forward global forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PARTIAL-PHI-GLOBAL-SCRAMBLED",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    call touch()\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.1:\n"
        "    ssa.3 = phi [bb.2: ssa.0], [bb.3: ssa.2]\n"
        "    ssa.1 = mov ssa.3\n"
        "    ret ssa.1\n"
        "  bb.2:\n"
        "    ssa.0 = load_global g.0\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.2 = load_global g.0\n"
        "    jmp bb.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_partially_redundant_global_join_prefers_ancestor_value(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_partial_phi_forward_global_with_ancestor_value_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: partial phi-forward global ancestor setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: partial phi-forward global ancestor forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PARTIAL-PHI-GLOBAL-ANCESTOR",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    call touch()\n"
        "    ssa.0 = load_global g.0\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.1 = mov ssa.0\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.2 = mov ssa.0\n"
        "    ret ssa.2\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_partially_redundant_multi_pred_global_join_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_multi_pred_partial_phi_forward_global_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: multi-pred partial phi-forward global setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_global_loads(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: multi-pred partial phi-forward global forward failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-PARTIAL-PHI-GLOBAL-MULTI",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  bb.0:\n"
        "    call touch()\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    ssa.0 = load_global g.0\n"
        "    ssa.1 = mov ssa.0\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    br 1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    ssa.3 = load_global g.0\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    ssa.4 = load_global g.0\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    ssa.5 = phi [bb.1: ssa.1], [bb.3: ssa.3], [bb.4: ssa.4]\n"
        "    ssa.2 = mov ssa.5\n"
        "    ret ssa.2\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_keeps_loop_header_partial_parameter_local_load(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_loop_header_partial_parameter_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: loop-header partial parameter-local setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_forward_local_loads(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: loop-header partial parameter-local forward failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-FWD-LOOP-PARTIAL-PARAM-LOCAL",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.0 = load_local a.0\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.1 = mov ssa.0\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_promotes_differing_value_loop_local_slot(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_differing_value_loop_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: promote loop local setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_promote_local_slots(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: promote loop local pass failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PROMOTE-LOOP-LOCAL",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    store_local a.0, 7\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = phi [bb.0: 7], [bb.2: 9]\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    store_local a.0, 9\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.0 = mov ssa.1\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_promotes_join_local_slot_to_value_phi(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_phi_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: promote join local setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_promote_local_slots(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: promote join local pass failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PROMOTE-JOIN-LOCAL",
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
        "    ssa.1 = phi [bb.1: 1], [bb.2: 2]\n"
        "    ssa.0 = mov ssa.1\n"
        "    ret ssa.0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_promotes_local_slot_using_ancestor_input_value(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_promote_parameter_local_with_ancestor_input_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: promote local ancestor-input setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_promote_local_slots(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: promote local ancestor-input pass failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PROMOTE-LOCAL-ANCESTOR-INPUT",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    store_local a.0, ssa.0\n"
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

static int test_memory_ssa_pass_keeps_loop_global_load_after_call_for_global_promotion(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_loop_global_call_barrier_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: promote loop global-call setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_promote_global_slots(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: promote loop global-call pass failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-KEEP-LOOP-GLOBAL-CALL-PROMOTE",
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

static int test_memory_ssa_pass_eliminates_redundant_parameter_local_store_locally(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_parameter_local_store_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant parameter-local local-only setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_local_stores(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant parameter-local local-only elimination failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-REDUNDANT-PARAM-LOCAL-ONLY",
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

static int test_memory_ssa_pass_keeps_nonredundant_parameter_local_loop_phi_store(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_nonredundant_parameter_local_loop_phi_store_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: nonredundant parameter-local loop phi store setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_stores(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: nonredundant parameter-local loop phi store elimination failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-KEEP-PARAM-LOCAL-LOOP-PHI",
        &program,
        "func main(a.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.1 = phi [bb.0: ssa.0], [bb.2: ssa.2]\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    ssa.2 = mov 99\n"
        "    store_local a.0, ssa.2\n"
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

static int test_memory_ssa_pass_eliminates_redundant_global_store_after_load_globally(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_redundant_global_store_after_load_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant global global-only setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_global_stores(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: redundant global global-only elimination failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-REDUNDANT-GLOBAL-ONLY",
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

static int test_memory_ssa_pass_eliminates_dead_global_store_globally(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_dead_global_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: dead global global-only setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_dead_global_stores(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: dead global global-only elimination failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-DEAD-GLOBAL-ONLY",
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

static int test_memory_ssa_pass_scalar_replace_local_slots_join(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_phi_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace local join setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_scalar_replace_local_slots(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace local join failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-SCALAR-REPLACE-LOCAL-JOIN",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_scalar_replace_global_slots_join(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_scalar_replace_global_join_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace global join setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_scalar_replace_global_slots(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace global join failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-SCALAR-REPLACE-GLOBAL-JOIN",
        &program,
        "global g.0\n"
        "\n"
        "func main(c.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 7], [bb.2: 9]\n"
        "    ret ssa.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_scalar_replace_slots_mixed_join(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_scalar_replace_mixed_join_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace mixed join setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_scalar_replace_slots(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace mixed join failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-SCALAR-REPLACE-MIXED-JOIN",
        &program,
        "global g.0\n"
        "\n"
        "func main(c.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 1], [bb.2: 2]\n"
        "    ssa.2 = phi [bb.1: 10], [bb.2: 20]\n"
        "    ssa.3 = add ssa.1, ssa.2\n"
        "    ret ssa.3\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_scalar_replace_slots_mixed_multi_pred_join(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_scalar_replace_mixed_multi_pred_join_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace mixed multi-pred setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_scalar_replace_slots(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace mixed multi-pred failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-SCALAR-REPLACE-MIXED-MULTI",
        &program,
        "global g.0\n"
        "\n"
        "func main(c.0, d.1) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    ssa.2 = phi [bb.1: 1], [bb.3: 2], [bb.4: 3]\n"
        "    ssa.3 = phi [bb.1: 10], [bb.3: 20], [bb.4: 30]\n"
        "    ssa.4 = add ssa.2, ssa.3\n"
        "    ret ssa.4\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_scalar_replace_slots_mixed_loop_exit(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_scalar_replace_mixed_loop_exit_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace mixed loop-exit setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_scalar_replace_slots(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace mixed loop-exit failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-SCALAR-REPLACE-MIXED-LOOP",
        &program,
        "global g.0\n"
        "\n"
        "func main(c.0) {\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.0 = phi [bb.0: 1], [bb.2: 2]\n"
        "    ssa.1 = phi [bb.0: 10], [bb.2: 20]\n"
        "    ssa.2 = load_local c.0\n"
        "    br ssa.2, bb.2, bb.3\n"
        "  bb.2:\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.3 = add ssa.0, ssa.1\n"
        "    ret ssa.3\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_scalar_replace_slots_keeps_global_call_barrier(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_scalar_replace_mixed_global_barrier_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace mixed global-barrier setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_scalar_replace_slots(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: scalar-replace mixed global-barrier failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-SCALAR-REPLACE-MIXED-GLOBAL-BARRIER",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    call touch()\n"
        "    ssa.1 = load_global g.0\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.2 = phi [bb.1: 1], [bb.2: 2]\n"
        "    ssa.3 = phi [bb.1: 10], [bb.2: ssa.1]\n"
        "    ssa.4 = add ssa.2, ssa.3\n"
        "    ret ssa.4\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_scalar_replace_slots_mixed_multi_pred_global_barrier(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_scalar_replace_mixed_multi_pred_global_barrier_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: scalar-replace mixed multi-pred global-barrier setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_scalar_replace_slots(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: scalar-replace mixed multi-pred global-barrier failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-SCALAR-REPLACE-MIXED-MULTI-GLOBAL-BARRIER",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    ssa.3 = phi [bb.1: 1], [bb.3: 2], [bb.4: 3]\n"
        "    ssa.4 = phi [bb.1: 10], [bb.3: 20], [bb.4: ssa.2]\n"
        "    ssa.5 = add ssa.3, ssa.4\n"
        "    ret ssa.5\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_scalar_replace_slots_mixed_loop_global_barrier(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_scalar_replace_mixed_loop_global_barrier_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: scalar-replace mixed loop global-barrier setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_scalar_replace_slots(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: scalar-replace mixed loop global-barrier failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-SCALAR-REPLACE-MIXED-LOOP-GLOBAL-BARRIER",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0) {\n"
        "  bb.0:\n"
        "    store_global g.0, 10\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    ssa.0 = phi [bb.0: 1], [bb.2: 2]\n"
        "    ssa.1 = load_local c.0\n"
        "    br ssa.1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    call touch()\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = add ssa.0, ssa.2\n"
        "    ret ssa.3\n"
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

static int test_memory_ssa_pass_build_canonicalized_from_lower_ir_local_join(void) {
    return expect_built_canonicalized_dump("MEMORY-SSA-PASS-BUILD-CANONICALIZE-LOCAL-JOIN",
        build_lower_ir_join_preserved_local_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 7\n"
        "}\n");
}

static int test_memory_ssa_pass_build_memory_canonicalized_from_lower_ir_fold_program(void) {
    return expect_built_memory_canonicalized_dump("MEMORY-SSA-PASS-BUILD-MEMVAL-FOLD",
        build_lower_ir_memory_fold_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ssa.0 = add 7, 5\n"
        "    ret ssa.0\n"
        "}\n");
}

static int test_memory_ssa_pass_build_canonicalized_from_lower_ir_fold_program(void) {
    return expect_built_canonicalized_dump("MEMORY-SSA-PASS-BUILD-CANONICALIZE-FOLD",
        build_lower_ir_memory_fold_program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 12\n"
        "}\n");
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

static int test_memory_ssa_pass_pipeline_eliminates_memory_aware_redundant_binary(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_pipeline_memory_aware_cse_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline memory-aware cse setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline memory-aware cse run failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-MEMORY-AWARE-CSE",
        &program,
        "global g.0\n"
        "\n"
        "func main(c.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 11], [bb.2: 22]\n"
        "    ret ssa.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_memory_binaries(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_pipeline_memory_aware_cse_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: memory-aware cse setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_memory_binaries(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: memory-aware cse failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-MEMORY-AWARE-CSE",
        &program,
        "global g.0\n"
        "\n"
        "func main(c.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 11], [bb.2: 22]\n"
        "    ret ssa.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_nested_join_memory_binaries(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_pipeline_memory_aware_nested_join_cse_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: nested memory-aware cse setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_memory_binaries(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: nested memory-aware cse failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-MEMORY-AWARE-CSE-NESTED",
        &program,
        "global g.0\n"
        "\n"
        "func main(c.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 16], [bb.2: 27]\n"
        "    ret ssa.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_global_barrier(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_memory_aware_cse_mixed_multi_pred_global_barrier_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: memory-aware cse barrier setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_memory_binaries(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: memory-aware cse barrier failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-MEMORY-AWARE-CSE-GLOBAL-BARRIER",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = add ssa.2, 3\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    ssa.4 = phi [bb.1: 11], [bb.3: 22], [bb.4: ssa.3]\n"
        "    ret ssa.4\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_partial_unknown_predecessor_binary(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_memory_aware_cse_partial_unknown_predecessor_binary_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-aware cse partial-unknown setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_memory_binaries(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-aware cse partial-unknown failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-MEMORY-AWARE-CSE-PARTIAL-UNKNOWN-PRED-BINARY",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = add ssa.2, 3\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    ssa.4 = phi [bb.1: 11], [bb.3: 22], [bb.4: ssa.3]\n"
        "    ret ssa.4\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_materialized_partial_unknown_edge(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_memory_aware_cse_partial_unknown_materialized_binary_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-aware cse materialized partial-unknown setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_memory_binaries(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-aware cse materialized partial-unknown failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-MEMORY-AWARE-CSE-MATERIALIZED-PARTIAL-UNKNOWN",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = add ssa.2, 3\n"
        "    ssa.4 = add ssa.3, 5\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    ssa.5 = phi [bb.1: 16], [bb.3: 27], [bb.4: ssa.4]\n"
        "    ret ssa.5\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_branch_materialized_partial_unknown_edge(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_memory_aware_cse_branch_materialized_partial_unknown_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-aware cse branch materialized partial-unknown setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_memory_binaries(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-aware cse branch materialized partial-unknown failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-MEMORY-AWARE-CSE-BRANCH-MATERIALIZED-PARTIAL-UNKNOWN",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1, e.2) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = load_local e.2\n"
        "    ssa.4 = add ssa.2, 3\n"
        "    br ssa.3, bb.5, bb.6\n"
        "  bb.5:\n"
        "    ssa.5 = phi [bb.1: 11], [bb.3: 22], [bb.4: ssa.4]\n"
        "    ret ssa.5\n"
        "  bb.6:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_branch_materialized_nested_partial_unknown_edge(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_memory_aware_cse_branch_materialized_nested_partial_unknown_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-aware cse branch materialized nested partial-unknown setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_memory_binaries(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-aware cse branch materialized nested partial-unknown failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-MEMORY-AWARE-CSE-BRANCH-MATERIALIZED-NESTED-PARTIAL-UNKNOWN",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1, e.2) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = load_local e.2\n"
        "    ssa.4 = add ssa.2, 3\n"
        "    ssa.5 = add ssa.4, 5\n"
        "    br ssa.3, bb.5, bb.6\n"
        "  bb.5:\n"
        "    ssa.6 = phi [bb.1: 16], [bb.3: 27], [bb.4: ssa.5]\n"
        "    ret ssa.6\n"
        "  bb.6:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_branch_materialized_deep_partial_unknown_edge(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_memory_aware_cse_branch_materialized_deep_partial_unknown_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-aware cse branch materialized deep partial-unknown setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_eliminate_redundant_memory_binaries(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: memory-aware cse branch materialized deep partial-unknown failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-MEMORY-AWARE-CSE-BRANCH-MATERIALIZED-DEEP-PARTIAL-UNKNOWN",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1, e.2) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = load_local e.2\n"
        "    ssa.4 = add ssa.2, 3\n"
        "    ssa.5 = add ssa.4, 5\n"
        "    ssa.6 = add ssa.5, 7\n"
        "    ssa.7 = add ssa.6, 9\n"
        "    ssa.8 = add ssa.7, 11\n"
        "    ssa.9 = add ssa.8, 13\n"
        "    br ssa.3, bb.5, bb.6\n"
        "  bb.5:\n"
        "    ssa.10 = phi [bb.1: 56], [bb.3: 67], [bb.4: ssa.9]\n"
        "    ret ssa.10\n"
        "  bb.6:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_eliminates_partial_unknown_predecessor_binary(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_memory_aware_cse_partial_unknown_predecessor_binary_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline partial-unknown setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline partial-unknown failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-PARTIAL-UNKNOWN-PRED-BINARY",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = add ssa.2, 3\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    ssa.4 = phi [bb.1: 11], [bb.3: 22], [bb.4: ssa.3]\n"
        "    ret ssa.4\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_eliminates_nested_join_memory_binaries(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_pipeline_memory_aware_nested_join_cse_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline nested memory-aware cse setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: pipeline nested memory-aware cse failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-MEMORY-AWARE-CSE-NESTED",
        &program,
        "global g.0\n"
        "\n"
        "func main(c.0) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    ssa.1 = phi [bb.1: 16], [bb.2: 27]\n"
        "    ret ssa.1\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_eliminates_materialized_partial_unknown_edge(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_memory_aware_cse_partial_unknown_materialized_binary_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline materialized partial-unknown setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline materialized partial-unknown failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-MATERIALIZED-PARTIAL-UNKNOWN",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = add ssa.2, 3\n"
        "    ssa.4 = add ssa.3, 5\n"
        "    jmp bb.5\n"
        "  bb.5:\n"
        "    ssa.5 = phi [bb.1: 16], [bb.3: 27], [bb.4: ssa.4]\n"
        "    ret ssa.5\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_eliminates_branch_materialized_partial_unknown_edge(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_memory_aware_cse_branch_materialized_partial_unknown_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline branch materialized partial-unknown setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline branch materialized partial-unknown failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-BRANCH-MATERIALIZED-PARTIAL-UNKNOWN",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1, e.2) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = load_local e.2\n"
        "    ssa.4 = add ssa.2, 3\n"
        "    br ssa.3, bb.5, bb.6\n"
        "  bb.5:\n"
        "    ssa.5 = phi [bb.1: 11], [bb.3: 22], [bb.4: ssa.4]\n"
        "    ret ssa.5\n"
        "  bb.6:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_eliminates_branch_materialized_nested_partial_unknown_edge(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_memory_aware_cse_branch_materialized_nested_partial_unknown_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline branch materialized nested partial-unknown setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline branch materialized nested partial-unknown failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-BRANCH-MATERIALIZED-NESTED-PARTIAL-UNKNOWN",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1, e.2) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = load_local e.2\n"
        "    ssa.4 = add ssa.2, 3\n"
        "    ssa.5 = add ssa.4, 5\n"
        "    br ssa.3, bb.5, bb.6\n"
        "  bb.5:\n"
        "    ssa.6 = phi [bb.1: 16], [bb.3: 27], [bb.4: ssa.5]\n"
        "    ret ssa.6\n"
        "  bb.6:\n"
        "    ret 0\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_pass_pipeline_eliminates_branch_materialized_deep_partial_unknown_edge(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_memory_aware_cse_branch_materialized_deep_partial_unknown_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline branch materialized deep partial-unknown setup failed: %s\n",
            error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-pass] FAIL: pipeline branch materialized deep partial-unknown failed: %s\n",
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-BRANCH-MATERIALIZED-DEEP-PARTIAL-UNKNOWN",
        &program,
        "global g.0\n"
        "\n"
        "declare touch()\n"
        "\n"
        "func main(c.0, d.1, e.2) {\n"
        "  bb.0:\n"
        "    ssa.0 = load_local c.0\n"
        "    br ssa.0, bb.1, bb.2\n"
        "  bb.1:\n"
        "    jmp bb.5\n"
        "  bb.2:\n"
        "    ssa.1 = load_local d.1\n"
        "    br ssa.1, bb.3, bb.4\n"
        "  bb.3:\n"
        "    jmp bb.5\n"
        "  bb.4:\n"
        "    call touch()\n"
        "    ssa.2 = load_global g.0\n"
        "    ssa.3 = load_local e.2\n"
        "    ssa.4 = add ssa.2, 3\n"
        "    ssa.5 = add ssa.4, 5\n"
        "    ssa.6 = add ssa.5, 7\n"
        "    ssa.7 = add ssa.6, 9\n"
        "    ssa.8 = add ssa.7, 11\n"
        "    ssa.9 = add ssa.8, 13\n"
        "    br ssa.3, bb.5, bb.6\n"
        "  bb.5:\n"
        "    ssa.10 = phi [bb.1: 56], [bb.3: 67], [bb.4: ssa.9]\n"
        "    ret ssa.10\n"
        "  bb.6:\n"
        "    ret 0\n"
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

/*
 * Nested diamond with same value: outer diamond branches to two inner diamonds,
 * all four leaf branches store 42 to a.0, final join loads.
 *
 *   bb.0 (entry): br -> bb.1, bb.4
 *   bb.1: br -> bb.2, bb.3     (inner diamond left)
 *   bb.2: store_local a.0, 42; jmp bb.7
 *   bb.3: store_local a.0, 42; jmp bb.7
 *   bb.4: br -> bb.5, bb.6     (inner diamond right)
 *   bb.5: store_local a.0, 42; jmp bb.7
 *   bb.6: store_local a.0, 42; jmp bb.7
 *   bb.7 (join): load_local a.0; ret
 *
 * Expected after pipeline: all stores/loads fold to ret 42.
 */
static int build_nested_diamond_same_value_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *blk = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t i;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (i = 0; i < 8; ++i) {
        if (!value_ssa_function_append_block(function, NULL, &blk, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    /* bb.0: entry - outer diamond branch */
    if (!value_ssa_block_set_branch(&function->blocks[0], value_ssa_value_immediate(1), 1, 4, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    /* bb.1: inner left diamond entry */
    if (!value_ssa_block_set_branch(&function->blocks[1], value_ssa_value_immediate(1), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    /* bb.2, bb.3: store 42, jump to join */
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(&function->blocks[2], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[2], 7, error) ||
        !value_ssa_block_append_instruction(&function->blocks[3], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[3], 7, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    /* bb.4: inner right diamond entry */
    if (!value_ssa_block_set_branch(&function->blocks[4], value_ssa_value_immediate(1), 5, 6, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    /* bb.5, bb.6: store 42, jump to join */
    if (!value_ssa_block_append_instruction(&function->blocks[5], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[5], 7, error) ||
        !value_ssa_block_append_instruction(&function->blocks[6], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[6], 7, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    /* bb.7: join - load and return */
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
    if (!value_ssa_block_append_instruction(&function->blocks[7], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[7], value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int test_memory_ssa_pass_pipeline_nested_diamond_same_value(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_nested_diamond_same_value_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: nested-diamond-same setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: nested-diamond-same pipeline failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-NESTED-DIAMOND",
        &program,
        "func main() {\n"
        "  bb.0:\n"
        "    ret 42\n"
        "}\n");

    value_ssa_program_free(&program);
    return ok;
}

/*
 * Loop with diamond inside, same value: entry stores 42, loop body has
 * diamond where both branches store 42, exit loads.
 *
 *   bb.0: store_local a.0, 42; jmp bb.1
 *   bb.1 (header): br 1, bb.2, bb.5
 *   bb.2 (diamond entry): br 1, bb.3, bb.4
 *   bb.3: store_local a.0, 42; jmp bb.4  (both go to bb.4 as simple join)
 *   bb.4: store_local a.0, 42; jmp bb.1  (second branch also stores 42)
 *   bb.5 (exit): load_local a.0; ret
 *
 * Simplification: use 7 blocks for cleaner diamond join.
 *   bb.0: store 42; jmp bb.1
 *   bb.1: br 1, bb.2, bb.6 (loop or exit)
 *   bb.2: br 1, bb.3, bb.4 (diamond)
 *   bb.3: store 42; jmp bb.5
 *   bb.4: store 42; jmp bb.5
 *   bb.5: jmp bb.1 (loop latch)
 *   bb.6: load a.0; ret
 *
 * Expected after pipeline: all stores fold, load forwards to 42, ret 42.
 */
static int build_loop_diamond_same_value_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *blk = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t i;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (i = 0; i < 7; ++i) {
        if (!value_ssa_function_append_block(function, NULL, &blk, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    /* bb.0: entry - store 42, jump to header */
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.as.store.value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(&function->blocks[0], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[0], 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    /* bb.1: loop header - branch to body or exit */
    if (!value_ssa_block_set_branch(&function->blocks[1], value_ssa_value_immediate(1), 2, 6, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    /* bb.2: diamond entry - branch to left/right */
    if (!value_ssa_block_set_branch(&function->blocks[2], value_ssa_value_immediate(1), 3, 4, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    /* bb.3: diamond left - store 42, jump to join */
    if (!value_ssa_block_append_instruction(&function->blocks[3], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[3], 5, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    /* bb.4: diamond right - store 42, jump to join */
    if (!value_ssa_block_append_instruction(&function->blocks[4], &instruction, error) ||
        !value_ssa_block_set_jump(&function->blocks[4], 5, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    /* bb.5: diamond join / loop latch - jump back to header */
    if (!value_ssa_block_set_jump(&function->blocks[5], 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    /* bb.6: exit - load and return */
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
    if (!value_ssa_block_append_instruction(&function->blocks[6], &instruction, error) ||
        !value_ssa_block_set_return(&function->blocks[6], value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int test_memory_ssa_pass_pipeline_loop_with_diamond_same_value(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_loop_diamond_same_value_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: loop-diamond-same setup failed: %s\n", error.message);
        return 0;
    }

    if (!memory_ssa_pass_run_pipeline(&program, &error)) {
        fprintf(stderr, "[memory-ssa-pass] FAIL: loop-diamond-same pipeline failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_dump("MEMORY-SSA-PASS-PIPELINE-LOOP-DIAMOND",
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

int main(void) {
    int ok = 1;

    ok &= test_memory_ssa_pass_forwards_join_preserved_local_load();
    ok &= test_memory_ssa_pass_forwards_same_value_phi_local_load();
    ok &= test_memory_ssa_pass_forwards_parameter_local_reuse_load();
    ok &= test_memory_ssa_pass_forwards_phi_join_parameter_local_load();
    ok &= test_memory_ssa_pass_forwards_phi_join_parameter_local_load_through_mov();
    ok &= test_memory_ssa_pass_reuses_dominating_equivalent_phi_parameter_local_load();
    ok &= test_memory_ssa_pass_partially_redundant_parameter_local_join_load();
    ok &= test_memory_ssa_pass_partially_redundant_scrambled_parameter_local_join_load();
    ok &= test_memory_ssa_pass_partially_redundant_parameter_local_join_prefers_ancestor_value();
    ok &= test_memory_ssa_pass_partially_redundant_multi_pred_parameter_local_join_load();
    ok &= test_memory_ssa_pass_keeps_loop_header_partial_parameter_local_load();
    ok &= test_memory_ssa_pass_promotes_differing_value_loop_local_slot();
    ok &= test_memory_ssa_pass_promotes_join_local_slot_to_value_phi();
    ok &= test_memory_ssa_pass_promotes_local_slot_using_ancestor_input_value();
    ok &= test_memory_ssa_pass_forwards_same_value_loop_local_load();
    ok &= test_memory_ssa_pass_forwards_loop_header_parameter_local_load();
    ok &= test_memory_ssa_pass_forwards_loop_header_global_load_through_readonly_call();
    ok &= test_memory_ssa_pass_promotes_same_value_loop_global_slot();
    ok &= test_memory_ssa_pass_keeps_differing_value_loop_local_load();
    ok &= test_memory_ssa_pass_keeps_phi_load_unforwarded();
    ok &= test_memory_ssa_pass_eliminates_branch_dead_local_store();
    ok &= test_memory_ssa_pass_forwards_join_preserved_global_load();
    ok &= test_memory_ssa_pass_forwards_same_value_phi_global_load();
    ok &= test_memory_ssa_pass_forwards_same_value_loop_global_load();
    ok &= test_memory_ssa_pass_forwards_repeated_global_load_after_call();
    ok &= test_memory_ssa_pass_forwards_phi_join_global_load();
    ok &= test_memory_ssa_pass_promotes_same_value_phi_global_slot();
    ok &= test_memory_ssa_pass_promotes_global_slot_using_ancestor_input_value();
    ok &= test_memory_ssa_pass_reuses_dominating_equivalent_phi_global_load();
    ok &= test_memory_ssa_pass_partially_redundant_global_join_load();
    ok &= test_memory_ssa_pass_partially_redundant_scrambled_global_join_load();
    ok &= test_memory_ssa_pass_partially_redundant_global_join_prefers_ancestor_value();
    ok &= test_memory_ssa_pass_partially_redundant_multi_pred_global_join_load();
    ok &= test_memory_ssa_pass_forwards_global_load_after_internal_pure_call();
    ok &= test_memory_ssa_pass_forwards_global_load_after_other_global_call();
    ok &= test_memory_ssa_pass_keeps_loop_global_load_after_call();
    ok &= test_memory_ssa_pass_keeps_loop_global_load_after_call_for_global_promotion();
    ok &= test_memory_ssa_pass_keeps_global_load_after_call();
    ok &= test_memory_ssa_pass_eliminates_redundant_local_join_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_parameter_local_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_parameter_local_store_locally();
    ok &= test_memory_ssa_pass_eliminates_redundant_parameter_local_phi_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_parameter_local_join_load_phi_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_parameter_local_loop_phi_store();
    ok &= test_memory_ssa_pass_keeps_nonredundant_parameter_local_loop_phi_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_global_join_store();
    ok &= test_memory_ssa_pass_eliminates_redundant_global_store_after_load();
    ok &= test_memory_ssa_pass_eliminates_redundant_global_store_after_load_globally();
    ok &= test_memory_ssa_pass_keeps_redundant_global_store_after_call();
    ok &= test_memory_ssa_pass_eliminates_dead_global_store();
    ok &= test_memory_ssa_pass_eliminates_dead_global_store_globally();
    ok &= test_memory_ssa_pass_keeps_global_store_before_call_live();
    ok &= test_memory_ssa_pass_eliminates_overwritten_stores();
    ok &= test_memory_ssa_pass_eliminates_phi_and_loop_overwritten_stores();
    ok &= test_memory_ssa_pass_canonicalize_memory_values_local_join();
    ok &= test_memory_ssa_pass_scalar_replace_local_slots_join();
    ok &= test_memory_ssa_pass_scalar_replace_global_slots_join();
    ok &= test_memory_ssa_pass_scalar_replace_slots_mixed_join();
    ok &= test_memory_ssa_pass_scalar_replace_slots_mixed_multi_pred_join();
    ok &= test_memory_ssa_pass_scalar_replace_slots_mixed_loop_exit();
    ok &= test_memory_ssa_pass_scalar_replace_slots_keeps_global_call_barrier();
    ok &= test_memory_ssa_pass_scalar_replace_slots_mixed_multi_pred_global_barrier();
    ok &= test_memory_ssa_pass_scalar_replace_slots_mixed_loop_global_barrier();
    ok &= test_memory_ssa_pass_canonicalize_memory_values_global_chain();
    ok &= test_memory_ssa_pass_canonicalize_memory_values_rejects_invalid_input();
    ok &= test_memory_ssa_pass_build_canonicalized_from_lower_ir_local_join();
    ok &= test_memory_ssa_pass_build_memory_canonicalized_from_lower_ir_fold_program();
    ok &= test_memory_ssa_pass_build_canonicalized_from_lower_ir_fold_program();
    ok &= test_memory_ssa_pass_pipeline_combo();
    ok &= test_memory_ssa_pass_pipeline_fold_exposed_constant();
    ok &= test_memory_ssa_pass_pipeline_eliminates_redundant_global_store_after_load();
    ok &= test_memory_ssa_pass_pipeline_eliminates_redundant_parameter_local_phi_store();
    ok &= test_memory_ssa_pass_pipeline_eliminates_redundant_parameter_local_join_load_phi_store();
    ok &= test_memory_ssa_pass_pipeline_eliminates_redundant_parameter_local_loop_phi_store();
    ok &= test_memory_ssa_pass_pipeline_forwards_internal_pure_call_global();
    ok &= test_memory_ssa_pass_pipeline_reuses_repeated_global_load_after_call();
    ok &= test_memory_ssa_pass_eliminates_redundant_memory_binaries();
    ok &= test_memory_ssa_pass_eliminates_redundant_nested_join_memory_binaries();
    ok &= test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_global_barrier();
    ok &= test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_partial_unknown_predecessor_binary();
    ok &= test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_materialized_partial_unknown_edge();
    ok &= test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_branch_materialized_partial_unknown_edge();
    ok &= test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_branch_materialized_nested_partial_unknown_edge();
    ok &= test_memory_ssa_pass_eliminates_redundant_memory_binaries_with_branch_materialized_deep_partial_unknown_edge();
    ok &= test_memory_ssa_pass_pipeline_eliminates_memory_aware_redundant_binary();
    ok &= test_memory_ssa_pass_pipeline_eliminates_nested_join_memory_binaries();
    ok &= test_memory_ssa_pass_pipeline_eliminates_partial_unknown_predecessor_binary();
    ok &= test_memory_ssa_pass_pipeline_eliminates_materialized_partial_unknown_edge();
    ok &= test_memory_ssa_pass_pipeline_eliminates_branch_materialized_partial_unknown_edge();
    ok &= test_memory_ssa_pass_pipeline_eliminates_branch_materialized_nested_partial_unknown_edge();
    ok &= test_memory_ssa_pass_pipeline_eliminates_branch_materialized_deep_partial_unknown_edge();
    ok &= test_memory_ssa_pass_pipeline_cfg_revisit_local();
    ok &= test_memory_ssa_pass_pipeline_cfg_revisit_global();
    ok &= test_memory_ssa_pass_pipeline_same_value_loop();
    ok &= test_memory_ssa_pass_pipeline_keeps_loop_global_call_barrier();
    ok &= test_memory_ssa_pass_pipeline_is_fixed_point();
    ok &= test_memory_ssa_pass_pipeline_rejects_invalid_input();
    ok &= test_memory_ssa_pass_keeps_live_join_store();
    ok &= test_memory_ssa_pass_pipeline_nested_diamond_same_value();
    ok &= test_memory_ssa_pass_pipeline_loop_with_diamond_same_value();
    if (!ok) {
        return 1;
    }

    printf("[memory-ssa-pass] PASS\n");
    return 0;
}
