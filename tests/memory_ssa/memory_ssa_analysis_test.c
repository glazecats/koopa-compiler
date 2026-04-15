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

static char *dup_text(const char *text) {
    char *copy;
    size_t length;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static int append_store(MemorySsaBasicBlock *block,
    ValueSsaSlotRef slot,
    ValueSsaValueRef value,
    size_t slot_id,
    size_t def_version_id,
    MemorySsaError *error) {
    MemorySsaInstruction instruction;
    MemorySsaAccess access;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.kind =
        slot.kind == VALUE_SSA_SLOT_LOCAL ? VALUE_SSA_INSTR_STORE_LOCAL : VALUE_SSA_INSTR_STORE_GLOBAL;
    instruction.instruction.as.store.slot = slot;
    instruction.instruction.as.store.value = value;
    memset(&access, 0, sizeof(access));
    access.slot_id = slot_id;
    access.has_def_version = 1;
    access.def_version_id = def_version_id;
    if (!memory_ssa_instruction_append_access(&instruction, &access, error) ||
        !memory_ssa_block_append_instruction(block, &instruction, error)) {
        free_temp_instruction(&instruction);
        return 0;
    }

    free_temp_instruction(&instruction);
    return 1;
}

static int append_load(MemorySsaBasicBlock *block,
    ValueSsaSlotRef slot,
    size_t result_id,
    size_t slot_id,
    size_t use_version_id,
    MemorySsaError *error) {
    MemorySsaInstruction instruction;
    MemorySsaAccess access;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.kind =
        slot.kind == VALUE_SSA_SLOT_LOCAL ? VALUE_SSA_INSTR_LOAD_LOCAL : VALUE_SSA_INSTR_LOAD_GLOBAL;
    instruction.instruction.has_result = 1;
    instruction.instruction.result = value_ssa_value_id(result_id);
    instruction.instruction.as.load_slot = slot;
    memset(&access, 0, sizeof(access));
    access.slot_id = slot_id;
    access.has_use_version = 1;
    access.use_version_id = use_version_id;
    if (!memory_ssa_instruction_append_access(&instruction, &access, error) ||
        !memory_ssa_block_append_instruction(block, &instruction, error)) {
        free_temp_instruction(&instruction);
        return 0;
    }

    free_temp_instruction(&instruction);
    return 1;
}

static int append_call_with_access(MemorySsaBasicBlock *block,
    const char *callee_name,
    size_t slot_id,
    size_t use_version_id,
    size_t def_version_id,
    MemorySsaError *error) {
    MemorySsaInstruction instruction;
    MemorySsaAccess access;

    memset(&instruction, 0, sizeof(instruction));
    instruction.instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.instruction.as.call.callee_name = dup_text(callee_name);
    if (!instruction.instruction.as.call.callee_name) {
        return 0;
    }
    memset(&access, 0, sizeof(access));
    access.slot_id = slot_id;
    access.has_use_version = 1;
    access.use_version_id = use_version_id;
    access.has_def_version = 1;
    access.def_version_id = def_version_id;
    if (!memory_ssa_instruction_append_access(&instruction, &access, error) ||
        !memory_ssa_block_append_instruction(block, &instruction, error)) {
        free_temp_instruction(&instruction);
        return 0;
    }

    free_temp_instruction(&instruction);
    return 1;
}

static int build_straight_local_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *block = NULL;
    size_t slot_id;
    size_t version0;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_local(0), "a.0", &slot_id, error) ||
        !memory_ssa_function_append_block(function, NULL, &block, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    version0 = memory_ssa_function_allocate_version(function);
    if (version0 == (size_t)-1 ||
        !append_store(block, value_ssa_slot_local(0), value_ssa_value_immediate(7), slot_id, version0, error) ||
        !append_load(block, value_ssa_slot_local(0), 0, slot_id, version0, error) ||
        !memory_ssa_block_set_return(block, value_ssa_value_id(0), error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_store_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *block = NULL;
    size_t slot_id;
    size_t version0;
    size_t version1;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_local(0), "a.0", &slot_id, error) ||
        !memory_ssa_function_append_block(function, NULL, &block, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    version0 = memory_ssa_function_allocate_version(function);
    version1 = memory_ssa_function_allocate_version(function);
    if (version0 == (size_t)-1 || version1 == (size_t)-1 ||
        !append_store(block, value_ssa_slot_local(0), value_ssa_value_immediate(1), slot_id, version0, error) ||
        !append_store(block, value_ssa_slot_local(0), value_ssa_value_immediate(2), slot_id, version1, error) ||
        !memory_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_overwritten_local_store_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *block = NULL;
    size_t slot_id;
    size_t version0;
    size_t version1;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_local(0), "a.0", &slot_id, error) ||
        !memory_ssa_function_append_block(function, NULL, &block, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    version0 = memory_ssa_function_allocate_version(function);
    version1 = memory_ssa_function_allocate_version(function);
    if (version0 == (size_t)-1 || version1 == (size_t)-1 ||
        !append_store(block, value_ssa_slot_local(0), value_ssa_value_immediate(1), slot_id, version0, error) ||
        !append_store(block, value_ssa_slot_local(0), value_ssa_value_immediate(2), slot_id, version1, error) ||
        !memory_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_overwritten_global_store_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *block = NULL;
    size_t slot_id;
    size_t version0;
    size_t version1;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_global(0), "g.0", &slot_id, error) ||
        !memory_ssa_function_append_block(function, NULL, &block, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    version0 = memory_ssa_function_allocate_version(function);
    version1 = memory_ssa_function_allocate_version(function);
    if (version0 == (size_t)-1 || version1 == (size_t)-1 ||
        !append_store(block, value_ssa_slot_global(0), value_ssa_value_immediate(1), slot_id, version0, error) ||
        !append_store(block, value_ssa_slot_global(0), value_ssa_value_immediate(2), slot_id, version1, error) ||
        !memory_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_phi_overwritten_local_store_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *entry = NULL;
    MemorySsaBasicBlock *then_block = NULL;
    MemorySsaBasicBlock *else_block = NULL;
    MemorySsaBasicBlock *join_block = NULL;
    MemorySsaPhiInput phi_inputs[2];
    size_t slot_id;
    size_t version0;
    size_t version1;
    size_t version2;
    size_t version3;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_local(0), "a.0", &slot_id, error) ||
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
    version3 = memory_ssa_function_allocate_version(function);
    if (version0 == (size_t)-1 || version1 == (size_t)-1 || version2 == (size_t)-1 || version3 == (size_t)-1 ||
        !memory_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !append_store(then_block, value_ssa_slot_local(0), value_ssa_value_immediate(1), slot_id, version0, error) ||
        !memory_ssa_block_set_jump(then_block, 3, error) ||
        !append_store(else_block, value_ssa_slot_local(0), value_ssa_value_immediate(2), slot_id, version1, error) ||
        !memory_ssa_block_set_jump(else_block, 3, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].version_id = version0;
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].version_id = version1;
    if (!memory_ssa_block_append_phi(join_block, slot_id, version2, phi_inputs, 2, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    {
        MemorySsaInstruction instruction;
        MemorySsaAccess access;

        memset(&instruction, 0, sizeof(instruction));
        instruction.instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
        instruction.instruction.as.store.slot = value_ssa_slot_local(0);
        instruction.instruction.as.store.value = value_ssa_value_immediate(9);
        memset(&access, 0, sizeof(access));
        access.slot_id = slot_id;
        access.has_use_version = 1;
        access.use_version_id = version2;
        access.has_def_version = 1;
        access.def_version_id = version3;
        if (!memory_ssa_instruction_append_access(&instruction, &access, error) ||
            !memory_ssa_block_append_instruction(join_block, &instruction, error) ||
            !memory_ssa_block_set_return(join_block, value_ssa_value_immediate(0), error)) {
            free_temp_instruction(&instruction);
            memory_ssa_program_free(program);
            return 0;
        }
        free_temp_instruction(&instruction);
    }

    return 1;
}

static int build_loop_overwritten_local_store_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *entry = NULL;
    MemorySsaBasicBlock *header = NULL;
    MemorySsaBasicBlock *body = NULL;
    MemorySsaBasicBlock *exit_block = NULL;
    MemorySsaPhiInput phi_inputs[2];
    size_t slot_id;
    size_t version0;
    size_t version1;
    size_t version2;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_local(0), "a.0", &slot_id, error) ||
        !memory_ssa_function_append_block(function, NULL, &entry, error) ||
        !memory_ssa_function_append_block(function, NULL, &header, error) ||
        !memory_ssa_function_append_block(function, NULL, &body, error) ||
        !memory_ssa_function_append_block(function, NULL, &exit_block, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    version0 = memory_ssa_function_allocate_version(function);
    version1 = memory_ssa_function_allocate_version(function);
    version2 = memory_ssa_function_allocate_version(function);
    if (version0 == (size_t)-1 || version1 == (size_t)-1 || version2 == (size_t)-1 ||
        !append_store(entry, value_ssa_slot_local(0), value_ssa_value_immediate(1), slot_id, version0, error) ||
        !memory_ssa_block_set_jump(entry, 1, error) ||
        !memory_ssa_block_set_branch(header, value_ssa_value_immediate(1), 2, 3, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].version_id = version0;
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].version_id = version2;
    if (!memory_ssa_block_append_phi(header, slot_id, version1, phi_inputs, 2, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    {
        MemorySsaInstruction instruction;
        MemorySsaAccess access;

        memset(&instruction, 0, sizeof(instruction));
        instruction.instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
        instruction.instruction.as.store.slot = value_ssa_slot_local(0);
        instruction.instruction.as.store.value = value_ssa_value_immediate(2);
        memset(&access, 0, sizeof(access));
        access.slot_id = slot_id;
        access.has_use_version = 1;
        access.use_version_id = version1;
        access.has_def_version = 1;
        access.def_version_id = version2;
        if (!memory_ssa_instruction_append_access(&instruction, &access, error) ||
            !memory_ssa_block_append_instruction(body, &instruction, error) ||
            !memory_ssa_block_set_jump(body, 1, error) ||
            !memory_ssa_block_set_return(exit_block, value_ssa_value_immediate(0), error)) {
            free_temp_instruction(&instruction);
            memory_ssa_program_free(program);
            return 0;
        }
        free_temp_instruction(&instruction);
    }

    return 1;
}

static int build_phi_load_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *entry = NULL;
    MemorySsaBasicBlock *then_block = NULL;
    MemorySsaBasicBlock *else_block = NULL;
    MemorySsaBasicBlock *join_block = NULL;
    MemorySsaPhiInput phi_inputs[2];
    size_t slot_id;
    size_t version0;
    size_t version1;
    size_t version2;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_local(0), "a.0", &slot_id, error) ||
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
    if (version0 == (size_t)-1 || version1 == (size_t)-1 || version2 == (size_t)-1 ||
        !memory_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !append_store(then_block, value_ssa_slot_local(0), value_ssa_value_immediate(1), slot_id, version0, error) ||
        !memory_ssa_block_set_jump(then_block, 3, error) ||
        !append_store(else_block, value_ssa_slot_local(0), value_ssa_value_immediate(2), slot_id, version1, error) ||
        !memory_ssa_block_set_jump(else_block, 3, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].version_id = version0;
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].version_id = version1;
    if (!memory_ssa_block_append_phi(join_block, slot_id, version2, phi_inputs, 2, error) ||
        !append_load(join_block, value_ssa_slot_local(0), 0, slot_id, version2, error) ||
        !memory_ssa_block_set_return(join_block, value_ssa_value_id(0), error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_same_value_phi_load_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *entry = NULL;
    MemorySsaBasicBlock *then_block = NULL;
    MemorySsaBasicBlock *else_block = NULL;
    MemorySsaBasicBlock *join_block = NULL;
    MemorySsaPhiInput phi_inputs[2];
    size_t slot_id;
    size_t version0;
    size_t version1;
    size_t version2;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_local(0), "a.0", &slot_id, error) ||
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
    if (version0 == (size_t)-1 || version1 == (size_t)-1 || version2 == (size_t)-1 ||
        !memory_ssa_block_set_branch(entry, value_ssa_value_immediate(1), 1, 2, error) ||
        !append_store(then_block, value_ssa_slot_local(0), value_ssa_value_immediate(7), slot_id, version0, error) ||
        !memory_ssa_block_set_jump(then_block, 3, error) ||
        !append_store(else_block, value_ssa_slot_local(0), value_ssa_value_immediate(7), slot_id, version1, error) ||
        !memory_ssa_block_set_jump(else_block, 3, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 1;
    phi_inputs[0].version_id = version0;
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].version_id = version1;
    if (!memory_ssa_block_append_phi(join_block, slot_id, version2, phi_inputs, 2, error) ||
        !append_load(join_block, value_ssa_slot_local(0), 0, slot_id, version2, error) ||
        !memory_ssa_block_set_return(join_block, value_ssa_value_id(0), error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_global_call_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *declare_function = NULL;
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *block = NULL;
    size_t slot_id;
    size_t entry_version;
    size_t version1;
    size_t version2;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "touch", 0, &declare_function, error) ||
        !memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_global(0), "g.0", &slot_id, error) ||
        !memory_ssa_function_append_block(function, NULL, &block, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    entry_version = memory_ssa_function_allocate_version(function);
    version1 = memory_ssa_function_allocate_version(function);
    version2 = memory_ssa_function_allocate_version(function);
    if (entry_version == (size_t)-1 || version1 == (size_t)-1 || version2 == (size_t)-1 ||
        !memory_ssa_function_set_entry_version(function, slot_id, entry_version, error) ||
        !append_store(block, value_ssa_slot_global(0), value_ssa_value_immediate(9), slot_id, version1, error) ||
        !append_call_with_access(block, "touch", slot_id, version1, version2, error) ||
        !append_load(block, value_ssa_slot_global(0), 0, slot_id, version2, error) ||
        !memory_ssa_block_set_return(block, value_ssa_value_id(0), error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_dead_global_store_program(MemorySsaProgram *program, MemorySsaError *error) {
    MemorySsaFunction *function = NULL;
    MemorySsaBasicBlock *block = NULL;
    size_t slot_id;
    size_t version0;

    memory_ssa_program_init(program);
    if (!memory_ssa_program_append_function(program, "main", 1, &function, error) ||
        !memory_ssa_function_append_tracked_slot(function, value_ssa_slot_global(0), "g.0", &slot_id, error) ||
        !memory_ssa_function_append_block(function, NULL, &block, error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    version0 = memory_ssa_function_allocate_version(function);
    if (version0 == (size_t)-1 ||
        !append_store(block, value_ssa_slot_global(0), value_ssa_value_immediate(9), slot_id, version0, error) ||
        !memory_ssa_block_set_return(block, value_ssa_value_immediate(0), error)) {
        memory_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int expect_int(const char *case_id, const char *label, long long actual, long long expected) {
    if (actual != expected) {
        fprintf(stderr,
            "[memory-ssa-analysis] FAIL: %s %s expected %lld got %lld\n",
            case_id,
            label,
            expected,
            actual);
        return 0;
    }
    return 1;
}

static int test_straight_local_analysis(void) {
    MemorySsaProgram program;
    MemorySsaDefUseAnalysis analysis;
    MemorySsaError error;
    const MemorySsaVersionUseSite *sites = NULL;
    size_t site_count = 0;
    ValueSsaValueRef value;
    int has_value = 0;
    int is_dead = 0;
    int ok = 1;

    if (!build_straight_local_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: straight-local setup failed: %s\n", error.message);
        return 0;
    }

    memory_ssa_def_use_analysis_init(&analysis);
    if (!memory_ssa_compute_def_use_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: straight-local analysis failed: %s\n", error.message);
        memory_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_int("STRAIGHT-LOCAL", "version_count", (long long)analysis.version_count, 1);
    ok &= expect_int("STRAIGHT-LOCAL", "def_slot", (long long)analysis.def_slot_ids[0], 0);
    ok &= expect_int("STRAIGHT-LOCAL", "def_kind", (long long)analysis.def_kinds[0], MEMORY_SSA_VERSION_DEF_INSTR);
    ok &= expect_int("STRAIGHT-LOCAL", "use_count", (long long)analysis.use_counts[0], 1);
    if (!memory_ssa_find_version_use_sites(&analysis, 0, &sites, &site_count)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: STRAIGHT-LOCAL use-site lookup failed\n");
        ok = 0;
    } else {
        ok &= expect_int("STRAIGHT-LOCAL", "site_count", (long long)site_count, 1);
        if (site_count == 1) {
            ok &= expect_int("STRAIGHT-LOCAL", "site_kind", (long long)sites[0].kind, MEMORY_SSA_VERSION_USE_INSTR);
            ok &= expect_int("STRAIGHT-LOCAL", "site_role", (long long)sites[0].role, MEMORY_SSA_VERSION_USE_ROLE_LOAD);
            ok &= expect_int("STRAIGHT-LOCAL", "site_instr", (long long)sites[0].instruction_index, 1);
        }
    }
    if (!memory_ssa_resolve_load_store_value(&program.functions[0], &analysis, 0, 1, &has_value, &value, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: STRAIGHT-LOCAL resolve failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("STRAIGHT-LOCAL", "has_value", has_value, 1);
        ok &= expect_int("STRAIGHT-LOCAL", "resolved_immediate", value.immediate, 7);
    }
    if (!memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 0, 0, &is_dead, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: STRAIGHT-LOCAL dead-store query failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("STRAIGHT-LOCAL", "is_dead", is_dead, 0);
    }

    memory_ssa_def_use_analysis_free(&analysis);
    memory_ssa_program_free(&program);
    return ok;
}

static int test_dead_store_analysis(void) {
    MemorySsaProgram program;
    MemorySsaDefUseAnalysis analysis;
    MemorySsaError error;
    int is_dead0 = 0;
    int is_dead1 = 0;
    int ok = 1;

    if (!build_dead_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: dead-store setup failed: %s\n", error.message);
        return 0;
    }

    memory_ssa_def_use_analysis_init(&analysis);
    if (!memory_ssa_compute_def_use_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: dead-store analysis failed: %s\n", error.message);
        memory_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_int("DEAD-STORE", "use_count_mem0", (long long)analysis.use_counts[0], 0);
    ok &= expect_int("DEAD-STORE", "use_count_mem1", (long long)analysis.use_counts[1], 0);
    if (!memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 0, 0, &is_dead0, &error) ||
        !memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 0, 1, &is_dead1, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: DEAD-STORE query failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("DEAD-STORE", "instr0_dead", is_dead0, 1);
        ok &= expect_int("DEAD-STORE", "instr1_dead", is_dead1, 1);
    }

    memory_ssa_def_use_analysis_free(&analysis);
    memory_ssa_program_free(&program);
    return ok;
}

static int test_overwritten_store_analysis(void) {
    MemorySsaProgram program;
    MemorySsaDefUseAnalysis analysis;
    MemorySsaError error;
    int is_dead0 = 0;
    int is_dead1 = 0;
    int ok = 1;

    if (!build_overwritten_local_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: overwritten local-store setup failed: %s\n", error.message);
        return 0;
    }

    memory_ssa_def_use_analysis_init(&analysis);
    if (!memory_ssa_compute_def_use_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: overwritten local-store analysis failed: %s\n", error.message);
        memory_ssa_program_free(&program);
        return 0;
    }

    if (!memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 0, 0, &is_dead0, &error) ||
        !memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 0, 1, &is_dead1, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: overwritten local-store query failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("OVERWRITE-LOCAL-STORE", "instr0_dead", is_dead0, 1);
        ok &= expect_int("OVERWRITE-LOCAL-STORE", "instr1_dead", is_dead1, 1);
    }

    memory_ssa_def_use_analysis_free(&analysis);
    memory_ssa_program_free(&program);
    if (!ok) {
        return 0;
    }

    if (!build_overwritten_global_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: overwritten global-store setup failed: %s\n", error.message);
        return 0;
    }

    memory_ssa_def_use_analysis_init(&analysis);
    if (!memory_ssa_compute_def_use_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: overwritten global-store analysis failed: %s\n", error.message);
        memory_ssa_program_free(&program);
        return 0;
    }

    if (!memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 0, 0, &is_dead0, &error) ||
        !memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 0, 1, &is_dead1, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: overwritten global-store query failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("OVERWRITE-GLOBAL-STORE", "instr0_dead", is_dead0, 1);
        ok &= expect_int("OVERWRITE-GLOBAL-STORE", "instr1_dead", is_dead1, 1);
    }

    memory_ssa_def_use_analysis_free(&analysis);
    memory_ssa_program_free(&program);
    return ok;
}

static int test_phi_and_loop_overwritten_store_analysis(void) {
    MemorySsaProgram program;
    MemorySsaDefUseAnalysis analysis;
    MemorySsaError error;
    int is_dead = 0;
    int ok = 1;

    if (!build_phi_overwritten_local_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: phi-overwritten local setup failed: %s\n", error.message);
        return 0;
    }

    memory_ssa_def_use_analysis_init(&analysis);
    if (!memory_ssa_compute_def_use_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: phi-overwritten local analysis failed: %s\n", error.message);
        memory_ssa_program_free(&program);
        return 0;
    }

    if (!memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 1, 0, &is_dead, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: phi-overwritten local query failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("PHI-OVERWRITE-LOCAL", "then_store_dead", is_dead, 1);
    }
    if (!memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 2, 0, &is_dead, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: phi-overwritten local else query failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("PHI-OVERWRITE-LOCAL", "else_store_dead", is_dead, 1);
    }

    memory_ssa_def_use_analysis_free(&analysis);
    memory_ssa_program_free(&program);
    if (!ok) {
        return 0;
    }

    if (!build_loop_overwritten_local_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: loop-overwritten local setup failed: %s\n", error.message);
        return 0;
    }

    memory_ssa_def_use_analysis_init(&analysis);
    if (!memory_ssa_compute_def_use_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: loop-overwritten local analysis failed: %s\n", error.message);
        memory_ssa_program_free(&program);
        return 0;
    }

    if (!memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 0, 0, &is_dead, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: loop-overwritten entry query failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("LOOP-OVERWRITE-LOCAL", "entry_store_dead", is_dead, 1);
    }
    if (!memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 2, 0, &is_dead, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: loop-overwritten body query failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("LOOP-OVERWRITE-LOCAL", "body_store_dead", is_dead, 1);
    }

    memory_ssa_def_use_analysis_free(&analysis);
    memory_ssa_program_free(&program);
    return ok;
}

static int test_phi_load_analysis(void) {
    MemorySsaProgram program;
    MemorySsaDefUseAnalysis analysis;
    MemorySsaError error;
    const MemorySsaVersionUseSite *sites = NULL;
    size_t site_count = 0;
    int has_value = 0;
    ValueSsaValueRef value;
    int ok = 1;

    if (!build_phi_load_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: phi-load setup failed: %s\n", error.message);
        return 0;
    }

    memory_ssa_def_use_analysis_init(&analysis);
    if (!memory_ssa_compute_def_use_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: phi-load analysis failed: %s\n", error.message);
        memory_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_int("PHI-LOAD", "use_count_mem0", (long long)analysis.use_counts[0], 1);
    ok &= expect_int("PHI-LOAD", "use_count_mem1", (long long)analysis.use_counts[1], 1);
    ok &= expect_int("PHI-LOAD", "use_count_mem2", (long long)analysis.use_counts[2], 1);
    if (!memory_ssa_find_version_use_sites(&analysis, 2, &sites, &site_count)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: PHI-LOAD use-site lookup failed\n");
        ok = 0;
    } else if (site_count == 1) {
        ok &= expect_int("PHI-LOAD", "phi_result_use_role", (long long)sites[0].role, MEMORY_SSA_VERSION_USE_ROLE_LOAD);
    } else {
        ok &= expect_int("PHI-LOAD", "phi_result_site_count", (long long)site_count, 1);
    }
    if (!memory_ssa_resolve_load_store_value(&program.functions[0], &analysis, 3, 0, &has_value, &value, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: PHI-LOAD resolve failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("PHI-LOAD", "has_direct_store_value", has_value, 0);
    }

    memory_ssa_def_use_analysis_free(&analysis);
    memory_ssa_program_free(&program);
    return ok;
}

static int test_same_value_phi_load_analysis(void) {
    MemorySsaProgram program;
    MemorySsaDefUseAnalysis analysis;
    MemorySsaError error;
    int has_value = 0;
    ValueSsaValueRef value;
    int ok = 1;

    if (!build_same_value_phi_load_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: same-value phi-load setup failed: %s\n", error.message);
        return 0;
    }

    memory_ssa_def_use_analysis_init(&analysis);
    if (!memory_ssa_compute_def_use_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: same-value phi-load analysis failed: %s\n", error.message);
        memory_ssa_program_free(&program);
        return 0;
    }

    if (!memory_ssa_resolve_load_store_value(&program.functions[0], &analysis, 3, 0, &has_value, &value, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: same-value phi-load resolve failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("PHI-LOAD-SAME", "has_direct_store_value", has_value, 1);
        ok &= expect_int("PHI-LOAD-SAME", "resolved_immediate", value.immediate, 7);
    }

    memory_ssa_def_use_analysis_free(&analysis);
    memory_ssa_program_free(&program);
    return ok;
}

static int test_global_call_analysis(void) {
    MemorySsaProgram program;
    MemorySsaDefUseAnalysis analysis;
    MemorySsaError error;
    const MemorySsaVersionUseSite *sites = NULL;
    size_t site_count = 0;
    int has_value = 0;
    int is_dead = 0;
    ValueSsaValueRef value;
    int ok = 1;

    if (!build_global_call_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: global-call setup failed: %s\n", error.message);
        return 0;
    }

    memory_ssa_def_use_analysis_init(&analysis);
    if (!memory_ssa_compute_def_use_analysis(&program.functions[1], &analysis, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: global-call analysis failed: %s\n", error.message);
        memory_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_int("GLOBAL-CALL", "use_count_entry", (long long)analysis.use_counts[0], 0);
    ok &= expect_int("GLOBAL-CALL", "use_count_store", (long long)analysis.use_counts[1], 1);
    ok &= expect_int("GLOBAL-CALL", "use_count_call_def", (long long)analysis.use_counts[2], 1);
    if (!memory_ssa_find_version_use_sites(&analysis, 1, &sites, &site_count)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: GLOBAL-CALL use-site lookup failed\n");
        ok = 0;
    } else if (site_count == 1) {
        ok &= expect_int("GLOBAL-CALL", "store_use_role", (long long)sites[0].role, MEMORY_SSA_VERSION_USE_ROLE_CALL);
        ok &= expect_int("GLOBAL-CALL", "store_use_instr", (long long)sites[0].instruction_index, 1);
    } else {
        ok &= expect_int("GLOBAL-CALL", "store_use_site_count", (long long)site_count, 1);
    }
    if (!memory_ssa_resolve_load_store_value(&program.functions[1], &analysis, 0, 2, &has_value, &value, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: GLOBAL-CALL resolve failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("GLOBAL-CALL", "has_direct_store_value", has_value, 0);
    }
    if (!memory_ssa_is_trivially_dead_store(&program.functions[1], &analysis, 0, 0, &is_dead, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: GLOBAL-CALL dead-store query failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("GLOBAL-CALL", "global_store_dead", is_dead, 0);
    }

    memory_ssa_def_use_analysis_free(&analysis);
    memory_ssa_program_free(&program);
    return ok;
}

static int test_dead_global_store_analysis(void) {
    MemorySsaProgram program;
    MemorySsaDefUseAnalysis analysis;
    MemorySsaError error;
    int is_dead = 0;
    int ok = 1;

    if (!build_dead_global_store_program(&program, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: dead-global-store setup failed: %s\n", error.message);
        return 0;
    }

    memory_ssa_def_use_analysis_init(&analysis);
    if (!memory_ssa_compute_def_use_analysis(&program.functions[0], &analysis, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: dead-global-store analysis failed: %s\n", error.message);
        memory_ssa_program_free(&program);
        return 0;
    }

    if (!memory_ssa_is_trivially_dead_store(&program.functions[0], &analysis, 0, 0, &is_dead, &error)) {
        fprintf(stderr, "[memory-ssa-analysis] FAIL: dead-global-store query failed: %s\n", error.message);
        ok = 0;
    } else {
        ok &= expect_int("DEAD-GLOBAL-STORE", "is_dead", is_dead, 1);
    }

    memory_ssa_def_use_analysis_free(&analysis);
    memory_ssa_program_free(&program);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_straight_local_analysis();
    ok &= test_dead_store_analysis();
    ok &= test_overwritten_store_analysis();
    ok &= test_phi_and_loop_overwritten_store_analysis();
    ok &= test_phi_load_analysis();
    ok &= test_same_value_phi_load_analysis();
    ok &= test_global_call_analysis();
    ok &= test_dead_global_store_analysis();
    if (!ok) {
        return 1;
    }

    printf("[memory-ssa-analysis] PASS\n");
    return 0;
}
