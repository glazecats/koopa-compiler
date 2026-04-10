#include "value_ssa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int build_valid_program(ValueSsaProgram *program, ValueSsaError *error) {
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
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_valid_phi_program(ValueSsaProgram *program, ValueSsaError *error) {
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
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(else_value);
    instruction.as.mov_value = value_ssa_value_immediate(20);
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

static int expect_verifier_rejects(const char *case_id,
    ValueSsaProgram *program,
    const char *expected_fragment) {
    ValueSsaError error;

    if (value_ssa_verify_program(program, &error)) {
        fprintf(stderr,
            "[value-ssa-verify] FAIL: %s unexpectedly passed verifier\n",
            case_id);
        return 0;
    }

    if (!strstr(error.message, expected_fragment)) {
        fprintf(stderr,
            "[value-ssa-verify] FAIL: %s mismatch\nexpected fragment: %s\nactual: %s\n",
            case_id,
            expected_fragment,
            error.message);
        return 0;
    }

    return 1;
}

static int test_value_ssa_verifier_accepts_valid_program(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_valid_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-verify] FAIL: valid setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    ok = value_ssa_verify_program(&program, &error);
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-verify] FAIL: valid program rejected at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
    }

    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_verifier_accepts_valid_phi_program(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_valid_phi_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-verify] FAIL: valid phi setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    ok = value_ssa_verify_program(&program, &error);
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-verify] FAIL: valid phi program rejected at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
    }

    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_verifier_rejects_phi_input_count_mismatch(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    int ok;

    if (!build_valid_phi_program(&program, &error)) {
        return 0;
    }

    program.functions[0].blocks[3].phis[0].input_count = 1;
    ok = expect_verifier_rejects("VALUE-SSA-PHI-COUNT", &program, "VALUE-SSA-066");
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_verifier_rejects_duplicate_value_definition(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaInstruction instruction;
    ValueSsaFunction *function;
    ValueSsaBasicBlock *block;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    function = &program.functions[0];
    block = &function->blocks[0];
    block->has_terminator = 0;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(1);
    instruction.as.mov_value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(block, &instruction, &error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(1), &error)) {
        fprintf(stderr,
            "[value-ssa-verify] FAIL: duplicate-value setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("VALUE-SSA-DUP-DEF", &program, "VALUE-SSA-062");
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_verifier_rejects_unavailable_use(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    int ok;

    value_ssa_program_init(&program);
    if (!value_ssa_program_append_function(&program, "main", 1, &function, &error) ||
        !value_ssa_function_append_block(function, NULL, &block, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    function->next_value_id = 1;
    if (!value_ssa_block_set_return(block, value_ssa_value_id(0), &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("VALUE-SSA-UNAVAILABLE-USE", &program, "VALUE-SSA-064");
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_verifier_rejects_unreachable_block(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *dead = NULL;
    int ok;

    value_ssa_program_init(&program);
    if (!value_ssa_program_append_function(&program, "main", 1, &function, &error) ||
        !value_ssa_function_append_block(function, NULL, &entry, &error) ||
        !value_ssa_function_append_block(function, NULL, &dead, &error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_immediate(0), &error) ||
        !value_ssa_block_set_return(dead, value_ssa_value_immediate(1), &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("VALUE-SSA-UNREACHABLE", &program, "VALUE-SSA-059");
    value_ssa_program_free(&program);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_value_ssa_verifier_accepts_valid_program();
    ok &= test_value_ssa_verifier_accepts_valid_phi_program();
    ok &= test_value_ssa_verifier_rejects_phi_input_count_mismatch();
    ok &= test_value_ssa_verifier_rejects_duplicate_value_definition();
    ok &= test_value_ssa_verifier_rejects_unavailable_use();
    ok &= test_value_ssa_verifier_rejects_unreachable_block();

    if (!ok) {
        return 1;
    }

    printf("[value-ssa-verify] PASS\n");
    return 0;
}
