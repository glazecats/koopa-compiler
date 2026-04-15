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

static int build_straight_local_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *block = NULL;
    MemorySsaInstruction instruction;
    MemorySsaAccess access;
    size_t slot_id;
    size_t version0;
    size_t value0 = 0;

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

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.instruction.has_result = 1;
    instruction.instruction.result = value_ssa_value_id(value0);
    instruction.instruction.as.load_slot = value_ssa_slot_local(0);
    memset(&access, 0, sizeof(access));
    access.slot_id = slot_id;
    access.has_use_version = 1;
    access.use_version_id = version0;
    if (!memory_ssa_instruction_append_access(&instruction, &access, error) ||
        !memory_ssa_block_append_instruction(block, &instruction, error) ||
        !memory_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        free_temp_instruction(&instruction);
        memory_ssa_program_free(program);
        return 0;
    }
    free_temp_instruction(&instruction);
    return 1;
}

static int build_phi_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *entry = NULL;
    MemorySsaBasicBlock *then_block = NULL;
    MemorySsaBasicBlock *else_block = NULL;
    MemorySsaBasicBlock *join_block = NULL;
    MemorySsaInstruction instruction;
    MemorySsaAccess access;
    MemorySsaPhiInput phi_inputs[2];
    size_t slot_id;
    size_t version0;
    size_t version1;
    size_t version2;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(
            function, value_ssa_slot_global(0), "g.0", &slot_id, error) ||
        !memory_ssa_function_append_block(function, NULL, &entry, error) ||
        !memory_ssa_function_append_block(function, NULL, &then_block, error) ||
        !memory_ssa_function_append_block(function, NULL, &else_block, error) ||
        !memory_ssa_function_append_block(function, NULL, &join_block, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    version0 = memory_ssa_function_allocate_version(function);
    version1 = memory_ssa_function_allocate_version(function);
    version2 = memory_ssa_function_allocate_version(function);
    if (version0 == (size_t)-1 || version1 == (size_t)-1 || version2 == (size_t)-1) {
        memory_ssa_program_free(program);
        return 0;
    }

    if (!memory_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.instruction.as.store.value = value_ssa_value_immediate(1);
    memset(&access, 0, sizeof(access));
    access.slot_id = slot_id;
    access.has_def_version = 1;
    access.def_version_id = version0;
    if (!memory_ssa_instruction_append_access(&instruction, &access, error) ||
        !memory_ssa_block_append_instruction(then_block, &instruction, error) ||
        !memory_ssa_block_set_jump(then_block, 3, error)) {
        free_temp_instruction(&instruction);
        memory_ssa_program_free(program);
        return 0;
    }
    free_temp_instruction(&instruction);

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.instruction.as.store.slot = value_ssa_slot_global(0);
    instruction.instruction.as.store.value = value_ssa_value_immediate(2);
    memset(&access, 0, sizeof(access));
    access.slot_id = slot_id;
    access.has_def_version = 1;
    access.def_version_id = version1;
    if (!memory_ssa_instruction_append_access(&instruction, &access, error) ||
        !memory_ssa_block_append_instruction(else_block, &instruction, error) ||
        !memory_ssa_block_set_jump(else_block, 3, error)) {
        free_temp_instruction(&instruction);
        memory_ssa_program_free(program);
        return 0;
    }
    free_temp_instruction(&instruction);

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].version_id = version0;
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].version_id = version1;
    if (!memory_ssa_block_append_phi(join_block, slot_id, version2, phi_inputs, 2, error) ||
        !memory_ssa_block_set_return(join_block, value_ssa_value_immediate(0), error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_value_ssa_straight_local_program(ValueSsaProgram *program, ValueSsaError *error) {
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
    instruction.as.store.value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(block, &instruction, error)) {
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

static int build_value_ssa_diamond_local_program(ValueSsaProgram *program, ValueSsaError *error) {
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
        !value_ssa_block_set_jump(else_block, 3, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_value_ssa_loop_local_program(ValueSsaProgram *program, ValueSsaError *error) {
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
    instruction.as.store.value = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(header, value_ssa_value_immediate(1), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.as.store.value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(0);
    instruction.as.load_slot = value_ssa_slot_local(0);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_value_ssa_global_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", NULL, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

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
    instruction.result = value_ssa_value_id(0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_value_ssa_loop_global_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", NULL, error) ||
        !value_ssa_program_append_function(program, "touch", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

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
    instruction.as.call.callee_name = "touch";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_value_ssa_internal_other_global_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *helper = NULL;
    ValueSsaFunction *main_function = NULL;
    ValueSsaBasicBlock *helper_block = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_global(program, "g", NULL, error) ||
        !value_ssa_program_append_global(program, "h", NULL, error) ||
        !value_ssa_program_append_function(program, "helper", 1, &helper, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_function, error) ||
        !value_ssa_function_append_block(helper, NULL, &helper_block, error) ||
        !value_ssa_function_append_block(main_function, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(0);
    instruction.as.load_slot = value_ssa_slot_global(1);
    if (!value_ssa_block_append_instruction(helper_block, &instruction, error) ||
        !value_ssa_block_set_return(helper_block, value_ssa_value_id(0), error)) {
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
    instruction.as.call.callee_name = "helper";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!value_ssa_block_append_instruction(main_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(0);
    instruction.as.load_slot = value_ssa_slot_global(0);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_value_ssa_parameter_local_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 1, NULL, error) ||
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

static int expect_dump(const char *case_id, const MemorySsaProgram *program, const char *expected) {
    char *actual = NULL;
    int ok = 1;

    if (!memory_ssa_dump_program(program, &actual)) {
        fprintf(stderr, "[memory-ssa-reg] FAIL: %s dump failed\n", case_id);
        return 0;
    }

    if (strcmp(actual, expected) != 0) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: %s dump mismatch\nexpected:\n%sactual:\n%s",
            case_id,
            expected,
            actual);
        ok = 0;
    }

    free(actual);
    return ok;
}

int main(void) {
    MemorySsaProgram program;
    MemorySsaError error;
    ValueSsaProgram value_program;
    ValueSsaError value_error;
    int ok = 1;

    if (!build_straight_local_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: straight-local setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 1;
    }
    ok &= expect_dump("MEMORY-SSA-STRAIGHT-LOCAL",
        &program,
        "func main() {\n"
        "  slots:\n"
        "    slot.0 = local a.0\n"
        "  bb.0:\n"
        "    mem.0 = store_local a.0, 7\n"
        "    ssa.0 = load_local a.0 @ mem.0\n"
        "    ret ssa.0\n"
        "}\n");
    memory_ssa_program_free(&program);

    if (!build_phi_program(&program, &error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: phi setup failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        return 1;
    }
    ok &= expect_dump("MEMORY-SSA-PHI",
        &program,
        "func main() {\n"
        "  slots:\n"
        "    slot.0 = global g.0\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    mem.0 = store_global g.0, 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    mem.1 = store_global g.0, 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    mem.2 = phi slot.0 [bb.1: mem.0], [bb.2: mem.1]\n"
        "    ret 0\n"
        "}\n");
    memory_ssa_program_free(&program);

    if (!build_value_ssa_straight_local_program(&value_program, &value_error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: value-ssa straight setup failed at %d:%d: %s\n",
            value_error.line,
            value_error.column,
            value_error.message);
        return 1;
    }
    if (!memory_ssa_build_from_value_ssa(&value_program, &program, &error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: straight bridge failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&value_program);
        return 1;
    }
    ok &= expect_dump("MEMORY-SSA-BRIDGE-STRAIGHT",
        &program,
        "func main() {\n"
        "  slots:\n"
        "    slot.0 = local a.0\n"
        "  bb.0:\n"
        "    mem.0 = store_local a.0, 7\n"
        "    ssa.0 = load_local a.0 @ mem.0\n"
        "    ret ssa.0\n"
        "}\n");
    memory_ssa_program_free(&program);
    value_ssa_program_free(&value_program);

    if (!build_value_ssa_diamond_local_program(&value_program, &value_error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: value-ssa diamond setup failed at %d:%d: %s\n",
            value_error.line,
            value_error.column,
            value_error.message);
        return 1;
    }
    if (!memory_ssa_build_from_value_ssa(&value_program, &program, &error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: diamond bridge failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&value_program);
        return 1;
    }
    ok &= expect_dump("MEMORY-SSA-BRIDGE-DIAMOND",
        &program,
        "func main() {\n"
        "  slots:\n"
        "    slot.0 = local a.0\n"
        "  bb.0:\n"
        "    br 1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    mem.1 = store_local a.0, 1\n"
        "    jmp bb.3\n"
        "  bb.2:\n"
        "    mem.2 = store_local a.0, 2\n"
        "    jmp bb.3\n"
        "  bb.3:\n"
        "    mem.0 = phi slot.0 [bb.1: mem.1], [bb.2: mem.2]\n"
        "    ret 0\n"
        "}\n");
    memory_ssa_program_free(&program);
    value_ssa_program_free(&value_program);

    if (!build_value_ssa_loop_local_program(&value_program, &value_error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: value-ssa loop setup failed at %d:%d: %s\n",
            value_error.line,
            value_error.column,
            value_error.message);
        return 1;
    }
    if (!memory_ssa_build_from_value_ssa(&value_program, &program, &error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: loop bridge failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&value_program);
        return 1;
    }
    ok &= expect_dump("MEMORY-SSA-BRIDGE-LOOP",
        &program,
        "func main() {\n"
        "  slots:\n"
        "    slot.0 = local a.0\n"
        "  bb.0:\n"
        "    mem.1 = store_local a.0, 0\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    mem.0 = phi slot.0 [bb.0: mem.1], [bb.2: mem.2]\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    mem.2 = store_local a.0 @ mem.0, 1\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.0 = load_local a.0 @ mem.0\n"
        "    ret ssa.0\n"
        "}\n");
    memory_ssa_program_free(&program);
    value_ssa_program_free(&value_program);

    if (!build_value_ssa_global_call_program(&value_program, &value_error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: value-ssa global-call setup failed at %d:%d: %s\n",
            value_error.line,
            value_error.column,
            value_error.message);
        return 1;
    }
    if (!memory_ssa_build_from_value_ssa(&value_program, &program, &error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: global-call bridge failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&value_program);
        return 1;
    }
    ok &= expect_dump("MEMORY-SSA-BRIDGE-GLOBAL-CALL",
        &program,
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  slots:\n"
        "    slot.0 = global g.0 entry=mem.0\n"
        "  bb.0:\n"
        "    mem.1 = store_global g.0 @ mem.0, 9\n"
        "    call touch() [slot.0 use=mem.1 def=mem.2]\n"
        "    ssa.0 = load_global g.0 @ mem.2\n"
        "    ret ssa.0\n"
        "}\n");
    memory_ssa_program_free(&program);
    value_ssa_program_free(&value_program);

    if (!build_value_ssa_internal_other_global_call_program(&value_program, &value_error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: value-ssa internal-other-global-call setup failed at %d:%d: %s\n",
            value_error.line,
            value_error.column,
            value_error.message);
        return 1;
    }
    if (!memory_ssa_build_from_value_ssa(&value_program, &program, &error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: internal-other-global-call bridge failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&value_program);
        return 1;
    }
    ok &= expect_dump("MEMORY-SSA-BRIDGE-INTERNAL-OTHER-GLOBAL-CALL",
        &program,
        "func helper() {\n"
        "  slots:\n"
        "    slot.0 = global h.1 entry=mem.0\n"
        "  bb.0:\n"
        "    ssa.0 = load_global h.1 @ mem.0\n"
        "    ret ssa.0\n"
        "}\n"
        "\n"
        "func main() {\n"
        "  slots:\n"
        "    slot.0 = global g.0 entry=mem.0\n"
        "  bb.0:\n"
        "    mem.1 = store_global g.0 @ mem.0, 9\n"
        "    call helper()\n"
        "    ssa.0 = load_global g.0 @ mem.1\n"
        "    ret ssa.0\n"
        "}\n");
    memory_ssa_program_free(&program);
    value_ssa_program_free(&value_program);

    if (!build_value_ssa_loop_global_call_program(&value_program, &value_error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: value-ssa loop-global-call setup failed at %d:%d: %s\n",
            value_error.line,
            value_error.column,
            value_error.message);
        return 1;
    }
    if (!memory_ssa_build_from_value_ssa(&value_program, &program, &error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: loop-global-call bridge failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&value_program);
        return 1;
    }
    ok &= expect_dump("MEMORY-SSA-BRIDGE-LOOP-GLOBAL-CALL",
        &program,
        "declare touch()\n"
        "\n"
        "func main() {\n"
        "  slots:\n"
        "    slot.0 = global g.0 entry=mem.0\n"
        "  bb.0:\n"
        "    mem.2 = store_global g.0 @ mem.0, 9\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    mem.1 = phi slot.0 [bb.0: mem.2], [bb.2: mem.3]\n"
        "    br 1, bb.2, bb.3\n"
        "  bb.2:\n"
        "    call touch() [slot.0 use=mem.1 def=mem.3]\n"
        "    jmp bb.1\n"
        "  bb.3:\n"
        "    ssa.0 = load_global g.0 @ mem.1\n"
        "    ret ssa.0\n"
        "}\n");
    memory_ssa_program_free(&program);
    value_ssa_program_free(&value_program);

    if (!build_value_ssa_parameter_local_program(&value_program, &value_error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: value-ssa parameter-local setup failed at %d:%d: %s\n",
            value_error.line,
            value_error.column,
            value_error.message);
        return 1;
    }
    if (!memory_ssa_build_from_value_ssa(&value_program, &program, &error)) {
        fprintf(stderr,
            "[memory-ssa-reg] FAIL: parameter-local bridge failed at %d:%d: %s\n",
            error.line,
            error.column,
            error.message);
        value_ssa_program_free(&value_program);
        return 1;
    }
    ok &= expect_dump("MEMORY-SSA-BRIDGE-PARAM-LOCAL",
        &program,
        "func main() {\n"
        "  slots:\n"
        "    slot.0 = local a.0 entry=mem.0\n"
        "  bb.0:\n"
        "    ssa.0 = load_local a.0 @ mem.0\n"
        "    ret ssa.0\n"
        "}\n");
    memory_ssa_program_free(&program);
    value_ssa_program_free(&value_program);

    if (!ok) {
        return 1;
    }

    printf("[memory-ssa-reg] PASS\n");
    return 0;
}
