#include "value_ssa_alloc.h"
#include "value_ssa_machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_machine_register_bank_dump(const char *case_id,
    const ValueSsaMachineRegisterBank *bank,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_machine_register_bank(bank, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: %s machine register bank dump failed: %s\n", case_id, error.message);
        return 0;
    }
    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: %s machine register bank dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_function_machine_allocation_view_dump(const char *case_id,
    const ValueSsaFunctionMachineAllocationView *view,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_function_machine_allocation_view(view, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: %s function machine allocation dump failed: %s\n", case_id, error.message);
        return 0;
    }
    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: %s function machine allocation dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_program_machine_allocation_view_dump(const char *case_id,
    const ValueSsaProgramMachineAllocationView *view,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_program_machine_allocation_view(view, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: %s program machine allocation dump failed: %s\n", case_id, error.message);
        return 0;
    }
    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: %s program machine allocation dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int build_alloc_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t sum0;
    size_t sum1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    sum0 = value_ssa_function_allocate_value(function);
    sum1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 ||
        sum0 == (size_t)-1 || sum1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(30);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(sum1);
    instruction.as.binary.lhs = value_ssa_value_id(sum0);
    instruction.as.binary.rhs = value_ssa_value_id(value2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(sum1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_two_function_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaProgram local;

    value_ssa_program_init(&local);
    if (!build_alloc_spill_program(&local, error)) {
        value_ssa_program_free(&local);
        return 0;
    }

    *program = local;
    if (!value_ssa_program_append_function(program, "main", 1, NULL, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaFunction *function = &program->functions[1];
        ValueSsaBasicBlock *entry = NULL;
        ValueSsaInstruction instruction;
        size_t value_id;

        if (!value_ssa_function_append_block(function, NULL, &entry, error)) {
            value_ssa_program_free(program);
            return 0;
        }

        for (value_id = 0; value_id < 5; ++value_id) {
            size_t allocated = value_ssa_function_allocate_value(function);

            if (allocated == (size_t)-1) {
                value_ssa_program_free(program);
                return 0;
            }
            memset(&instruction, 0, sizeof(instruction));
            instruction.kind = VALUE_SSA_INSTR_MOV;
            instruction.has_result = 1;
            instruction.result = value_ssa_value_id(allocated);
            instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
            if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
                value_ssa_program_free(program);
                return 0;
            }
        }
        if (!value_ssa_block_set_return(entry, value_ssa_value_id(4), error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    {
        ValueSsaFunction tmp = program->functions[0];
        program->functions[0] = program->functions[1];
        program->functions[1] = tmp;
    }

    return 1;
}

static int prepare_manual_two_function_program_result(ValueSsaProgramAllocationResult *result,
    const ValueSsaProgram *program) {
    size_t function_index;
    size_t value_count;
    size_t value_index;

    if (!result || !program || program->function_count != 2) {
        return 0;
    }

    value_ssa_program_allocation_result_free(result);
    result->function_count = 2;
    result->function_results = (ValueSsaAllocationResult *)calloc(2, sizeof(ValueSsaAllocationResult));
    if (!result->function_results) {
        return 0;
    }

    value_ssa_allocation_result_init(&result->function_results[0]);
    value_ssa_allocation_result_init(&result->function_results[1]);

    for (function_index = 0; function_index < 2; ++function_index) {
        value_count = program->functions[function_index].next_value_id;
        result->function_results[function_index].value_count = value_count;
        result->function_results[function_index].color_budget = 2;
        result->function_results[function_index].spill_slot_count = 0;
        result->function_results[function_index].colors = (size_t *)malloc(value_count * sizeof(size_t));
        result->function_results[function_index].has_color = (unsigned char *)calloc(value_count, sizeof(unsigned char));
        result->function_results[function_index].spill_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
        result->function_results[function_index].spill_intended_flags =
            (unsigned char *)calloc(value_count, sizeof(unsigned char));
        result->function_results[function_index].spill_confirmed_flags =
            (unsigned char *)calloc(value_count, sizeof(unsigned char));
        result->function_results[function_index].spill_recovered_flags =
            (unsigned char *)calloc(value_count, sizeof(unsigned char));
        result->function_results[function_index].spill_recovery_orders =
            (size_t *)malloc(value_count * sizeof(size_t));
        result->function_results[function_index].spill_recovery_phase_ids =
            (size_t *)malloc(value_count * sizeof(size_t));
        result->function_results[function_index].spill_recovery_phase_member_orders =
            (size_t *)malloc(value_count * sizeof(size_t));
        result->function_results[function_index].spill_recovery_phase_member_counts =
            (size_t *)malloc(value_count * sizeof(size_t));
        result->function_results[function_index].spill_recovery_phase_entries =
            (ValueSsaAllocatorRetryFamilyAgendaItem *)calloc(value_count, sizeof(ValueSsaAllocatorRetryFamilyAgendaItem));
        result->function_results[function_index].spill_priorities = (size_t *)calloc(value_count, sizeof(size_t));
        result->function_results[function_index].spill_slots = (size_t *)malloc(value_count * sizeof(size_t));
        if (!result->function_results[function_index].colors ||
            !result->function_results[function_index].has_color ||
            !result->function_results[function_index].spill_flags ||
            !result->function_results[function_index].spill_intended_flags ||
            !result->function_results[function_index].spill_confirmed_flags ||
            !result->function_results[function_index].spill_recovered_flags ||
            !result->function_results[function_index].spill_recovery_orders ||
            !result->function_results[function_index].spill_recovery_phase_ids ||
            !result->function_results[function_index].spill_recovery_phase_member_orders ||
            !result->function_results[function_index].spill_recovery_phase_member_counts ||
            !result->function_results[function_index].spill_recovery_phase_entries ||
            !result->function_results[function_index].spill_priorities ||
            !result->function_results[function_index].spill_slots) {
            value_ssa_program_allocation_result_free(result);
            return 0;
        }

        for (value_index = 0; value_index < value_count; ++value_index) {
            result->function_results[function_index].colors[value_index] = (size_t)-1;
            result->function_results[function_index].spill_recovery_orders[value_index] = (size_t)-1;
            result->function_results[function_index].spill_recovery_phase_ids[value_index] = (size_t)-1;
            result->function_results[function_index].spill_recovery_phase_member_orders[value_index] = (size_t)-1;
            result->function_results[function_index].spill_recovery_phase_member_counts[value_index] = (size_t)-1;
            result->function_results[function_index].spill_slots[value_index] = (size_t)-1;
        }
    }

    for (value_index = 0; value_index < result->function_results[0].value_count; ++value_index) {
        result->function_results[0].has_color[value_index] = 1;
        result->function_results[0].colors[value_index] = 0;
    }

    for (value_index = 1; value_index < result->function_results[1].value_count; ++value_index) {
        result->function_results[1].has_color[value_index] = 1;
        result->function_results[1].colors[value_index] = value_index == 2 ? 1 : 0;
    }
    result->function_results[1].spill_flags[0] = 1;
    result->function_results[1].spill_confirmed_flags[0] = 1;
    result->function_results[1].spill_slots[0] = 0;
    result->function_results[1].spill_slot_count = 1;
    result->function_results[1].spill_intended_flags[2] = 1;
    result->function_results[1].spill_intended_flags[4] = 1;
    result->function_results[1].spill_recovered_flags[4] = 1;

    return 1;
}

static int test_value_ssa_build_flat_machine_register_bank(void) {
    ValueSsaMachineRegisterBank bank;
    int ok = 1;

    value_ssa_machine_register_bank_init(&bank);
    if (!value_ssa_build_flat_machine_register_bank(3, &bank, NULL)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: machine-register-bank build failed\n");
        return 0;
    }

    ok &= expect_machine_register_bank_dump("VALUE-SSA-MACHINE-BANK",
        &bank,
        "machine-register-bank registers=3\n"
        "r0 name=r0 class=general alloc=1 caller_clobbered=1 callee_preserved=0\n"
        "r1 name=r1 class=general alloc=1 caller_clobbered=1 callee_preserved=0\n"
        "r2 name=r2 class=general alloc=1 caller_clobbered=1 callee_preserved=0\n");

    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

static int test_value_ssa_build_function_machine_allocation_view(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaFunctionAllocationLayout layout;
    ValueSsaMachineRegisterBank bank;
    ValueSsaFunctionMachineAllocationView view;
    int ok = 1;

    if (!build_alloc_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: function-machine-view setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_function_allocation_layout_init(&layout);
    value_ssa_machine_register_bank_init(&bank);
    value_ssa_function_machine_allocation_view_init(&view);
    if (!value_ssa_allocate_function_layout(&program.functions[0], 2, &layout, &error) ||
        !value_ssa_build_flat_machine_register_bank(2, &bank, &error) ||
        !value_ssa_build_function_machine_allocation_view(&layout, &bank, &view, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: function-machine-view build failed: %s\n", error.message);
        value_ssa_function_machine_allocation_view_free(&view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_function_allocation_layout_free(&layout);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_function_machine_allocation_view_dump("VALUE-SSA-MACHINE-FUNCTION-VIEW",
        &view,
        "machine-allocation func spill regs=2 values=5 colored=4 spilled=1 spill_slots=1\n"
        "ssa.0 spill=0 spill-confirmed\n"
        "ssa.1 reg=r0\n"
        "ssa.2 reg=r1 spill-intended\n"
        "ssa.3 reg=r0\n"
        "ssa.4 reg=r0\n");

    value_ssa_function_machine_allocation_view_free(&view);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_function_allocation_layout_free(&layout);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_build_program_machine_allocation_view(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaMachineRegisterBank bank;
    ValueSsaProgramMachineAllocationView view;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, NULL)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-view setup failed\n");
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_machine_register_bank_init(&bank);
    value_ssa_program_machine_allocation_view_init(&view);
    if (!prepare_manual_two_function_program_result(&result, &program) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, NULL) ||
        !value_ssa_program_allocation_layout_attach_program_context(&layout, &program, NULL) ||
        !value_ssa_build_flat_machine_register_bank(2, &bank, NULL) ||
        !value_ssa_build_program_machine_allocation_view(&layout, &bank, &view, NULL)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-view build failed\n");
        value_ssa_program_machine_allocation_view_free(&view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_program_machine_allocation_view_dump("VALUE-SSA-MACHINE-PROGRAM-VIEW",
        &view,
        "machine-allocation program functions=2 values=10 colored=9 spilled=1\n"
        "machine-allocation func main regs=2 values=5 colored=5 spilled=0 spill_slots=0\n"
        "ssa.0 reg=r0\n"
        "ssa.1 reg=r0\n"
        "ssa.2 reg=r0\n"
        "ssa.3 reg=r0\n"
        "ssa.4 reg=r0\n"
        "\n"
        "machine-allocation func spill regs=2 values=5 colored=4 spilled=1 spill_slots=1\n"
        "ssa.0 spill=0 spill-confirmed\n"
        "ssa.1 reg=r0\n"
        "ssa.2 reg=r1 spill-intended\n"
        "ssa.3 reg=r0\n"
        "ssa.4 reg=r0 spill-intended spill-recovered\n");

    value_ssa_program_machine_allocation_view_free(&view);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_value_ssa_build_flat_machine_register_bank();
    ok &= test_value_ssa_build_function_machine_allocation_view();
    ok &= test_value_ssa_build_program_machine_allocation_view();
    if (!ok) {
        return 1;
    }

    printf("[value-ssa-machine] PASS\n");
    return 0;
}
