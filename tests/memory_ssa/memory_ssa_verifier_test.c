#include "memory_ssa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void free_temp_instruction(MemorySsaInstruction *instruction) {
    if (!instruction) {
        return;
    }

    if (instruction->instruction.kind == VALUE_SSA_INSTR_CALL) {
        free(instruction->instruction.as.call.callee_name);
        free(instruction->instruction.as.call.args);
    }
    free(instruction->accesses);
    memset(instruction, 0, sizeof(*instruction));
}

static int build_valid_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *block = NULL;
    MemorySsaInstruction instruction;
    MemorySsaAccess access;
    size_t slot_id;
    size_t version0;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(
            function, value_ssa_slot_local(0), "a.0", &slot_id, error) ||
        !memory_ssa_function_append_block(function, NULL, &block, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    version0 = memory_ssa_function_allocate_version(function);
    if (version0 == (size_t)-1) {
        memory_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.instruction.as.store.value = value_ssa_value_immediate(7);
    memset(&access, 0, sizeof(access));
    access.slot_id = slot_id;
    access.has_def_version = 1;
    access.def_version_id = version0;
    if (!memory_ssa_instruction_append_access(&instruction, &access, error) ||
        !memory_ssa_block_append_instruction(block, &instruction, error)) {
        free_temp_instruction(&instruction);
        memory_ssa_program_free(program);
        return 0;
    }
    free_temp_instruction(&instruction);

    if (!memory_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int expect_verifier_rejects(const char *case_id,
    MemorySsaProgram *program,
    const char *expected_fragment) {
    MemorySsaError error;

    if (memory_ssa_verify_program(program, &error)) {
        fprintf(stderr,
            "[memory-ssa-verify] FAIL: %s unexpectedly passed verifier\n",
            case_id);
        return 0;
    }

    if (!strstr(error.message, expected_fragment)) {
        fprintf(stderr,
            "[memory-ssa-verify] FAIL: %s mismatch\nexpected fragment: %s\nactual: %s\n",
            case_id,
            expected_fragment,
            error.message);
        return 0;
    }

    return 1;
}

static int test_memory_ssa_verifier_accepts_valid_program(void) {
    MemorySsaProgram program;
    MemorySsaError error;
    int ok;

    if (!build_valid_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-verify] FAIL: valid setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 0;
    }

    ok = memory_ssa_verify_program(&program, &error);
    if (!ok) {
        fprintf(stderr,
            "[memory-ssa-verify] FAIL: valid program rejected at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
    }

    memory_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_verifier_rejects_duplicate_version_definition(void) {
    MemorySsaProgram program;
    MemorySsaError error;
    MemorySsaFunction *function;
    MemorySsaBasicBlock *block;
    MemorySsaInstruction instruction;
    MemorySsaAccess access;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    function = &program.functions[0];
    block = &function->blocks[0];
    block->has_terminator = 0;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.instruction.as.store.slot = value_ssa_slot_local(0);
    instruction.instruction.as.store.value = value_ssa_value_immediate(8);
    memset(&access, 0, sizeof(access));
    access.slot_id = 0;
    access.has_def_version = 1;
    access.def_version_id = 0;
    if (!memory_ssa_instruction_append_access(&instruction, &access, &error) ||
        !memory_ssa_block_append_instruction(block, &instruction, &error) ||
        !memory_ssa_block_set_return(block, value_ssa_value_immediate(0), &error)) {
        free_temp_instruction(&instruction);
        memory_ssa_program_free(&program);
        return 0;
    }
    free_temp_instruction(&instruction);

    ok = expect_verifier_rejects("MEMORY-SSA-DUP-VERSION", &program, "MEMORY-SSA-056");
    memory_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_verifier_rejects_load_without_access(void) {
    MemorySsaProgram program;
    MemorySsaError error;
    MemorySsaFunction *function;
    MemorySsaBasicBlock *block;
    MemorySsaInstruction instruction;
    int ok;

    if (!build_valid_program(&program, &error)) {
        return 0;
    }

    function = &program.functions[0];
    block = &function->blocks[0];
    memory_ssa_program_free(&program);

    memory_ssa_program_init(&program);
    if (!memory_ssa_program_append_function(&program, "main", 1, &function, &error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_local(0), "a.0", NULL, &error) ||
        !memory_ssa_function_append_block(function, NULL, &block, &error)) {
        memory_ssa_program_free(&program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.instruction.has_result = 1;
    instruction.instruction.result = value_ssa_value_id(0);
    instruction.instruction.as.load_slot = value_ssa_slot_local(0);
    if (!memory_ssa_block_append_instruction(block, &instruction, &error) ||
        !memory_ssa_block_set_return(block, value_ssa_value_id(0), &error)) {
        memory_ssa_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("MEMORY-SSA-LOAD-NO-ACCESS", &program, "MEMORY-SSA-051");
    memory_ssa_program_free(&program);
    return ok;
}

static int test_memory_ssa_verifier_rejects_phi_input_count_mismatch(void) {
    MemorySsaProgram program;
    MemorySsaError error;
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *entry = NULL;
    MemorySsaBasicBlock *then_block = NULL;
    MemorySsaBasicBlock *join_block = NULL;
    MemorySsaPhiInput phi_inputs[1];
    int ok;

    memory_ssa_program_init(&program);
    if (!memory_ssa_program_append_function(&program, "main", 1, &function, &error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_global(0), "g.0", NULL, &error) ||
        !memory_ssa_function_append_block(function, NULL, &entry, &error) ||
        !memory_ssa_function_append_block(function, NULL, &then_block, &error) ||
        !memory_ssa_function_append_block(function, NULL, &join_block, &error)) {
        memory_ssa_program_free(&program);
        return 0;
    }
    if (memory_ssa_function_allocate_version(function) == (size_t)-1) {
        memory_ssa_program_free(&program);
        return 0;
    }

    if (!memory_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, &error) ||
        !memory_ssa_block_set_jump(then_block, 2, &error)) {
        memory_ssa_program_free(&program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].version_id = 0;
    if (!memory_ssa_block_append_phi(join_block, 0, 0, phi_inputs, 1, &error) ||
        !memory_ssa_block_set_return(join_block, value_ssa_value_immediate(0), &error)) {
        memory_ssa_program_free(&program);
        return 0;
    }

    ok = expect_verifier_rejects("MEMORY-SSA-PHI-COUNT", &program, "MEMORY-SSA-047");
    memory_ssa_program_free(&program);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_memory_ssa_verifier_accepts_valid_program();
    ok &= test_memory_ssa_verifier_rejects_duplicate_version_definition();
    ok &= test_memory_ssa_verifier_rejects_load_without_access();
    ok &= test_memory_ssa_verifier_rejects_phi_input_count_mismatch();
    if (!ok) {
        return 1;
    }

    printf("[memory-ssa-verify] PASS\n");
    return 0;
}
