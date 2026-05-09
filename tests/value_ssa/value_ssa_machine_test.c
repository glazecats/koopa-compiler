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

static int build_caller_callee_machine_register_bank(ValueSsaMachineRegisterBank *bank) {
    if (!bank) {
        return 0;
    }

    value_ssa_machine_register_bank_free(bank);
    bank->register_count = 2;
    bank->registers = (ValueSsaMachineRegisterDesc *)calloc(2, sizeof(ValueSsaMachineRegisterDesc));
    if (!bank->registers) {
        value_ssa_machine_register_bank_free(bank);
        return 0;
    }

    bank->registers[0].register_id = 0;
    bank->registers[0].name = "r0";
    bank->registers[0].register_class = VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL;
    bank->registers[0].allocatable = 1u;
    bank->registers[0].caller_clobbered = 1u;
    bank->registers[0].callee_preserved = 0u;

    bank->registers[1].register_id = 1;
    bank->registers[1].name = "r1";
    bank->registers[1].register_class = VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL;
    bank->registers[1].allocatable = 1u;
    bank->registers[1].caller_clobbered = 0u;
    bank->registers[1].callee_preserved = 1u;
    return 1;
}

static int build_non_identity_machine_register_bank(ValueSsaMachineRegisterBank *bank) {
    if (!bank) {
        return 0;
    }

    value_ssa_machine_register_bank_free(bank);
    bank->register_count = 3;
    bank->registers = (ValueSsaMachineRegisterDesc *)calloc(3, sizeof(ValueSsaMachineRegisterDesc));
    if (!bank->registers) {
        value_ssa_machine_register_bank_free(bank);
        return 0;
    }

    bank->registers[0].register_id = 10;
    bank->registers[0].name = "a10";
    bank->registers[0].register_class = VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL;
    bank->registers[0].allocatable = 1u;
    bank->registers[0].caller_clobbered = 1u;
    bank->registers[0].callee_preserved = 0u;

    bank->registers[1].register_id = 20;
    bank->registers[1].name = "s20";
    bank->registers[1].register_class = VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL;
    bank->registers[1].allocatable = 1u;
    bank->registers[1].caller_clobbered = 0u;
    bank->registers[1].callee_preserved = 1u;

    bank->registers[2].register_id = 30;
    bank->registers[2].name = "p30";
    bank->registers[2].register_class = VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL;
    bank->registers[2].allocatable = 1u;
    bank->registers[2].caller_clobbered = 0u;
    bank->registers[2].callee_preserved = 1u;
    return 1;
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

static int expect_allocate_rewrite_machine_report_dump(const char *case_id,
    const ValueSsaAllocateRewriteMachineReport *report,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_allocate_rewrite_machine_report_artifact(report, &actual_text, &error)) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: %s allocate+rewrite machine report dump failed: %s\n",
            case_id,
            error.message);
        return 0;
    }
    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: %s allocate+rewrite machine report dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_machine_dump_failure(const char *case_id, int ok, const ValueSsaError *error, const char *expected_code) {
    if (ok) {
        fprintf(stderr, "[value-ssa-machine] FAIL: %s expected dump failure, but call succeeded\n", case_id);
        return 0;
    }
    if (!error || !error->message[0] || strstr(error->message, expected_code) == NULL) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: %s expected error code %s, got: %s\n",
            case_id,
            expected_code,
            error ? error->message : "<null>");
        return 0;
    }
    return 1;
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

static int build_alloc_call_density_prep_program(ValueSsaProgram *program,
    size_t *out_call_live_value,
    size_t *out_post_call_value,
    ValueSsaError *error) {
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *call_block = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaValueRef *call_args = NULL;
    size_t call_live_value;
    size_t post_call_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "sink", 1, &decl, error) ||
        !value_ssa_program_append_function(program, "call_density", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &call_block, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    call_live_value = value_ssa_function_allocate_value(function);
    post_call_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (call_live_value == (size_t)-1 || post_call_value == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }
    call_args = (ValueSsaValueRef *)malloc(sizeof(ValueSsaValueRef));
    if (!call_args) {
        value_ssa_program_free(program);
        return 0;
    }
    call_args[0] = value_ssa_value_id(call_live_value);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(call_live_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, call_block->id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "sink";
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!value_ssa_block_append_instruction(call_block, &instruction, error) ||
        !value_ssa_block_set_jump(call_block, exit_block->id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(post_call_value);
    instruction.as.mov_value = value_ssa_value_immediate(11);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(call_live_value);
    instruction.as.binary.rhs = value_ssa_value_id(post_call_value);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (out_call_live_value) {
        *out_call_live_value = call_live_value;
    }
    if (out_post_call_value) {
        *out_post_call_value = post_call_value;
    }
    return 1;
}

static int build_alloc_call_density_pressure_program(ValueSsaProgram *program,
    size_t *out_call_live_value,
    ValueSsaError *error) {
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *call_block = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaValueRef *call_args = NULL;
    size_t call_live_value;
    size_t helper0;
    size_t helper1;
    size_t helper2;
    size_t helper3;
    size_t post_call_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "sink", 1, &decl, error) ||
        !value_ssa_program_append_function(program, "call_pressure", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &call_block, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    call_live_value = value_ssa_function_allocate_value(function);
    helper0 = value_ssa_function_allocate_value(function);
    helper1 = value_ssa_function_allocate_value(function);
    helper2 = value_ssa_function_allocate_value(function);
    helper3 = value_ssa_function_allocate_value(function);
    post_call_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (call_live_value == (size_t)-1 || helper0 == (size_t)-1 || helper1 == (size_t)-1 ||
        helper2 == (size_t)-1 || helper3 == (size_t)-1 || post_call_value == (size_t)-1 ||
        sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    call_args = (ValueSsaValueRef *)malloc(sizeof(ValueSsaValueRef));
    if (!call_args) {
        value_ssa_program_free(program);
        return 0;
    }
    call_args[0] = value_ssa_value_id(call_live_value);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;

    instruction.result = value_ssa_value_id(call_live_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(helper0);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(helper1);
    instruction.as.mov_value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(helper2);
    instruction.as.mov_value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(helper3);
    instruction.as.mov_value = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, call_block->id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "sink";
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!value_ssa_block_append_instruction(call_block, &instruction, error) ||
        !value_ssa_block_set_jump(call_block, exit_block->id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(post_call_value);
    instruction.as.mov_value = value_ssa_value_immediate(11);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(call_live_value);
    instruction.as.binary.rhs = value_ssa_value_id(post_call_value);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (out_call_live_value) {
        *out_call_live_value = call_live_value;
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

static int prepare_manual_call_pressure_result(ValueSsaProgramAllocationResult *result,
    const ValueSsaProgram *program) {
    size_t value_count;
    size_t value_index;

    if (!result || !program || program->function_count < 2) {
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

    value_count = program->functions[1].next_value_id;
    result->function_results[1].value_count = value_count;
    result->function_results[1].color_budget = 3;
    result->function_results[1].spill_slot_count = 0;
    result->function_results[1].colors = (size_t *)malloc(value_count * sizeof(size_t));
    result->function_results[1].has_color = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    result->function_results[1].spill_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    result->function_results[1].spill_intended_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    result->function_results[1].spill_confirmed_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    result->function_results[1].spill_recovered_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    result->function_results[1].spill_recovery_orders = (size_t *)malloc(value_count * sizeof(size_t));
    result->function_results[1].spill_recovery_phase_ids = (size_t *)malloc(value_count * sizeof(size_t));
    result->function_results[1].spill_recovery_phase_member_orders = (size_t *)malloc(value_count * sizeof(size_t));
    result->function_results[1].spill_recovery_phase_member_counts = (size_t *)malloc(value_count * sizeof(size_t));
    result->function_results[1].spill_recovery_phase_entries =
        (ValueSsaAllocatorRetryFamilyAgendaItem *)calloc(value_count, sizeof(ValueSsaAllocatorRetryFamilyAgendaItem));
    result->function_results[1].spill_priorities = (size_t *)calloc(value_count, sizeof(size_t));
    result->function_results[1].spill_slots = (size_t *)malloc(value_count * sizeof(size_t));
    if (!result->function_results[1].colors ||
        !result->function_results[1].has_color ||
        !result->function_results[1].spill_flags ||
        !result->function_results[1].spill_intended_flags ||
        !result->function_results[1].spill_confirmed_flags ||
        !result->function_results[1].spill_recovered_flags ||
        !result->function_results[1].spill_recovery_orders ||
        !result->function_results[1].spill_recovery_phase_ids ||
        !result->function_results[1].spill_recovery_phase_member_orders ||
        !result->function_results[1].spill_recovery_phase_member_counts ||
        !result->function_results[1].spill_recovery_phase_entries ||
        !result->function_results[1].spill_priorities ||
        !result->function_results[1].spill_slots) {
        value_ssa_program_allocation_result_free(result);
        return 0;
    }

    for (value_index = 0; value_index < value_count; ++value_index) {
        result->function_results[1].colors[value_index] = (size_t)-1;
        result->function_results[1].spill_recovery_orders[value_index] = (size_t)-1;
        result->function_results[1].spill_recovery_phase_ids[value_index] = (size_t)-1;
        result->function_results[1].spill_recovery_phase_member_orders[value_index] = (size_t)-1;
        result->function_results[1].spill_recovery_phase_member_counts[value_index] = (size_t)-1;
        result->function_results[1].spill_slots[value_index] = (size_t)-1;
    }

    result->function_results[1].has_color[0] = 1; result->function_results[1].colors[0] = 0;
    result->function_results[1].has_color[1] = 1; result->function_results[1].colors[1] = 1;
    result->function_results[1].has_color[2] = 1; result->function_results[1].colors[2] = 1;
    result->function_results[1].has_color[3] = 1; result->function_results[1].colors[3] = 1;
    result->function_results[1].has_color[4] = 1; result->function_results[1].colors[4] = 2;
    result->function_results[1].has_color[5] = 1; result->function_results[1].colors[5] = 0;
    result->function_results[1].has_color[6] = 1; result->function_results[1].colors[6] = 0;
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
    ValueSsaMachineRegisterBank malformed_bank;
    ValueSsaMachineRegisterBank duplicate_id_bank;
    ValueSsaFunctionMachineAllocationView view;
    int ok = 1;

    if (!build_alloc_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: function-machine-view setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_function_allocation_layout_init(&layout);
    value_ssa_machine_register_bank_init(&bank);
    value_ssa_machine_register_bank_init(&malformed_bank);
    value_ssa_machine_register_bank_init(&duplicate_id_bank);
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
    malformed_bank.register_count = 1;
    if (value_ssa_build_function_machine_allocation_view(&layout, &malformed_bank, &view, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: malformed bank should not build function machine view\n");
        ok = 0;
    }
    if (!build_caller_callee_machine_register_bank(&duplicate_id_bank)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: duplicate-id bank setup failed\n");
        ok = 0;
    } else {
        duplicate_id_bank.registers[1].register_id = duplicate_id_bank.registers[0].register_id;
        if (value_ssa_build_function_machine_allocation_view(&layout, &duplicate_id_bank, &view, &error)) {
            fprintf(stderr, "[value-ssa-machine] FAIL: duplicate register-id bank should not build function machine view\n");
            ok = 0;
        }
    }

    ok &= expect_function_machine_allocation_view_dump("VALUE-SSA-MACHINE-FUNCTION-VIEW",
        &view,
        "machine-allocation func spill regs=2 values=5 colored=4 spilled=1 spill_slots=1 caller_clobbered=4 callee_preserved=0 used_regs=2 reg_groups=2\n"
        "used-registers: r0 r1\n"
        "caller-clobbered-values: ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 spill=0 spill-confirmed\n"
        "ssa.1 reg=r0\n"
        "ssa.2 reg=r1 spill-intended\n"
        "ssa.3 reg=r0\n"
        "ssa.4 reg=r0\n"
        "register-group r0: ssa.1 ssa.3 ssa.4\n"
        "register-group r1: ssa.2\n");

    value_ssa_function_machine_allocation_view_free(&view);
    value_ssa_machine_register_bank_free(&duplicate_id_bank);
    value_ssa_machine_register_bank_free(&malformed_bank);
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
    ValueSsaMachineRegisterBank malformed_bank;
    ValueSsaMachineRegisterBank duplicate_id_bank;
    ValueSsaProgramMachineAllocationView view;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, NULL)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-view setup failed\n");
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_machine_register_bank_init(&bank);
    value_ssa_machine_register_bank_init(&malformed_bank);
    value_ssa_machine_register_bank_init(&duplicate_id_bank);
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
    malformed_bank.register_count = 1;
    if (value_ssa_build_program_machine_allocation_view(&layout, &malformed_bank, &view, NULL)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: malformed bank should not build program machine view\n");
        ok = 0;
    }
    if (!build_caller_callee_machine_register_bank(&duplicate_id_bank)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: duplicate-id bank setup failed\n");
        ok = 0;
    } else {
        duplicate_id_bank.registers[1].register_id = duplicate_id_bank.registers[0].register_id;
        if (value_ssa_build_program_machine_allocation_view(&layout, &duplicate_id_bank, &view, NULL)) {
            fprintf(stderr, "[value-ssa-machine] FAIL: duplicate register-id bank should not build program machine view\n");
            ok = 0;
        }
    }

    ok &= expect_program_machine_allocation_view_dump("VALUE-SSA-MACHINE-PROGRAM-VIEW",
        &view,
        "machine-allocation program functions=2 values=10 colored=9 spilled=1 caller_clobbered=9 callee_preserved=0 colored_funcs=2 spilled_funcs=1 caller_clobbered_funcs=2 callee_preserved_funcs=0\n"
        "functions-with-machine-colors: 0 1\n"
        "functions-with-spills: 1\n"
        "functions-with-caller-clobbered-values: 0 1\n"
        "machine-allocation func main regs=2 values=5 colored=5 spilled=0 spill_slots=0 caller_clobbered=5 callee_preserved=0 used_regs=1 reg_groups=1\n"
        "used-registers: r0\n"
        "caller-clobbered-values: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 reg=r0\n"
        "ssa.1 reg=r0\n"
        "ssa.2 reg=r0\n"
        "ssa.3 reg=r0\n"
        "ssa.4 reg=r0\n"
        "register-group r0: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "\n"
        "machine-allocation func spill regs=2 values=5 colored=4 spilled=1 spill_slots=1 caller_clobbered=4 callee_preserved=0 used_regs=2 reg_groups=2\n"
        "used-registers: r0 r1\n"
        "caller-clobbered-values: ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 spill=0 spill-confirmed\n"
        "ssa.1 reg=r0\n"
        "ssa.2 reg=r1 spill-intended\n"
        "ssa.3 reg=r0\n"
        "ssa.4 reg=r0 spill-intended spill-recovered\n"
        "register-group r0: ssa.1 ssa.3 ssa.4\n"
        "register-group r1: ssa.2\n");

    value_ssa_program_machine_allocation_view_free(&view);
    value_ssa_machine_register_bank_free(&duplicate_id_bank);
    value_ssa_machine_register_bank_free(&malformed_bank);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_machine_register_bank_query_surface(void) {
    ValueSsaMachineRegisterBank bank;
    const ValueSsaMachineRegisterDesc *desc = NULL;
    size_t register_count = 0;
    size_t allocatable_count = 0;
    size_t caller_clobbered_count = 0;
    size_t callee_preserved_count = 0;
    size_t register_index = (size_t)-1;
    int ok = 1;

    value_ssa_machine_register_bank_init(&bank);
    if (!value_ssa_build_flat_machine_register_bank(3, &bank, NULL)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: bank-query setup failed\n");
        return 0;
    }

    if (!value_ssa_machine_register_bank_get_summary(
            &bank, &register_count, &allocatable_count, &caller_clobbered_count, &callee_preserved_count) ||
        register_count != 3 || allocatable_count != 3 || caller_clobbered_count != 3 || callee_preserved_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: bank-query summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_machine_register_bank_get_register(&bank, 1, &desc) || !desc || !desc->name ||
        strcmp(desc->name, "r1") != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: bank-query direct register lookup mismatch\n");
        ok = 0;
    }
    desc = NULL;
    if (!value_ssa_machine_register_bank_find_register_by_name(&bank, "r2", &register_index, &desc) ||
        register_index != 2 || !desc || desc->register_id != 2) {
        fprintf(stderr, "[value-ssa-machine] FAIL: bank-query name lookup mismatch\n");
        ok = 0;
    }
    desc = &bank.registers[0];
    if (value_ssa_machine_register_bank_get_register(&bank, 99, &desc) || desc != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: bank-query direct register failure should clear output\n");
        ok = 0;
    }
    register_index = 123;
    desc = &bank.registers[0];
    if (value_ssa_machine_register_bank_find_register_by_name(&bank, "missing", &register_index, &desc) ||
        register_index != 0 || desc != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: bank-query failed name lookup should clear outputs\n");
        ok = 0;
    }

    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

static int test_value_ssa_program_machine_allocation_view_query_surface(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaMachineRegisterBank bank;
    ValueSsaProgramMachineAllocationView view;
    const ValueSsaFunctionMachineAllocationView *function_view = NULL;
    const ValueSsaMachineAllocatedValuePlacement *placement = NULL;
    const ValueSsaMachineRegisterValueGroup *register_group = NULL;
    const char *function_name = NULL;
    size_t function_count = 0;
    size_t total_value_count = 0;
    size_t total_machine_colored_count = 0;
    size_t total_spilled_count = 0;
    size_t total_caller_clobbered_value_count = 0;
    size_t total_callee_preserved_value_count = 0;
    size_t function_index = (size_t)-1;
    size_t value_count = 0;
    size_t machine_register_count = 0;
    size_t spill_slot_count = 0;
    size_t machine_colored_count = 0;
    size_t spilled_count = 0;
    size_t caller_clobbered_value_count = 0;
    size_t callee_preserved_value_count = 0;
    size_t used_machine_register_count = 0;
    size_t machine_register_group_count = 0;
    size_t filtered_count = 0;
    const size_t *filtered_ids = NULL;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, NULL)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query setup failed\n");
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
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query build failed\n");
        value_ssa_program_machine_allocation_view_free(&view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_program_machine_allocation_view_get_summary(
            &view,
            &function_count,
            &total_value_count,
            &total_machine_colored_count,
            &total_spilled_count,
            &total_caller_clobbered_value_count,
            &total_callee_preserved_value_count) ||
        function_count != 2 || total_value_count != 10 || total_machine_colored_count != 9 || total_spilled_count != 1 ||
        total_caller_clobbered_value_count != 9 || total_callee_preserved_value_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_allocation_view_get_function_view_by_name(
            &view, "spill", &function_index, &function_view) ||
        function_index != 1 || !function_view) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query function lookup mismatch\n");
        ok = 0;
    } else if (!value_ssa_function_machine_allocation_view_get_summary(function_view,
                   &function_name,
                   &value_count,
                   &machine_register_count,
                   &spill_slot_count,
                   &machine_colored_count,
                   &spilled_count,
                   &caller_clobbered_value_count,
                   &callee_preserved_value_count,
                   &used_machine_register_count,
                   &machine_register_group_count) ||
        !function_name || strcmp(function_name, "spill") != 0 ||
        value_count != 5 || machine_register_count != 2 || spill_slot_count != 1 ||
        machine_colored_count != 4 || spilled_count != 1 ||
        caller_clobbered_value_count != 4 || callee_preserved_value_count != 0 ||
        used_machine_register_count != 2 || machine_register_group_count != 2) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query function summary mismatch\n");
        ok = 0;
    }
    if (!function_view ||
        !value_ssa_function_machine_allocation_view_get_placement(function_view, 2, &placement) ||
        !placement || placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR ||
        !placement->machine_register_name || strcmp(placement->machine_register_name, "r1") != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query placement lookup mismatch\n");
        ok = 0;
    }
    if (!function_view ||
        !value_ssa_function_machine_allocation_view_get_machine_colored_values(function_view, &filtered_count, &filtered_ids) ||
        filtered_count != 4 || !filtered_ids || filtered_ids[0] != 1 || filtered_ids[1] != 2) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query colored-values mismatch\n");
        ok = 0;
    }
    if (!function_view ||
        !value_ssa_function_machine_allocation_view_get_spilled_values(function_view, &filtered_count, &filtered_ids) ||
        filtered_count != 1 || !filtered_ids || filtered_ids[0] != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query spilled-values mismatch\n");
        ok = 0;
    }
    if (!function_view ||
        !value_ssa_function_machine_allocation_view_get_caller_clobbered_values(function_view, &filtered_count, &filtered_ids) ||
        filtered_count != 4 || !filtered_ids || filtered_ids[0] != 1) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query caller-clobbered mismatch\n");
        ok = 0;
    }
    if (!function_view ||
        !value_ssa_function_machine_allocation_view_get_used_machine_registers(function_view, &filtered_count, &filtered_ids) ||
        filtered_count != 2 || !filtered_ids || filtered_ids[0] != 0 || filtered_ids[1] != 1) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query used-registers mismatch\n");
        ok = 0;
    }
    if (!function_view ||
        !value_ssa_function_machine_allocation_view_get_machine_register_group(function_view, 0, &register_group) ||
        !register_group || register_group->value_count != 3 || !register_group->value_ids ||
        register_group->value_ids[0] != 1 || register_group->value_ids[1] != 3 || register_group->value_ids[2] != 4) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query register-group mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_allocation_view_get_functions_with_machine_colors(&view, &filtered_count, &filtered_ids) ||
        filtered_count != 2 || !filtered_ids || filtered_ids[0] != 0 || filtered_ids[1] != 1) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query functions-with-colors mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_allocation_view_get_functions_with_spills(&view, &filtered_count, &filtered_ids) ||
        filtered_count != 1 || !filtered_ids || filtered_ids[0] != 1) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query functions-with-spills mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_allocation_view_get_functions_with_caller_clobbered_values(
            &view, &filtered_count, &filtered_ids) ||
        filtered_count != 2 || !filtered_ids || filtered_ids[0] != 0 || filtered_ids[1] != 1) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query functions-with-caller-clobbered mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_allocation_view_get_functions_with_callee_preserved_values(
            &view, &filtered_count, &filtered_ids) ||
        filtered_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-machine-query functions-with-callee-preserved mismatch\n");
        ok = 0;
    }

    value_ssa_program_machine_allocation_view_free(&view);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_program_machine_allocation_view_preserves_public_register_ids(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaMachineRegisterBank bank;
    ValueSsaProgramMachineAllocationView view;
    const ValueSsaFunctionMachineAllocationView *function_view = NULL;
    const ValueSsaMachineAllocatedValuePlacement *placement = NULL;
    const ValueSsaMachineRegisterValueGroup *register_group = NULL;
    const size_t *used_machine_register_ids = NULL;
    size_t function_index = (size_t)-1;
    size_t used_count = 0;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, NULL)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-register-id setup failed\n");
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_machine_register_bank_init(&bank);
    value_ssa_program_machine_allocation_view_init(&view);
    if (!prepare_manual_two_function_program_result(&result, &program) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, NULL) ||
        !value_ssa_program_allocation_layout_attach_program_context(&layout, &program, NULL) ||
        !build_non_identity_machine_register_bank(&bank) ||
        !value_ssa_build_program_machine_allocation_view(&layout, &bank, &view, NULL)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-register-id build failed\n");
        value_ssa_program_machine_allocation_view_free(&view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_program_machine_allocation_view_get_function_view_by_name(
            &view, "spill", &function_index, &function_view) ||
        function_index != 1 || !function_view) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-register-id function lookup mismatch\n");
        ok = 0;
    }
    if (!function_view ||
        !value_ssa_function_machine_allocation_view_get_used_machine_registers(
            function_view, &used_count, &used_machine_register_ids) ||
        used_count != 2 || !used_machine_register_ids ||
        used_machine_register_ids[0] != 10 || used_machine_register_ids[1] != 20) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-register-id used-register ids mismatch\n");
        ok = 0;
    }
    if (!function_view ||
        !value_ssa_function_machine_allocation_view_get_placement(function_view, 2, &placement) ||
        !placement || placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR ||
        placement->machine_register_id != 20 ||
        !placement->machine_register_name || strcmp(placement->machine_register_name, "s20") != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-register-id placement mismatch\n");
        ok = 0;
    }
    if (!function_view ||
        !value_ssa_function_machine_allocation_view_get_machine_register_group(function_view, 10, &register_group) ||
        !register_group || register_group->machine_register_id != 10 ||
        !register_group->machine_register_name || strcmp(register_group->machine_register_name, "a10") != 0 ||
        register_group->value_count != 3 || !register_group->value_ids ||
        register_group->value_ids[0] != 1 || register_group->value_ids[1] != 3 || register_group->value_ids[2] != 4) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-register-id register-group mismatch\n");
        ok = 0;
    }

    value_ssa_program_machine_allocation_view_free(&view);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_rewrite_machine_report_artifact_dump(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteLayoutReport layout_report;
    ValueSsaAllocateRewriteMachineReport machine_report;
    ValueSsaAllocateRewriteMachineReport malformed_machine_report;
    ValueSsaMachineRegisterBank bank;
    ValueSsaError error;
    char *text = NULL;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: machine-report setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_layout_report_init(&layout_report);
    value_ssa_allocate_rewrite_machine_report_init(&machine_report);
    value_ssa_allocate_rewrite_machine_report_init(&malformed_machine_report);
    value_ssa_machine_register_bank_init(&bank);
    if (!prepare_manual_two_function_program_result(&result, &program) ||
        !value_ssa_build_allocate_rewrite_layout_report(&result, &(ValueSsaAllocateRewriteStats){2, 1}, &layout_report, &error) ||
        !value_ssa_allocate_rewrite_layout_report_attach_program_context(&layout_report, &program, &error) ||
        !value_ssa_build_flat_machine_register_bank(2, &bank, &error) ||
        !value_ssa_build_allocate_rewrite_machine_report(&layout_report, &bank, &machine_report, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: machine-report build failed: %s\n", error.message);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_allocate_rewrite_machine_report_free(&machine_report);
        value_ssa_allocate_rewrite_layout_report_free(&layout_report);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_allocate_rewrite_machine_report_dump("VALUE-SSA-MACHINE-REPORT",
        &machine_report,
        "allocate+rewrite machine-report allocation_rounds=2 rewrite_rounds=1\n"
        "machine-allocation program functions=2 values=10 colored=9 spilled=1 caller_clobbered=9 callee_preserved=0 colored_funcs=2 spilled_funcs=1 caller_clobbered_funcs=2 callee_preserved_funcs=0\n"
        "functions-with-machine-colors: 0 1\n"
        "functions-with-spills: 1\n"
        "functions-with-caller-clobbered-values: 0 1\n"
        "machine-allocation func main regs=2 values=5 colored=5 spilled=0 spill_slots=0 caller_clobbered=5 callee_preserved=0 used_regs=1 reg_groups=1\n"
        "used-registers: r0\n"
        "caller-clobbered-values: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 reg=r0\n"
        "ssa.1 reg=r0\n"
        "ssa.2 reg=r0\n"
        "ssa.3 reg=r0\n"
        "ssa.4 reg=r0\n"
        "register-group r0: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "\n"
        "machine-allocation func spill regs=2 values=5 colored=4 spilled=1 spill_slots=1 caller_clobbered=4 callee_preserved=0 used_regs=2 reg_groups=2\n"
        "used-registers: r0 r1\n"
        "caller-clobbered-values: ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 spill=0 spill-confirmed\n"
        "ssa.1 reg=r0\n"
        "ssa.2 reg=r1 spill-intended\n"
        "ssa.3 reg=r0\n"
        "ssa.4 reg=r0 spill-intended spill-recovered\n"
        "register-group r0: ssa.1 ssa.3 ssa.4\n"
        "register-group r1: ssa.2\n");

    malformed_machine_report.stats.allocation_rounds = 2;
    malformed_machine_report.stats.rewrite_rounds = 1;
    malformed_machine_report.view.function_count = 1;
    malformed_machine_report.view.function_views =
        (ValueSsaFunctionMachineAllocationView *)calloc(1, sizeof(ValueSsaFunctionMachineAllocationView));
    if (!malformed_machine_report.view.function_views) {
        fprintf(stderr, "[value-ssa-machine] FAIL: malformed machine-report setup failed\n");
        ok = 0;
    } else {
        value_ssa_function_machine_allocation_view_init(&malformed_machine_report.view.function_views[0]);
        malformed_machine_report.view.total_value_count = 1;
        malformed_machine_report.view.function_views[0].value_count = 1;
        malformed_machine_report.view.function_views[0].machine_register_count = 1;
        malformed_machine_report.view.function_views[0].machine_colored_count = 1;
        malformed_machine_report.view.function_views[0].used_machine_register_count = 1;
        malformed_machine_report.view.function_views[0].machine_register_group_count = 0;
        ok &= expect_machine_dump_failure(
            "VALUE-SSA-MACHINE-MALFORMED-REPORT-DUMP",
            value_ssa_dump_allocate_rewrite_machine_report_artifact(&malformed_machine_report, &text, &error),
            &error,
            "VALUE-SSA-MACHINE-025");
    }

    {
        const ValueSsaProgramMachineAllocationView *program_view = NULL;
        const ValueSsaFunctionMachineAllocationView *function_view = NULL;
        size_t allocation_rounds = 0;
        size_t rewrite_rounds = 0;
        size_t function_count = 0;
        size_t total_value_count = 0;
        size_t total_machine_colored_count = 0;
        size_t total_spilled_count = 0;
        size_t function_index = (size_t)-1;

        if (!value_ssa_allocate_rewrite_machine_report_get_summary(
                &machine_report,
                &allocation_rounds,
                &rewrite_rounds,
                &function_count,
                &total_value_count,
                &total_machine_colored_count,
                &total_spilled_count) ||
            allocation_rounds != 2 || rewrite_rounds != 1 ||
            function_count != 2 || total_value_count != 10 ||
            total_machine_colored_count != 9 || total_spilled_count != 1) {
            fprintf(stderr, "[value-ssa-machine] FAIL: machine-report summary mismatch\n");
            ok = 0;
        }
        if (!value_ssa_allocate_rewrite_machine_report_get_program_view(&machine_report, &program_view) ||
            !program_view) {
            fprintf(stderr, "[value-ssa-machine] FAIL: machine-report program-view query failed\n");
            ok = 0;
        }
        if (!value_ssa_allocate_rewrite_machine_report_get_function_view_by_name(
                &machine_report, "spill", &function_index, &function_view) ||
            function_index != 1 || !function_view) {
            fprintf(stderr, "[value-ssa-machine] FAIL: machine-report function lookup mismatch\n");
            ok = 0;
        }
    }

    value_ssa_machine_register_bank_free(&bank);
    free(text);
    value_ssa_allocate_rewrite_machine_report_free(&malformed_machine_report);
    value_ssa_allocate_rewrite_machine_report_free(&machine_report);
    value_ssa_allocate_rewrite_layout_report_free(&layout_report);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_machine_dump_rejects_malformed_function_view(void) {
    ValueSsaFunctionMachineAllocationView view;
    ValueSsaError error;
    char *text = NULL;
    int ok = 1;

    value_ssa_function_machine_allocation_view_init(&view);
    memset(&error, 0, sizeof(error));
    view.used_machine_register_count = 1;
    view.machine_register_group_count = 1;
    view.machine_register_groups =
        (ValueSsaMachineRegisterValueGroup *)calloc(1, sizeof(ValueSsaMachineRegisterValueGroup));
    if (!view.machine_register_groups) {
        fprintf(stderr, "[value-ssa-machine] FAIL: malformed function dump setup failed\n");
        value_ssa_function_machine_allocation_view_free(&view);
        return 0;
    }

    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-FUNCTION-DUMP",
        value_ssa_dump_function_machine_allocation_view(&view, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-065");

    free(text);
    value_ssa_function_machine_allocation_view_free(&view);
    return ok;
}

static int test_value_ssa_machine_dump_rejects_malformed_bank(void) {
    ValueSsaMachineRegisterBank malformed_bank;
    ValueSsaMachineRegisterBank duplicate_id_bank;
    ValueSsaMachineRegisterBank conflicting_policy_bank;
    ValueSsaError error;
    char *text = NULL;
    int ok = 1;

    value_ssa_machine_register_bank_init(&malformed_bank);
    value_ssa_machine_register_bank_init(&duplicate_id_bank);
    value_ssa_machine_register_bank_init(&conflicting_policy_bank);
    memset(&error, 0, sizeof(error));

    malformed_bank.register_count = 1;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-BANK-DUMP",
        value_ssa_dump_machine_register_bank(&malformed_bank, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-067");

    if (!build_caller_callee_machine_register_bank(&duplicate_id_bank)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: duplicate-id bank setup failed\n");
        ok = 0;
    } else {
        duplicate_id_bank.registers[1].register_id = duplicate_id_bank.registers[0].register_id;
        text = NULL;
        ok &= expect_machine_dump_failure(
            "VALUE-SSA-MACHINE-DUPLICATE-BANK-DUMP",
            value_ssa_dump_machine_register_bank(&duplicate_id_bank, &text, &error),
            &error,
            "VALUE-SSA-MACHINE-067");
    }

    if (!build_caller_callee_machine_register_bank(&conflicting_policy_bank)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: conflicting-policy bank setup failed\n");
        ok = 0;
    } else {
        conflicting_policy_bank.registers[0].callee_preserved = 1u;
        text = NULL;
        ok &= expect_machine_dump_failure(
            "VALUE-SSA-MACHINE-CONFLICTING-POLICY-BANK-DUMP",
            value_ssa_dump_machine_register_bank(&conflicting_policy_bank, &text, &error),
            &error,
            "VALUE-SSA-MACHINE-067");
    }

    free(text);
    value_ssa_machine_register_bank_free(&conflicting_policy_bank);
    value_ssa_machine_register_bank_free(&duplicate_id_bank);
    value_ssa_machine_register_bank_free(&malformed_bank);
    return ok;
}

static int test_value_ssa_machine_dump_rejects_malformed_program_view(void) {
    ValueSsaProgramMachineAllocationView view;
    ValueSsaError error;
    char *text = NULL;
    int ok = 1;

    value_ssa_program_machine_allocation_view_init(&view);
    memset(&error, 0, sizeof(error));
    view.function_count = 1;
    view.total_value_count = 1;
    view.total_machine_colored_count = 1;

    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-PROGRAM-DUMP",
        value_ssa_dump_program_machine_allocation_view(&view, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-066");

    text = NULL;
    view.functions_with_machine_colors_count = 1;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-PROGRAM-FILTER-DUMP",
        value_ssa_dump_program_machine_allocation_view(&view, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-066");

    free(text);
    value_ssa_program_machine_allocation_view_free(&view);
    return ok;
}

static int test_value_ssa_machine_dump_rejects_malformed_call_clobber_report(void) {
    ValueSsaMachineCallClobberRiskReport report;
    ValueSsaProgramMachineCallClobberRiskReport program_report;
    ValueSsaError error;
    char *text = NULL;
    int ok = 1;

    value_ssa_machine_call_clobber_risk_report_init(&report);
    value_ssa_program_machine_call_clobber_risk_report_init(&program_report);
    memset(&error, 0, sizeof(error));

    report.value_count = 1;
    report.item_count = 1;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-CALL-CLOBBER-DUMP",
        value_ssa_dump_machine_call_clobber_risk_report(&report, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-090");

    text = NULL;
    program_report.function_count = 1;
    program_report.total_value_count = 1;
    program_report.total_risky_value_count = 1;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-PROGRAM-CALL-CLOBBER-DUMP",
        value_ssa_dump_program_machine_call_clobber_risk_report(&program_report, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-091");

    text = NULL;
    {
        ValueSsaProgramMachineCallClobberRiskReport inconsistent_program_report;

        value_ssa_program_machine_call_clobber_risk_report_init(&inconsistent_program_report);
        inconsistent_program_report.function_count = 1;
        inconsistent_program_report.function_views =
            (ValueSsaFunctionMachineCallClobberRiskView *)calloc(1, sizeof(ValueSsaFunctionMachineCallClobberRiskView));
        inconsistent_program_report.function_indices_with_risky_values = (size_t *)malloc(sizeof(size_t));
        if (!inconsistent_program_report.function_views || !inconsistent_program_report.function_indices_with_risky_values) {
            fprintf(stderr, "[value-ssa-machine] FAIL: inconsistent call-clobber report setup failed\n");
            ok = 0;
        } else {
            value_ssa_function_machine_call_clobber_risk_view_init(&inconsistent_program_report.function_views[0]);
            inconsistent_program_report.functions_with_risky_values_count = 1;
            inconsistent_program_report.function_indices_with_risky_values[0] = 0;
            inconsistent_program_report.total_value_count = 1;
            inconsistent_program_report.total_risky_value_count = 1;
            ok &= expect_machine_dump_failure(
                "VALUE-SSA-MACHINE-INCONSISTENT-PROGRAM-CALL-CLOBBER-DUMP",
                value_ssa_dump_program_machine_call_clobber_risk_report(&inconsistent_program_report, &text, &error),
                &error,
                "VALUE-SSA-MACHINE-091");
        }
        value_ssa_program_machine_call_clobber_risk_report_free(&inconsistent_program_report);
    }

    value_ssa_machine_call_clobber_risk_report_free(&report);
    value_ssa_program_machine_call_clobber_risk_report_free(&program_report);
    return ok;
}

static int test_value_ssa_machine_dump_rejects_malformed_protection_report(void) {
    ValueSsaMachineProtectionAgenda agenda;
    ValueSsaProgramMachineProtectionReport report;
    ValueSsaProgramMachineRegisterProtectionPressureReport pressure_report;
    ValueSsaProgramMachinePreservationHintReport hint_report;
    ValueSsaProgramMachinePlanningReport planning_report;
    ValueSsaMachineRegisterProtectionPressureItem *malformed_pressure_items = NULL;
    ValueSsaFunctionMachinePlanningView malformed_planning_view;
    ValueSsaMachineRegisterProtectionPressureItem *malformed_planning_pressure_items = NULL;
    ValueSsaError error;
    char *text = NULL;
    int ok = 1;

    value_ssa_machine_protection_agenda_init(&agenda);
    value_ssa_program_machine_protection_report_init(&report);
    value_ssa_program_machine_register_protection_pressure_report_init(&pressure_report);
    value_ssa_program_machine_preservation_hint_report_init(&hint_report);
    value_ssa_program_machine_planning_report_init(&planning_report);
    value_ssa_function_machine_planning_view_init(&malformed_planning_view);
    memset(&error, 0, sizeof(error));

    agenda.value_count = 1;
    agenda.item_count = 1;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-PROTECTION-AGENDA-DUMP",
        value_ssa_dump_machine_protection_agenda(&agenda, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-180");

    text = NULL;
    report.function_count = 1;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-PROTECTION-REPORT-DUMP",
        value_ssa_dump_program_machine_protection_report(&report, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-181");

    text = NULL;
    {
        ValueSsaProgramMachineProtectionReport inconsistent_report;

        value_ssa_program_machine_protection_report_init(&inconsistent_report);
        inconsistent_report.function_count = 1;
        inconsistent_report.function_views =
            (ValueSsaFunctionMachineProtectionView *)calloc(1, sizeof(ValueSsaFunctionMachineProtectionView));
        inconsistent_report.function_indices_with_items = (size_t *)malloc(sizeof(size_t));
        if (!inconsistent_report.function_views || !inconsistent_report.function_indices_with_items) {
            fprintf(stderr, "[value-ssa-machine] FAIL: inconsistent protection-report setup failed\n");
            ok = 0;
        } else {
            value_ssa_function_machine_protection_view_init(&inconsistent_report.function_views[0]);
            inconsistent_report.functions_with_items_count = 1;
            inconsistent_report.function_indices_with_items[0] = 0;
            inconsistent_report.total_value_count = 1;
            inconsistent_report.total_item_count = 1;
            ok &= expect_machine_dump_failure(
                "VALUE-SSA-MACHINE-INCONSISTENT-PROTECTION-REPORT-DUMP",
                value_ssa_dump_program_machine_protection_report(&inconsistent_report, &text, &error),
                &error,
                "VALUE-SSA-MACHINE-181");
        }
        value_ssa_program_machine_protection_report_free(&inconsistent_report);
    }

    text = NULL;
    pressure_report.function_count = 1;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-PRESSURE-REPORT-DUMP",
        value_ssa_dump_program_machine_register_protection_pressure_report(&pressure_report, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-183");

    text = NULL;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-DUPLICATE-PRESSURE-REGISTER-DUMP",
        value_ssa_dump_machine_register_protection_pressure_view(
            &(ValueSsaMachineRegisterProtectionPressureView){
                .machine_register_count = 2,
                .pressured_register_count = 2,
                .total_item_count = 2,
                .total_protection_priority = 2,
                .risk_class_counts = {0, 0, 2, 0},
                .items = (ValueSsaMachineRegisterProtectionPressureItem[]){
                    {.machine_register_id = 0, .machine_register_name = "r0", .item_count = 1, .total_protection_priority = 1, .max_protection_priority = 1, .risk_class_counts = {0, 0, 1, 0}, .value_ids = (size_t[]){0}},
                    {.machine_register_id = 0, .machine_register_name = "r0b", .item_count = 1, .total_protection_priority = 1, .max_protection_priority = 1, .risk_class_counts = {0, 0, 1, 0}, .value_ids = (size_t[]){1}},
                },
            },
            &text,
            &error),
        &error,
        "VALUE-SSA-MACHINE-182");

    text = NULL;
    {
        ValueSsaProgramMachineRegisterProtectionPressureReport inconsistent_pressure_report;

        value_ssa_program_machine_register_protection_pressure_report_init(&inconsistent_pressure_report);
        inconsistent_pressure_report.function_count = 1;
        inconsistent_pressure_report.function_views = (ValueSsaFunctionMachineRegisterProtectionPressureView *)calloc(
            1, sizeof(ValueSsaFunctionMachineRegisterProtectionPressureView));
        inconsistent_pressure_report.function_indices_with_pressure = (size_t *)malloc(sizeof(size_t));
        if (!inconsistent_pressure_report.function_views || !inconsistent_pressure_report.function_indices_with_pressure) {
            fprintf(stderr, "[value-ssa-machine] FAIL: inconsistent pressure-report setup failed\n");
            ok = 0;
        } else {
            value_ssa_function_machine_register_protection_pressure_view_init(
                &inconsistent_pressure_report.function_views[0]);
            inconsistent_pressure_report.functions_with_pressure_count = 1;
            inconsistent_pressure_report.function_indices_with_pressure[0] = 0;
            inconsistent_pressure_report.total_value_count = 1;
            inconsistent_pressure_report.total_item_count = 1;
            inconsistent_pressure_report.total_protection_priority = 1;
            inconsistent_pressure_report.total_pressured_register_count = 1;
            ok &= expect_machine_dump_failure(
                "VALUE-SSA-MACHINE-INCONSISTENT-PRESSURE-REPORT-DUMP",
                value_ssa_dump_program_machine_register_protection_pressure_report(
                    &inconsistent_pressure_report, &text, &error),
                &error,
                "VALUE-SSA-MACHINE-183");
        }
        value_ssa_program_machine_register_protection_pressure_report_free(&inconsistent_pressure_report);
    }

    value_ssa_machine_register_protection_pressure_view_init(&malformed_planning_view.pressure_view);
    value_ssa_function_machine_register_protection_hotspot_view_init(&malformed_planning_view.hotspot_view);
    value_ssa_function_machine_preservation_hint_view_init(&malformed_planning_view.preservation_hint_view);
    malformed_pressure_items =
        (ValueSsaMachineRegisterProtectionPressureItem *)calloc(1, sizeof(ValueSsaMachineRegisterProtectionPressureItem));
    if (!malformed_pressure_items) {
        fprintf(stderr, "[value-ssa-machine] FAIL: malformed pressure-item setup failed\n");
        ok = 0;
        goto cleanup;
    }
    malformed_planning_pressure_items =
        (ValueSsaMachineRegisterProtectionPressureItem *)calloc(1, sizeof(ValueSsaMachineRegisterProtectionPressureItem));
    if (!malformed_planning_pressure_items) {
        fprintf(stderr, "[value-ssa-machine] FAIL: malformed planning pressure-item setup failed\n");
        ok = 0;
        goto cleanup;
    }
    malformed_pressure_items[0].item_count = 1;
    malformed_planning_pressure_items[0].item_count = 1;
    malformed_planning_view.pressure_view.pressured_register_count = 1;
    malformed_planning_view.pressure_view.items = malformed_planning_pressure_items;
    malformed_planning_view.pressure_view.items[0].item_count = 1;
    malformed_planning_view.pressure_view.items[0].value_ids = NULL;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-PRESSURE-VIEW-DUMP",
        value_ssa_dump_machine_register_protection_pressure_view(
            &(ValueSsaMachineRegisterProtectionPressureView){
                .machine_register_count = 1,
                .pressured_register_count = 1,
                .items = malformed_pressure_items,
            },
            &text,
            &error),
        &error,
        "VALUE-SSA-MACHINE-182");

    text = NULL;
    malformed_planning_view.pressure_view.pressured_register_count = 1;
    malformed_planning_view.pressure_view.items = malformed_planning_pressure_items;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-FUNCTION-PLANNING-PRESSURE-DUMP",
        value_ssa_dump_function_machine_planning_view(&malformed_planning_view, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-188");

    text = NULL;
    hint_report.function_count = 1;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-PRESERVATION-HINT-REPORT-DUMP",
        value_ssa_dump_program_machine_preservation_hint_report(&hint_report, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-185");

    text = NULL;
    {
        ValueSsaProgramMachinePreservationHintReport inconsistent_hint_report;

        value_ssa_program_machine_preservation_hint_report_init(&inconsistent_hint_report);
        inconsistent_hint_report.function_count = 1;
        inconsistent_hint_report.function_views =
            (ValueSsaFunctionMachinePreservationHintView *)calloc(1, sizeof(ValueSsaFunctionMachinePreservationHintView));
        inconsistent_hint_report.function_indices_with_hints = (size_t *)malloc(sizeof(size_t));
        inconsistent_hint_report.function_indices_with_reason[VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED] =
            (size_t *)malloc(sizeof(size_t));
        if (!inconsistent_hint_report.function_views || !inconsistent_hint_report.function_indices_with_hints ||
            !inconsistent_hint_report.function_indices_with_reason[VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED]) {
            fprintf(stderr, "[value-ssa-machine] FAIL: inconsistent hint-report setup failed\n");
            ok = 0;
        } else {
            value_ssa_function_machine_preservation_hint_view_init(&inconsistent_hint_report.function_views[0]);
            inconsistent_hint_report.functions_with_hints_count = 1;
            inconsistent_hint_report.function_indices_with_hints[0] = 0;
            inconsistent_hint_report.reason_counts[VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED] = 1;
            inconsistent_hint_report.functions_with_reason_counts
                [VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED] = 1;
            inconsistent_hint_report.function_indices_with_reason
                [VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED][0] = 0;
            ok &= expect_machine_dump_failure(
                "VALUE-SSA-MACHINE-INCONSISTENT-PRESERVATION-HINT-REPORT-DUMP",
                value_ssa_dump_program_machine_preservation_hint_report(&inconsistent_hint_report, &text, &error),
                &error,
                "VALUE-SSA-MACHINE-185");
        }
        value_ssa_program_machine_preservation_hint_report_free(&inconsistent_hint_report);
    }

    text = NULL;
    {
        ValueSsaFunctionMachinePreservationHintView hint_view;
        value_ssa_function_machine_preservation_hint_view_init(&hint_view);
        hint_view.has_hint = 1u;
        hint_view.candidate_count = 1;
        hint_view.candidates = (ValueSsaMachinePreservationHintCandidate *)calloc(
            1, sizeof(ValueSsaMachinePreservationHintCandidate));
        if (!hint_view.candidates) {
            fprintf(stderr, "[value-ssa-machine] FAIL: malformed function hint setup failed\n");
            ok = 0;
        } else {
            ok &= expect_machine_dump_failure(
                "VALUE-SSA-MACHINE-MALFORMED-FUNCTION-PRESERVATION-HINT-DUMP",
                value_ssa_dump_function_machine_preservation_hint_view(&hint_view, &text, &error),
                &error,
                "VALUE-SSA-MACHINE-184");
        }
        value_ssa_function_machine_preservation_hint_view_free(&hint_view);
    }

    text = NULL;
    planning_report.function_count = 1;
    ok &= expect_machine_dump_failure(
        "VALUE-SSA-MACHINE-MALFORMED-PLANNING-REPORT-DUMP",
        value_ssa_dump_program_machine_planning_report(&planning_report, &text, &error),
        &error,
        "VALUE-SSA-MACHINE-186");

    text = NULL;
    {
        ValueSsaProgramMachinePlanningReport inconsistent_planning_report;

        value_ssa_program_machine_planning_report_init(&inconsistent_planning_report);
        inconsistent_planning_report.function_count = 1;
        inconsistent_planning_report.function_views =
            (ValueSsaFunctionMachinePlanningView *)calloc(1, sizeof(ValueSsaFunctionMachinePlanningView));
        inconsistent_planning_report.function_indices_with_hints = (size_t *)malloc(sizeof(size_t));
        inconsistent_planning_report.function_indices_with_hint_reason
            [VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED] = (size_t *)malloc(sizeof(size_t));
        if (!inconsistent_planning_report.function_views || !inconsistent_planning_report.function_indices_with_hints ||
            !inconsistent_planning_report.function_indices_with_hint_reason
                 [VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED]) {
            fprintf(stderr, "[value-ssa-machine] FAIL: inconsistent planning-report setup failed\n");
            ok = 0;
        } else {
            value_ssa_function_machine_planning_view_init(&inconsistent_planning_report.function_views[0]);
            inconsistent_planning_report.functions_with_hints_count = 1;
            inconsistent_planning_report.function_indices_with_hints[0] = 0;
            inconsistent_planning_report.hint_reason_counts
                [VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED] = 1;
            inconsistent_planning_report.functions_with_hint_reason_counts
                [VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED] = 1;
            inconsistent_planning_report.function_indices_with_hint_reason
                [VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED][0] = 0;
            ok &= expect_machine_dump_failure(
                "VALUE-SSA-MACHINE-INCONSISTENT-PLANNING-REPORT-DUMP",
                value_ssa_dump_program_machine_planning_report(&inconsistent_planning_report, &text, &error),
                &error,
                "VALUE-SSA-MACHINE-186");
        }
        value_ssa_program_machine_planning_report_free(&inconsistent_planning_report);
    }

    text = NULL;
    {
        ValueSsaProgramMachinePlanningReport inconsistent_planning_totals_report;

        value_ssa_program_machine_planning_report_init(&inconsistent_planning_totals_report);
        inconsistent_planning_totals_report.function_count = 1;
        inconsistent_planning_totals_report.function_views =
            (ValueSsaFunctionMachinePlanningView *)calloc(1, sizeof(ValueSsaFunctionMachinePlanningView));
        if (!inconsistent_planning_totals_report.function_views) {
            fprintf(stderr, "[value-ssa-machine] FAIL: inconsistent planning-totals setup failed\n");
            ok = 0;
        } else {
            value_ssa_function_machine_planning_view_init(&inconsistent_planning_totals_report.function_views[0]);
            inconsistent_planning_totals_report.total_value_count = 1;
            inconsistent_planning_totals_report.total_pressure_item_count = 1;
            inconsistent_planning_totals_report.total_pressure_priority = 1;
            inconsistent_planning_totals_report.total_hotspot_priority = 1;
            inconsistent_planning_totals_report.total_hint_priority = 1;
            ok &= expect_machine_dump_failure(
                "VALUE-SSA-MACHINE-INCONSISTENT-PLANNING-TOTALS-DUMP",
                value_ssa_dump_program_machine_planning_report(&inconsistent_planning_totals_report, &text, &error),
                &error,
                "VALUE-SSA-MACHINE-186");
        }
        value_ssa_program_machine_planning_report_free(&inconsistent_planning_totals_report);
    }

    text = NULL;
    {
        ValueSsaProgramMachineRegisterProtectionHotspotReport hotspot_report;
        value_ssa_program_machine_register_protection_hotspot_report_init(&hotspot_report);
        hotspot_report.function_count = 1;
        ok &= expect_machine_dump_failure(
            "VALUE-SSA-MACHINE-MALFORMED-HOTSPOT-REPORT-DUMP",
            value_ssa_dump_program_machine_register_protection_hotspot_report(&hotspot_report, &text, &error),
            &error,
            "VALUE-SSA-MACHINE-187");
        value_ssa_program_machine_register_protection_hotspot_report_free(&hotspot_report);
    }

    text = NULL;
    {
        ValueSsaFunctionMachineRegisterProtectionHotspotView hotspot_view;
        value_ssa_function_machine_register_protection_hotspot_view_init(&hotspot_view);
        hotspot_view.has_hotspot = 1u;
        ok &= expect_machine_dump_failure(
            "VALUE-SSA-MACHINE-MALFORMED-FUNCTION-HOTSPOT-DUMP",
            value_ssa_dump_function_machine_register_protection_hotspot_view(&hotspot_view, &text, &error),
            &error,
            "VALUE-SSA-MACHINE-190");
        value_ssa_function_machine_register_protection_hotspot_view_free(&hotspot_view);
    }

    text = NULL;
    {
        ValueSsaFunctionMachineRegisterProtectionHotspotView hotspot_view;
        value_ssa_function_machine_register_protection_hotspot_view_init(&hotspot_view);
        hotspot_view.has_hotspot = 1u;
        hotspot_view.hotspot.machine_register_name = "r0";
        hotspot_view.hotspot.item_count = 2;
        hotspot_view.hotspot.total_protection_priority = 5;
        hotspot_view.hotspot.max_protection_priority = 5;
        hotspot_view.hotspot.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED] = 1;
        ok &= expect_machine_dump_failure(
            "VALUE-SSA-MACHINE-INCONSISTENT-FUNCTION-HOTSPOT-DUMP",
            value_ssa_dump_function_machine_register_protection_hotspot_view(&hotspot_view, &text, &error),
            &error,
            "VALUE-SSA-MACHINE-190");
        value_ssa_function_machine_register_protection_hotspot_view_free(&hotspot_view);
    }

    text = NULL;
    {
        ValueSsaFunctionMachinePlanningView planning_view;
        value_ssa_function_machine_planning_view_init(&planning_view);
        planning_view.pressure_view.pressured_register_count = 1;
        ok &= expect_machine_dump_failure(
            "VALUE-SSA-MACHINE-MALFORMED-FUNCTION-PLANNING-DUMP",
            value_ssa_dump_function_machine_planning_view(&planning_view, &text, &error),
            &error,
            "VALUE-SSA-MACHINE-188");
        value_ssa_function_machine_planning_view_free(&planning_view);
    }

cleanup:
    value_ssa_machine_protection_agenda_free(&agenda);
    value_ssa_program_machine_protection_report_free(&report);
    value_ssa_program_machine_register_protection_pressure_report_free(&pressure_report);
    value_ssa_program_machine_preservation_hint_report_free(&hint_report);
    value_ssa_program_machine_planning_report_free(&planning_report);
    value_ssa_function_machine_planning_view_free(&malformed_planning_view);
    free(malformed_pressure_items);
    return ok;
}

static int test_value_ssa_machine_summary_queries_clear_outputs_on_failure(void) {
    ValueSsaMachineRegisterBank malformed_bank;
    ValueSsaMachineRegisterBank duplicate_id_bank;
    ValueSsaMachineRegisterBank conflicting_policy_bank;
    ValueSsaMachineCallClobberRiskReport malformed_call_report;
    ValueSsaProgramMachineCallClobberRiskReport malformed_call_program_report;
    ValueSsaMachineProtectionAgenda malformed_agenda;
    ValueSsaProgramMachineProtectionReport malformed_protection_report;
    ValueSsaMachineRegisterProtectionPressureView malformed_pressure_view;
    ValueSsaProgramMachineRegisterProtectionPressureReport malformed_pressure_report;
    ValueSsaFunctionMachineRegisterProtectionHotspotView malformed_hotspot_view;
    ValueSsaProgramMachineRegisterProtectionHotspotReport malformed_hotspot_report;
    ValueSsaFunctionMachinePreservationHintView malformed_hint_view;
    ValueSsaProgramMachinePreservationHintReport malformed_hint_report;
    ValueSsaProgramMachinePlanningReport malformed_planning_report;
    ValueSsaFunctionMachineCallClobberRiskView *malformed_call_program_function_views = NULL;
    ValueSsaFunctionMachineProtectionView *malformed_protection_function_views = NULL;
    ValueSsaFunctionMachineRegisterProtectionPressureView *malformed_pressure_function_views = NULL;
    ValueSsaFunctionMachineRegisterProtectionHotspotView *malformed_hotspot_function_views = NULL;
    ValueSsaFunctionMachinePreservationHintView *malformed_hint_function_views = NULL;
    ValueSsaFunctionMachinePlanningView *malformed_planning_function_views = NULL;
    ValueSsaError error;
    char *text = NULL;
    size_t a = 123;
    size_t b = 456;
    size_t c = 789;
    size_t d = 321;
    size_t e = 654;
    size_t f = 987;
    int ok = 1;

    value_ssa_machine_register_bank_init(&malformed_bank);
    value_ssa_machine_register_bank_init(&duplicate_id_bank);
    value_ssa_machine_register_bank_init(&conflicting_policy_bank);
    malformed_bank.register_count = 1;
    if (!build_caller_callee_machine_register_bank(&duplicate_id_bank)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: duplicate-id bank setup failed\n");
        return 0;
    }
    duplicate_id_bank.registers[1].register_id = duplicate_id_bank.registers[0].register_id;
    if (!build_caller_callee_machine_register_bank(&conflicting_policy_bank)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: conflicting-policy bank setup failed\n");
        value_ssa_machine_register_bank_free(&duplicate_id_bank);
        value_ssa_machine_register_bank_free(&malformed_bank);
        return 0;
    }
    conflicting_policy_bank.registers[0].callee_preserved = 1u;
    value_ssa_machine_call_clobber_risk_report_init(&malformed_call_report);
    malformed_call_report.item_count = 1;
    value_ssa_program_machine_call_clobber_risk_report_init(&malformed_call_program_report);
    malformed_call_program_report.functions_with_risky_values_count = 1;
    value_ssa_machine_protection_agenda_init(&malformed_agenda);
    malformed_agenda.item_count = 1;
    value_ssa_program_machine_protection_report_init(&malformed_protection_report);
    malformed_protection_report.functions_with_items_count = 1;
    malformed_protection_report.functions_with_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED] = 1;
    value_ssa_machine_register_protection_pressure_view_init(&malformed_pressure_view);
    malformed_pressure_view.pressured_register_count = 1;
    value_ssa_program_machine_register_protection_pressure_report_init(&malformed_pressure_report);
    malformed_pressure_report.functions_with_pressure_count = 1;
    malformed_pressure_report.functions_with_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED] = 1;
    value_ssa_function_machine_register_protection_hotspot_view_init(&malformed_hotspot_view);
    malformed_hotspot_view.hotspot.item_count = 1;
    value_ssa_program_machine_register_protection_hotspot_report_init(&malformed_hotspot_report);
    malformed_hotspot_report.functions_with_hotspots_count = 1;
    value_ssa_function_machine_preservation_hint_view_init(&malformed_hint_view);
    malformed_hint_view.candidate_count = 1;
    value_ssa_program_machine_preservation_hint_report_init(&malformed_hint_report);
    malformed_hint_report.functions_with_hints_count = 1;
    malformed_hint_report.functions_with_reason_counts[VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED] = 1;
    value_ssa_program_machine_planning_report_init(&malformed_planning_report);
    malformed_planning_report.functions_with_pressure_count = 1;
    malformed_call_program_function_views =
        (ValueSsaFunctionMachineCallClobberRiskView *)calloc(1, sizeof(ValueSsaFunctionMachineCallClobberRiskView));
    malformed_protection_function_views =
        (ValueSsaFunctionMachineProtectionView *)calloc(1, sizeof(ValueSsaFunctionMachineProtectionView));
    malformed_pressure_function_views =
        (ValueSsaFunctionMachineRegisterProtectionPressureView *)calloc(
            1, sizeof(ValueSsaFunctionMachineRegisterProtectionPressureView));
    malformed_hotspot_function_views =
        (ValueSsaFunctionMachineRegisterProtectionHotspotView *)calloc(
            1, sizeof(ValueSsaFunctionMachineRegisterProtectionHotspotView));
    malformed_hint_function_views =
        (ValueSsaFunctionMachinePreservationHintView *)calloc(1, sizeof(ValueSsaFunctionMachinePreservationHintView));
    malformed_planning_function_views =
        (ValueSsaFunctionMachinePlanningView *)calloc(1, sizeof(ValueSsaFunctionMachinePlanningView));
    if (!malformed_call_program_function_views || !malformed_protection_function_views ||
        !malformed_pressure_function_views || !malformed_hotspot_function_views || !malformed_hint_function_views ||
        !malformed_planning_function_views) {
        fprintf(stderr, "[value-ssa-machine] FAIL: malformed nested report setup failed\n");
        ok = 0;
        goto cleanup_malformed_nested_reports;
    }
    value_ssa_function_machine_call_clobber_risk_view_init(&malformed_call_program_function_views[0]);
    malformed_call_program_function_views[0].risk_report.item_count = 1;
    malformed_call_program_report.function_count = 1;
    malformed_call_program_report.function_views = malformed_call_program_function_views;
    value_ssa_function_machine_protection_view_init(&malformed_protection_function_views[0]);
    malformed_protection_function_views[0].agenda.item_count = 1;
    malformed_protection_report.function_count = 1;
    malformed_protection_report.function_views = malformed_protection_function_views;
    value_ssa_function_machine_register_protection_pressure_view_init(&malformed_pressure_function_views[0]);
    malformed_pressure_function_views[0].pressure_view.pressured_register_count = 1;
    malformed_pressure_report.function_count = 1;
    malformed_pressure_report.function_views = malformed_pressure_function_views;
    value_ssa_function_machine_register_protection_hotspot_view_init(&malformed_hotspot_function_views[0]);
    malformed_hotspot_function_views[0].hotspot.item_count = 1;
    malformed_hotspot_report.function_count = 1;
    malformed_hotspot_report.function_views = malformed_hotspot_function_views;
    value_ssa_function_machine_preservation_hint_view_init(&malformed_hint_function_views[0]);
    malformed_hint_function_views[0].candidate_count = 1;
    malformed_hint_report.function_count = 1;
    malformed_hint_report.function_views = malformed_hint_function_views;
    value_ssa_function_machine_planning_view_init(&malformed_planning_function_views[0]);
    malformed_planning_function_views[0].preservation_hint_view.candidate_count = 1;
    malformed_planning_report.function_count = 1;
    malformed_planning_report.function_views = malformed_planning_function_views;
    text = (char *)1;

    if (value_ssa_program_machine_call_clobber_risk_report_get_summary(NULL, &a, &b, &c) ||
        a != 0 || b != 0 || c != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: call-clobber summary failure should clear outputs\n");
        ok = 0;
    }
    a = 123;
    b = 456;
    c = 789;
    if (value_ssa_program_machine_protection_report_get_summary(NULL, &a, &b, &c) ||
        a != 0 || b != 0 || c != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: protection summary failure should clear outputs\n");
        ok = 0;
    }
    a = 123;
    b = 456;
    c = 789;
    d = 321;
    e = 654;
    if (value_ssa_machine_register_protection_pressure_view_get_summary(NULL, &a, &b, &c, &d, &e, NULL, NULL, NULL) ||
        a != 0 || b != 0 || c != 0 || d != 0 || e != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: pressure-view summary failure should clear outputs\n");
        ok = 0;
    }
    a = 123;
    b = 456;
    c = 789;
    d = 321;
    e = 654;
    if (value_ssa_program_machine_register_protection_pressure_report_get_summary(NULL, &a, &b, &c, &d, &e) ||
        a != 0 || b != 0 || c != 0 || d != 0 || e != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: pressure summary failure should clear outputs\n");
        ok = 0;
    }
    a = 123;
    b = 456;
    c = 789;
    d = 321;
    e = 654;
    if (value_ssa_program_machine_register_protection_hotspot_report_get_summary(NULL, &a, &b, &c, &d, &e) ||
        a != 0 || b != 0 || c != 0 || d != 0 || e != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: hotspot summary failure should clear outputs\n");
        ok = 0;
    }
    a = 123;
    b = 456;
    c = 789;
    d = 321;
    if (value_ssa_program_machine_preservation_hint_report_get_summary(NULL, &a, &b, &c, &d) ||
        a != 0 || b != 0 || c != 0 || d != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation summary failure should clear outputs\n");
        ok = 0;
    }
    a = 123;
    b = 456;
    c = 789;
    d = 321;
    e = 654;
    f = 987;
    if (value_ssa_program_machine_planning_report_get_summary(NULL, &a, &b, &c, &d, &e, &f) ||
        a != 0 || b != 0 || c != 0 || d != 0 || e != 0 || f != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning summary failure should clear outputs\n");
        ok = 0;
    }
    if (value_ssa_dump_allocate_rewrite_machine_report(NULL, &duplicate_id_bank, &text, &error) || text != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: machine-report dump failure should clear text\n");
        ok = 0;
    }
    text = (char *)1;
    if (value_ssa_allocate_and_rewrite_program_single_block_spills_machine_report_dump(
            NULL, 2, &duplicate_id_bank, &text, &error) || text != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: report dump entrypoint failure should clear text\n");
        ok = 0;
    }

    {
        const ValueSsaMachineRegisterDesc *desc = (const ValueSsaMachineRegisterDesc *)1;
        const ValueSsaFunctionMachineAllocationView *function_view = (const ValueSsaFunctionMachineAllocationView *)1;
        const char *function_name = (const char *)1;

        a = 123;
        b = 456;
        c = 789;
        d = 321;
        if (value_ssa_machine_register_bank_get_summary(NULL, &a, &b, &c, &d) ||
            a != 0 || b != 0 || c != 0 || d != 0) {
            fprintf(stderr, "[value-ssa-machine] FAIL: bank summary failure should clear outputs\n");
            ok = 0;
        }
        a = 123;
        b = 456;
        if (value_ssa_machine_call_clobber_risk_report_get_summary(NULL, &a, &b) || a != 0 || b != 0) {
            fprintf(stderr, "[value-ssa-machine] FAIL: call-clobber local summary failure should clear outputs\n");
            ok = 0;
        }
        a = 123;
        {
            const ValueSsaMachineCallClobberRiskItem *item = (const ValueSsaMachineCallClobberRiskItem *)1;
            if (value_ssa_machine_call_clobber_risk_report_find_value(&malformed_call_report, 0, &a, &item) ||
                a != 0 || item != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed call-clobber item lookup should fail cleanly\n");
                ok = 0;
            }
        }
        a = 123;
        b = 456;
        c = 789;
        d = 321;
        e = 654;
        f = 987;
        if (value_ssa_program_machine_allocation_view_get_summary(NULL, &a, &b, &c, &d, &e, &f) ||
            a != 0 || b != 0 || c != 0 || d != 0 || e != 0 || f != 0) {
            fprintf(stderr, "[value-ssa-machine] FAIL: program allocation summary failure should clear outputs\n");
            ok = 0;
        }
        {
            ValueSsaProgramMachineAllocationView empty_view;
            const size_t *indices = (const size_t *)1;
            size_t count = 123;

            value_ssa_program_machine_allocation_view_init(&empty_view);
            empty_view.function_indices_with_machine_colors = (size_t *)1;
            if (!value_ssa_program_machine_allocation_view_get_functions_with_machine_colors(
                    &empty_view, &count, &indices) || count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: empty machine-colors filter should clear pointer\n");
                ok = 0;
            }
            indices = (const size_t *)1;
            count = 123;
            empty_view.function_indices_with_spills = (size_t *)1;
            if (!value_ssa_program_machine_allocation_view_get_functions_with_spills(
                    &empty_view, &count, &indices) || count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: empty spill filter should clear pointer\n");
                ok = 0;
            }
            indices = (const size_t *)1;
            count = 123;
            empty_view.function_indices_with_caller_clobbered_values = (size_t *)1;
            if (!value_ssa_program_machine_allocation_view_get_functions_with_caller_clobbered_values(
                    &empty_view, &count, &indices) || count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: empty caller-clobbered filter should clear pointer\n");
                ok = 0;
            }
            indices = (const size_t *)1;
            count = 123;
            empty_view.function_indices_with_callee_preserved_values = (size_t *)1;
            if (!value_ssa_program_machine_allocation_view_get_functions_with_callee_preserved_values(
                    &empty_view, &count, &indices) || count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: empty callee-preserved filter should clear pointer\n");
                ok = 0;
            }
        }
        {
            ValueSsaProgramMachineAllocationView malformed_program_view;
            const ValueSsaFunctionMachineAllocationView *nested_function_view =
                (const ValueSsaFunctionMachineAllocationView *)1;

            value_ssa_program_machine_allocation_view_init(&malformed_program_view);
            malformed_program_view.function_count = 1;
            malformed_program_view.function_views =
                (ValueSsaFunctionMachineAllocationView *)calloc(1, sizeof(ValueSsaFunctionMachineAllocationView));
            if (!malformed_program_view.function_views) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed program-allocation setup failed\n");
                ok = 0;
            } else {
                value_ssa_function_machine_allocation_view_init(&malformed_program_view.function_views[0]);
                malformed_program_view.function_views[0].machine_colored_count = 1;
                a = 123;
                b = 456;
                c = 789;
                d = 321;
                e = 654;
                f = 987;
                if (value_ssa_program_machine_allocation_view_get_summary(
                        &malformed_program_view, &a, &b, &c, &d, &e, &f) ||
                    a != 0 || b != 0 || c != 0 || d != 0 || e != 0 || f != 0) {
                    fprintf(stderr, "[value-ssa-machine] FAIL: malformed program-allocation summary should fail cleanly\n");
                    ok = 0;
                }
                a = 123;
                if (value_ssa_program_machine_allocation_view_get_function_view_by_name(
                        &malformed_program_view, "x", &a, &nested_function_view) ||
                    a != 0 || nested_function_view != NULL) {
                    fprintf(stderr, "[value-ssa-machine] FAIL: malformed program-allocation function lookup should fail cleanly\n");
                    ok = 0;
                }
            }
            value_ssa_program_machine_allocation_view_free(&malformed_program_view);
        }
        {
            ValueSsaFunctionMachineAllocationView malformed_view;
            const size_t *indices = (const size_t *)1;
            size_t count = 123;
            const char *function_name = (const char *)1;
            size_t a0 = 123;
            size_t b0 = 456;
            size_t c0 = 789;
            size_t d0 = 321;
            size_t e0 = 654;
            size_t f0 = 987;
            size_t g0 = 159;
            size_t h0 = 753;
            size_t i0 = 852;

            value_ssa_function_machine_allocation_view_init(&malformed_view);
            malformed_view.machine_colored_count = 1;
            if (value_ssa_function_machine_allocation_view_get_summary(
                    &malformed_view,
                    &function_name,
                    &a0,
                    &b0,
                    &c0,
                    &d0,
                    &e0,
                    &f0,
                    &g0,
                    &h0,
                    &i0) ||
                function_name != NULL || a0 != 0 || b0 != 0 || c0 != 0 || d0 != 0 || e0 != 0 || f0 != 0 ||
                g0 != 0 || h0 != 0 || i0 != 0) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed allocation summary should fail cleanly\n");
                ok = 0;
            }
            if (value_ssa_function_machine_allocation_view_get_machine_colored_values(
                    &malformed_view, &count, &indices) || count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed machine-colored query should fail cleanly\n");
                ok = 0;
            }
            indices = (const size_t *)1;
            count = 123;
            malformed_view.spilled_count = 1;
            if (value_ssa_function_machine_allocation_view_get_spilled_values(
                    &malformed_view, &count, &indices) || count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed spilled query should fail cleanly\n");
                ok = 0;
            }
            indices = (const size_t *)1;
            count = 123;
            malformed_view.caller_clobbered_value_count = 1;
            if (value_ssa_function_machine_allocation_view_get_caller_clobbered_values(
                    &malformed_view, &count, &indices) || count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed caller-clobbered query should fail cleanly\n");
                ok = 0;
            }
            indices = (const size_t *)1;
            count = 123;
            malformed_view.used_machine_register_count = 1;
            if (value_ssa_function_machine_allocation_view_get_used_machine_registers(
                    &malformed_view, &count, &indices) || count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed used-register query should fail cleanly\n");
                ok = 0;
            }
            value_ssa_function_machine_allocation_view_free(&malformed_view);
        }
        {
            const size_t *indices = (const size_t *)1;
            size_t count = 123;

            if (value_ssa_program_machine_call_clobber_risk_report_get_functions_with_risky_values(
                    &malformed_call_program_report, &count, &indices) ||
                count != 0 || indices != NULL) {
                fprintf(stderr,
                    "[value-ssa-machine] FAIL: malformed call-clobber risky filter should fail cleanly\n");
                ok = 0;
            }
        }
        {
            size_t count = 123;
            if (value_ssa_program_machine_call_clobber_risk_report_get_summary(
                    &malformed_call_program_report, &count, &b, &c) || count != 0 || b != 0 || c != 0) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed call-clobber report should fail cleanly\n");
                ok = 0;
            }
        }
        {
            const ValueSsaMachineProtectionAgendaItem *agenda_item = (const ValueSsaMachineProtectionAgendaItem *)1;
            size_t index = 123;

            if (value_ssa_machine_protection_agenda_find_value(&malformed_agenda, 0, &index, &agenda_item) ||
                index != 0 || agenda_item != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed protection agenda lookup should fail cleanly\n");
                ok = 0;
            }
        }
        {
            const size_t *indices = (const size_t *)1;
            size_t count = 123;

            if (value_ssa_program_machine_protection_report_get_functions_with_items(
                    &malformed_protection_report, &count, &indices) ||
                count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed protection item filter should fail cleanly\n");
                ok = 0;
            }
            indices = (const size_t *)1;
            count = 123;
            if (value_ssa_program_machine_protection_report_get_functions_with_risk_class(
                    &malformed_protection_report,
                    VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED,
                    &count,
                    &indices) ||
                count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed protection class filter should fail cleanly\n");
                ok = 0;
            }
        }
        {
            size_t count = 123;
            if (value_ssa_program_machine_protection_report_get_summary(
                    &malformed_protection_report, &count, &b, &c) || count != 0 || b != 0 || c != 0) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed protection report should fail cleanly\n");
                ok = 0;
            }
        }
        {
            const ValueSsaMachineRegisterProtectionPressureItem *pressure_item =
                (const ValueSsaMachineRegisterProtectionPressureItem *)1;

            if (value_ssa_machine_register_protection_pressure_view_get_register_item(
                    &malformed_pressure_view, 0, &pressure_item) || pressure_item != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed pressure-view lookup should fail cleanly\n");
                ok = 0;
            }
        }
        {
            const ValueSsaMachineRegisterProtectionHotspotItem *hotspot_item =
                (const ValueSsaMachineRegisterProtectionHotspotItem *)1;
            a = 123;
            b = 456;
            if (value_ssa_function_machine_register_protection_hotspot_view_get_summary(
                    &malformed_hotspot_view, NULL, NULL, &a, &b, &hotspot_item) ||
                a != 0 || b != 0 || hotspot_item != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed hotspot-view summary should fail cleanly\n");
                ok = 0;
            }
        }
        {
            const size_t *indices = (const size_t *)1;
            size_t count = 123;

            if (value_ssa_program_machine_register_protection_pressure_report_get_functions_with_pressure(
                    &malformed_pressure_report, &count, &indices) ||
                count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed pressure filter should fail cleanly\n");
                ok = 0;
            }
        }
        {
            size_t count = 123;
            if (value_ssa_program_machine_register_protection_pressure_report_get_summary(
                    &malformed_pressure_report, &count, &b, &c, &d, &e) ||
                count != 0 || b != 0 || c != 0 || d != 0 || e != 0) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed pressure report should fail cleanly\n");
                ok = 0;
            }
        }
        {
            const size_t *indices = (const size_t *)1;
            size_t count = 123;

            if (value_ssa_program_machine_register_protection_hotspot_report_get_functions_with_hotspots(
                    &malformed_hotspot_report, &count, &indices) ||
                count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed hotspot filter should fail cleanly\n");
                ok = 0;
            }
        }
        {
            size_t count = 123;
            if (value_ssa_program_machine_register_protection_hotspot_report_get_summary(
                    &malformed_hotspot_report, &count, &b, &c, &d, &e) ||
                count != 0 || b != 0 || c != 0 || d != 0 || e != 0) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed hotspot report should fail cleanly\n");
                ok = 0;
            }
        }
        {
            const ValueSsaMachinePreservationHintCandidate *candidate =
                (const ValueSsaMachinePreservationHintCandidate *)1;

            a = 123;
            b = 456;
            c = 789;
            if (value_ssa_function_machine_preservation_hint_view_get_summary(
                    &malformed_hint_view, NULL, NULL, &a, &b, &c) ||
                a != 0 || b != 0 || c != 0) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed hint-view summary should fail cleanly\n");
                ok = 0;
            }
            if (value_ssa_function_machine_preservation_hint_view_get_candidate_at(
                    &malformed_hint_view, 0, &candidate) || candidate != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed hint-view candidate lookup should fail cleanly\n");
                ok = 0;
            }
        }
        {
            const size_t *indices = (const size_t *)1;
            size_t count = 123;

            if (value_ssa_program_machine_preservation_hint_report_get_functions_with_hints(
                    &malformed_hint_report, &count, &indices) ||
                count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed hint filter should fail cleanly\n");
                ok = 0;
            }
            indices = (const size_t *)1;
            count = 123;
            if (value_ssa_program_machine_preservation_hint_report_get_functions_with_reason(
                    &malformed_hint_report,
                    VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED,
                    &count,
                    &indices) ||
                count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed hint-reason filter should fail cleanly\n");
                ok = 0;
            }
        }
        {
            size_t count = 123;
            if (value_ssa_program_machine_preservation_hint_report_get_summary(
                    &malformed_hint_report, &count, &b, &c, &d) || count != 0 || b != 0 || c != 0 || d != 0) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed hint report should fail cleanly\n");
                ok = 0;
            }
        }
        {
            const size_t *indices = (const size_t *)1;
            size_t count = 123;

            if (value_ssa_program_machine_planning_report_get_functions_with_pressure(
                    &malformed_planning_report, &count, &indices) || count != 0 || indices != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed planning filter should fail cleanly\n");
                ok = 0;
            }
        }
        {
            ValueSsaFunctionMachinePlanningView malformed_planning_view;

            value_ssa_function_machine_planning_view_init(&malformed_planning_view);
            malformed_planning_view.preservation_hint_view.candidate_count = 1;
            a = 123;
            b = 456;
            c = 789;
            if (value_ssa_function_machine_planning_view_get_summary(
                    &malformed_planning_view, NULL, &a, &b, &c, NULL, NULL) ||
                a != 0 || b != 0 || c != 0) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed planning-view summary should fail cleanly\n");
                ok = 0;
            }
            value_ssa_function_machine_planning_view_free(&malformed_planning_view);
        }
        {
            size_t count = 123;
            if (value_ssa_program_machine_planning_report_get_summary(
                    &malformed_planning_report, &count, &b, &c, &d, &e, &f) ||
                count != 0 || b != 0 || c != 0 || d != 0 || e != 0 || f != 0) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed planning report should fail cleanly\n");
                ok = 0;
            }
        }
        function_name = (const char *)1;
        a = 123;
        b = 456;
        c = 789;
        d = 321;
        e = 654;
        f = 987;
        if (value_ssa_function_machine_allocation_view_get_summary(
                NULL, &function_name, &a, &b, &c, &d, &e, &f, NULL, NULL, NULL) ||
            function_name != NULL || a != 0 || b != 0 || c != 0 || d != 0 || e != 0 || f != 0) {
            fprintf(stderr, "[value-ssa-machine] FAIL: function allocation summary failure should clear outputs\n");
            ok = 0;
        }
        desc = (const ValueSsaMachineRegisterDesc *)1;
        if (value_ssa_machine_register_bank_get_register(NULL, 0, &desc) || desc != NULL) {
            fprintf(stderr, "[value-ssa-machine] FAIL: direct register query failure should clear output\n");
            ok = 0;
        }
        a = 123;
        b = 456;
        c = 789;
        d = 321;
        if (value_ssa_machine_register_bank_get_summary(&malformed_bank, &a, &b, &c, &d) ||
            a != 0 || b != 0 || c != 0 || d != 0) {
            fprintf(stderr, "[value-ssa-machine] FAIL: malformed bank summary should fail cleanly\n");
            ok = 0;
        }
        a = 123;
        desc = (const ValueSsaMachineRegisterDesc *)1;
        if (value_ssa_machine_register_bank_find_register_by_name(&malformed_bank, "r0", &a, &desc) ||
            a != 0 || desc != NULL) {
            fprintf(stderr, "[value-ssa-machine] FAIL: malformed bank name lookup should fail cleanly\n");
            ok = 0;
        }
        a = 123;
        b = 456;
        c = 789;
        d = 321;
        if (value_ssa_machine_register_bank_get_summary(&duplicate_id_bank, &a, &b, &c, &d) ||
            a != 0 || b != 0 || c != 0 || d != 0) {
            fprintf(stderr, "[value-ssa-machine] FAIL: duplicate-id bank summary should fail cleanly\n");
            ok = 0;
        }
        a = 123;
        desc = (const ValueSsaMachineRegisterDesc *)1;
        if (value_ssa_machine_register_bank_find_register_by_name(&duplicate_id_bank, "r0", &a, &desc) ||
            a != 0 || desc != NULL) {
            fprintf(stderr, "[value-ssa-machine] FAIL: duplicate-id bank name lookup should fail cleanly\n");
            ok = 0;
        }
        a = 123;
        b = 456;
        c = 789;
        d = 321;
        if (value_ssa_machine_register_bank_get_summary(&conflicting_policy_bank, &a, &b, &c, &d) ||
            a != 0 || b != 0 || c != 0 || d != 0) {
            fprintf(stderr, "[value-ssa-machine] FAIL: conflicting-policy bank summary should fail cleanly\n");
            ok = 0;
        }
        a = 123;
        desc = (const ValueSsaMachineRegisterDesc *)1;
        if (value_ssa_machine_register_bank_find_register_by_name(&conflicting_policy_bank, "r0", &a, &desc) ||
            a != 0 || desc != NULL) {
            fprintf(stderr, "[value-ssa-machine] FAIL: conflicting-policy bank name lookup should fail cleanly\n");
            ok = 0;
        }
        {
            const ValueSsaMachineAllocatedValuePlacement *placement = (const ValueSsaMachineAllocatedValuePlacement *)1;
            if (value_ssa_function_machine_allocation_view_get_placement(NULL, 0, &placement) || placement != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: placement query failure should clear output\n");
                ok = 0;
            }
        }
        a = 123;
        function_view = (const ValueSsaFunctionMachineAllocationView *)1;
        if (value_ssa_program_machine_allocation_view_get_function_view_by_name(NULL, "x", &a, &function_view) ||
            a != 0 || function_view != NULL) {
            fprintf(stderr, "[value-ssa-machine] FAIL: function-by-name failure should clear outputs\n");
            ok = 0;
        }
        a = 123;
        b = 456;
        if (value_ssa_allocate_rewrite_machine_report_get_stats(NULL, &a, &b) || a != 0 || b != 0) {
            fprintf(stderr, "[value-ssa-machine] FAIL: machine report stats failure should clear outputs\n");
            ok = 0;
        }
        {
            ValueSsaAllocateRewriteMachineReport malformed_machine_report;
            const ValueSsaProgramMachineAllocationView *program_view = (const ValueSsaProgramMachineAllocationView *)1;

            value_ssa_allocate_rewrite_machine_report_init(&malformed_machine_report);
            malformed_machine_report.view.function_count = 1;
            a = 123;
            b = 456;
            c = 789;
            d = 321;
            e = 654;
            f = 987;
            if (value_ssa_allocate_rewrite_machine_report_get_program_view(
                    &malformed_machine_report, &program_view) || program_view != NULL) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed machine report program-view query should fail cleanly\n");
                ok = 0;
            }
            if (value_ssa_allocate_rewrite_machine_report_get_summary(
                    &malformed_machine_report, &a, &b, &c, &d, &e, &f) ||
                a != 0 || b != 0 || c != 0 || d != 0 || e != 0 || f != 0) {
                fprintf(stderr, "[value-ssa-machine] FAIL: malformed machine report summary should fail cleanly\n");
                ok = 0;
            }
            value_ssa_allocate_rewrite_machine_report_free(&malformed_machine_report);
        }
        a = 123;
        b = 456;
        c = 789;
        d = 321;
        e = 654;
        f = 987;
        if (value_ssa_allocate_rewrite_machine_report_get_summary(NULL, &a, &b, &c, &d, &e, &f) ||
            a != 0 || b != 0 || c != 0 || d != 0 || e != 0 || f != 0) {
            fprintf(stderr, "[value-ssa-machine] FAIL: machine report summary failure should clear outputs\n");
            ok = 0;
        }
    }

cleanup_malformed_nested_reports:
    value_ssa_machine_register_bank_free(&conflicting_policy_bank);
    value_ssa_machine_register_bank_free(&duplicate_id_bank);
    value_ssa_machine_register_bank_free(&malformed_bank);
    value_ssa_machine_call_clobber_risk_report_free(&malformed_call_report);
    value_ssa_program_machine_call_clobber_risk_report_free(&malformed_call_program_report);
    value_ssa_machine_protection_agenda_free(&malformed_agenda);
    value_ssa_program_machine_protection_report_free(&malformed_protection_report);
    value_ssa_machine_register_protection_pressure_view_free(&malformed_pressure_view);
    value_ssa_program_machine_register_protection_pressure_report_free(&malformed_pressure_report);
    value_ssa_function_machine_register_protection_hotspot_view_free(&malformed_hotspot_view);
    value_ssa_program_machine_register_protection_hotspot_report_free(&malformed_hotspot_report);
    value_ssa_function_machine_preservation_hint_view_free(&malformed_hint_view);
    value_ssa_program_machine_preservation_hint_report_free(&malformed_hint_report);
    value_ssa_program_machine_planning_report_free(&malformed_planning_report);

    return ok;
}

static int test_value_ssa_allocate_and_rewrite_machine_report_entrypoints(void) {
    ValueSsaProgram program_a;
    ValueSsaProgram program_b;
    ValueSsaProgram program_c;
    ValueSsaMachineRegisterBank bank;
    ValueSsaAllocateRewriteMachineReport direct_report;
    ValueSsaAllocateRewriteLayoutReport layout_report;
    ValueSsaAllocateRewriteMachineReport rebuilt_report;
    ValueSsaAllocateRewriteMachineReport flat_report;
    ValueSsaError error;
    char *direct_text = NULL;
    char *dump_text = NULL;
    char *flat_dump_text = NULL;
    int ok = 1;

    if (!build_alloc_spill_program(&program_a, &error) ||
        !build_alloc_spill_program(&program_b, &error) ||
        !build_alloc_spill_program(&program_c, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: machine-entry setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    value_ssa_allocate_rewrite_machine_report_init(&direct_report);
    value_ssa_allocate_rewrite_layout_report_init(&layout_report);
    value_ssa_allocate_rewrite_machine_report_init(&rebuilt_report);
    value_ssa_allocate_rewrite_machine_report_init(&flat_report);
    if (!value_ssa_build_flat_machine_register_bank(2, &bank, &error) ||
        !value_ssa_allocate_and_rewrite_program_single_block_spills_machine_report(
            &program_a, 2, &bank, &direct_report, &error) ||
        !value_ssa_allocate_and_rewrite_program_single_block_spills_layout_report(
            &program_b, 2, &layout_report, &error) ||
        !value_ssa_build_allocate_rewrite_machine_report(&layout_report, &bank, &rebuilt_report, &error) ||
        !value_ssa_allocate_and_rewrite_program_single_block_spills_flat_machine_report(
            &program_c, 2, 2, &flat_report, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: machine-entry build failed: %s\n", error.message);
        value_ssa_allocate_rewrite_machine_report_free(&flat_report);
        value_ssa_allocate_rewrite_machine_report_free(&rebuilt_report);
        value_ssa_allocate_rewrite_layout_report_free(&layout_report);
        value_ssa_allocate_rewrite_machine_report_free(&direct_report);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_program_free(&program_c);
        value_ssa_program_free(&program_b);
        value_ssa_program_free(&program_a);
        return 0;
    }

    if (!value_ssa_dump_allocate_rewrite_machine_report_artifact(&direct_report, &direct_text, &error) ||
        !value_ssa_dump_allocate_rewrite_machine_report_artifact(&rebuilt_report, &dump_text, &error) ||
        !value_ssa_dump_allocate_rewrite_machine_report_artifact(&flat_report, &flat_dump_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: machine-entry dump failed: %s\n", error.message);
        free(flat_dump_text);
        free(dump_text);
        free(direct_text);
        value_ssa_allocate_rewrite_machine_report_free(&flat_report);
        value_ssa_allocate_rewrite_machine_report_free(&rebuilt_report);
        value_ssa_allocate_rewrite_layout_report_free(&layout_report);
        value_ssa_allocate_rewrite_machine_report_free(&direct_report);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_program_free(&program_c);
        value_ssa_program_free(&program_b);
        value_ssa_program_free(&program_a);
        return 0;
    }

    if (strcmp(direct_text, dump_text) != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: machine-entry reports differ\nlhs:\n%s\nrhs:\n%s\n",
            direct_text,
            dump_text);
        ok = 0;
    }
    if (strcmp(direct_text, flat_dump_text) != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: machine-entry flat report differs\nlhs:\n%s\nrhs:\n%s\n",
            direct_text,
            flat_dump_text);
        ok = 0;
    }

    free(flat_dump_text);
    free(dump_text);
    free(direct_text);
    value_ssa_allocate_rewrite_machine_report_free(&flat_report);
    value_ssa_allocate_rewrite_machine_report_free(&rebuilt_report);
    value_ssa_allocate_rewrite_layout_report_free(&layout_report);
    value_ssa_allocate_rewrite_machine_report_free(&direct_report);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_program_free(&program_c);
    value_ssa_program_free(&program_b);
    value_ssa_program_free(&program_a);
    return ok;
}

static int test_value_ssa_machine_view_entrypoints(void) {
    ValueSsaProgram program_a;
    ValueSsaProgram program_b;
    ValueSsaProgram program_c;
    ValueSsaProgram program_d;
    ValueSsaMachineRegisterBank bank;
    ValueSsaFunctionMachineAllocationView direct_function_view;
    ValueSsaFunctionMachineAllocationView flat_function_view;
    ValueSsaProgramMachineAllocationView direct_program_view;
    ValueSsaProgramMachineAllocationView flat_program_view;
    char *direct_text = NULL;
    char *flat_text = NULL;
    char *direct_program_text = NULL;
    char *flat_program_text = NULL;
    char *direct_function_dump_text = NULL;
    char *flat_function_dump_text = NULL;
    char *direct_program_dump_text = NULL;
    char *flat_program_dump_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!build_alloc_spill_program(&program_a, &error) ||
        !build_alloc_spill_program(&program_b, &error) ||
        !build_alloc_two_function_program(&program_c, &error) ||
        !build_alloc_two_function_program(&program_d, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: machine-view entry setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    value_ssa_function_machine_allocation_view_init(&direct_function_view);
    value_ssa_function_machine_allocation_view_init(&flat_function_view);
    value_ssa_program_machine_allocation_view_init(&direct_program_view);
    value_ssa_program_machine_allocation_view_init(&flat_program_view);
    if (!value_ssa_build_flat_machine_register_bank(2, &bank, &error) ||
        !value_ssa_allocate_function_machine_view(&program_a.functions[0], 2, &bank, &direct_function_view, &error) ||
        !value_ssa_allocate_function_flat_machine_view(&program_b.functions[0], 2, 2, &flat_function_view, &error) ||
        !value_ssa_allocate_program_machine_view(&program_c, 2, &bank, &direct_program_view, &error) ||
        !value_ssa_allocate_program_flat_machine_view(&program_c, 2, 2, &flat_program_view, &error) ||
        !value_ssa_allocate_function_machine_view_dump(
            &program_a.functions[0], 2, &bank, &direct_function_dump_text, &error) ||
        !value_ssa_allocate_function_flat_machine_view_dump(
            &program_b.functions[0], 2, 2, &flat_function_dump_text, &error) ||
        !value_ssa_allocate_program_machine_view_dump(&program_d, 2, &bank, &direct_program_dump_text, &error) ||
        !value_ssa_allocate_program_flat_machine_view_dump(&program_d, 2, 2, &flat_program_dump_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: machine-view entry build failed: %s\n", error.message);
        free(flat_program_dump_text);
        free(direct_program_dump_text);
        free(flat_function_dump_text);
        free(direct_function_dump_text);
        value_ssa_program_machine_allocation_view_free(&flat_program_view);
        value_ssa_program_machine_allocation_view_free(&direct_program_view);
        value_ssa_function_machine_allocation_view_free(&flat_function_view);
        value_ssa_function_machine_allocation_view_free(&direct_function_view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_program_free(&program_d);
        value_ssa_program_free(&program_c);
        value_ssa_program_free(&program_b);
        value_ssa_program_free(&program_a);
        return 0;
    }

    if (!value_ssa_dump_function_machine_allocation_view(&direct_function_view, &direct_text, &error) ||
        !value_ssa_dump_function_machine_allocation_view(&flat_function_view, &flat_text, &error) ||
        !value_ssa_dump_program_machine_allocation_view(&direct_program_view, &direct_program_text, &error) ||
        !value_ssa_dump_program_machine_allocation_view(&flat_program_view, &flat_program_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: machine-view entry dump failed: %s\n", error.message);
        free(flat_program_text);
        free(direct_program_text);
        free(flat_text);
        free(direct_text);
        value_ssa_program_machine_allocation_view_free(&flat_program_view);
        value_ssa_program_machine_allocation_view_free(&direct_program_view);
        value_ssa_function_machine_allocation_view_free(&flat_function_view);
        value_ssa_function_machine_allocation_view_free(&direct_function_view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_program_free(&program_c);
        value_ssa_program_free(&program_b);
        value_ssa_program_free(&program_a);
        return 0;
    }

    if (strcmp(direct_text, flat_text) != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: function machine-view entrypoints differ\nlhs:\n%s\nrhs:\n%s\n",
            direct_text,
            flat_text);
        ok = 0;
    }
    if (strcmp(direct_program_text, flat_program_text) != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: program machine-view entrypoints differ\nlhs:\n%s\nrhs:\n%s\n",
            direct_program_text,
            flat_program_text);
        ok = 0;
    }
    if (strcmp(direct_text, direct_function_dump_text) != 0 || strcmp(direct_text, flat_function_dump_text) != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: function machine-view dump entrypoints differ\nview:\n%s\ndirect_dump:\n%s\nflat_dump:\n%s\n",
            direct_text,
            direct_function_dump_text,
            flat_function_dump_text);
        ok = 0;
    }
    if (strcmp(direct_program_text, direct_program_dump_text) != 0 ||
        strcmp(direct_program_text, flat_program_dump_text) != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: program machine-view dump entrypoints differ\nview:\n%s\ndirect_dump:\n%s\nflat_dump:\n%s\n",
            direct_program_text,
            direct_program_dump_text,
            flat_program_dump_text);
        ok = 0;
    }

    free(flat_program_dump_text);
    free(direct_program_dump_text);
    free(flat_function_dump_text);
    free(direct_function_dump_text);
    free(flat_program_text);
    free(direct_program_text);
    free(flat_text);
    free(direct_text);
    value_ssa_program_machine_allocation_view_free(&flat_program_view);
    value_ssa_program_machine_allocation_view_free(&direct_program_view);
    value_ssa_function_machine_allocation_view_free(&flat_function_view);
    value_ssa_function_machine_allocation_view_free(&direct_function_view);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_program_free(&program_d);
    value_ssa_program_free(&program_c);
    value_ssa_program_free(&program_b);
    value_ssa_program_free(&program_a);
    return ok;
}

static int test_value_ssa_machine_call_clobber_risk_report(void) {
    ValueSsaProgram program;
    ValueSsaAllocationPrep prep;
    ValueSsaMachineCallClobberRiskReport report;
    const ValueSsaMachineCallClobberRiskItem *item = NULL;
    char *actual_text = NULL;
    ValueSsaError error;
    size_t call_live_value = (size_t)-1;
    size_t post_call_value = (size_t)-1;
    size_t value_count = 0;
    size_t risky_value_count = 0;
    size_t crossing_count = 0;
    size_t repeated_count = 0;
    size_t hot_count = 0;
    int ok = 1;

    if (!build_alloc_call_density_prep_program(&program, &call_live_value, &post_call_value, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: call-clobber-risk setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_prep_init(&prep);
    value_ssa_machine_call_clobber_risk_report_init(&report);
    if (!value_ssa_compute_allocation_prep(&program.functions[1], NULL, NULL, NULL, NULL, &prep, &error) ||
        !value_ssa_compute_function_flat_machine_call_clobber_risk(
            &program.functions[1], 1, 1, &prep, &report, &error) ||
        !value_ssa_dump_machine_call_clobber_risk_report(&report, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: call-clobber-risk build failed: %s\n", error.message);
        free(actual_text);
        value_ssa_machine_call_clobber_risk_report_free(&report);
        value_ssa_allocation_prep_free(&prep);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text,
            "machine-call-clobber-risk values=3 risky=1 crossing=0 repeated=1 hot=0\n"
            "ssa.0 reg=r0 class=repeated call_cross=1 calls=1 use_calls=1\n") != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: call-clobber-risk dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }
    if (!value_ssa_machine_call_clobber_risk_report_get_summary(&report, &value_count, &risky_value_count) ||
        value_count != 3 || risky_value_count != 1) {
        fprintf(stderr, "[value-ssa-machine] FAIL: call-clobber-risk summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_machine_call_clobber_risk_report_get_class_count(
            &report, VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING, &crossing_count) ||
        !value_ssa_machine_call_clobber_risk_report_get_class_count(
            &report, VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED, &repeated_count) ||
        !value_ssa_machine_call_clobber_risk_report_get_class_count(
            &report, VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT, &hot_count) ||
        crossing_count != 0 || repeated_count != 1 || hot_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: call-clobber-risk class counts mismatch\n");
        ok = 0;
    }
    if (!value_ssa_machine_call_clobber_risk_report_find_value(&report, call_live_value, NULL, &item) ||
        !item || item->call_crossing_count != 1 || !item->machine_register_name ||
        strcmp(item->machine_register_name, "r0") != 0 ||
        item->risk_class != VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED) {
        fprintf(stderr, "[value-ssa-machine] FAIL: call-clobber-risk item lookup mismatch\n");
        ok = 0;
    }
    if (value_ssa_machine_call_clobber_risk_report_find_value(&report, post_call_value, NULL, &item)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: call-clobber-risk should not include non-crossing value\n");
        ok = 0;
    }

    free(actual_text);
    value_ssa_machine_call_clobber_risk_report_free(&report);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_program_machine_call_clobber_risk_report(void) {
    ValueSsaProgram program;
    ValueSsaProgramMachineCallClobberRiskReport report;
    const ValueSsaFunctionMachineCallClobberRiskView *function_view = NULL;
    const ValueSsaMachineCallClobberRiskItem *item = NULL;
    const size_t *risky_function_indices = NULL;
    const size_t *class_function_indices = NULL;
    char *actual_text = NULL;
    ValueSsaError error;
    size_t function_count = 0;
    size_t total_value_count = 0;
    size_t total_risky_value_count = 0;
    size_t risky_function_count = 0;
    size_t crossing_count = 0;
    size_t repeated_count = 0;
    size_t hot_count = 0;
    size_t class_function_count = 0;
    size_t function_index = (size_t)-1;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-call-clobber-risk setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_machine_call_clobber_risk_report_init(&report);
    if (!value_ssa_compute_program_flat_machine_call_clobber_risk(&program, 2, 2, &report, &error) ||
        !value_ssa_dump_program_machine_call_clobber_risk_report(&report, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-call-clobber-risk build failed: %s\n", error.message);
        free(actual_text);
        value_ssa_program_machine_call_clobber_risk_report_free(&report);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text,
            "program-machine-call-clobber-risk functions=2 values=10 risky=0 risky_funcs=0 crossing=0 repeated=0 hot=0\n"
            "function main risky=0 values=5 crossing=0 repeated=0 hot=0\n"
            "machine-call-clobber-risk values=5 risky=0 crossing=0 repeated=0 hot=0\n"
            "function spill risky=0 values=5 crossing=0 repeated=0 hot=0\n"
            "machine-call-clobber-risk values=5 risky=0 crossing=0 repeated=0 hot=0\n") != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: program-call-clobber-risk dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }

    if (!value_ssa_program_machine_call_clobber_risk_report_get_summary(
            &report, &function_count, &total_value_count, &total_risky_value_count) ||
        function_count != 2 || total_value_count != 10 || total_risky_value_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-call-clobber-risk summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_call_clobber_risk_report_get_class_count(
            &report, VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING, &crossing_count) ||
        !value_ssa_program_machine_call_clobber_risk_report_get_class_count(
            &report, VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED, &repeated_count) ||
        !value_ssa_program_machine_call_clobber_risk_report_get_class_count(
            &report, VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT, &hot_count) ||
        crossing_count != 0 || repeated_count != 0 || hot_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-call-clobber-risk class summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_call_clobber_risk_report_get_functions_with_risky_values(
            &report, &risky_function_count, &risky_function_indices) ||
        risky_function_count != 0 || risky_function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-call-clobber-risk filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_call_clobber_risk_report_get_functions_with_risk_class(
            &report,
            VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED,
            &class_function_count,
            &class_function_indices) ||
        class_function_count != 0 || class_function_indices != NULL) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: program-call-clobber-risk class filter mismatch count=%zu ptr=%p\n",
            class_function_count,
            (const void *)class_function_indices);
        ok = 0;
    }
    if (!value_ssa_program_machine_call_clobber_risk_report_get_function_view_by_name(
            &report, "main", &function_index, &function_view) ||
        function_index != 0 || !function_view ||
        function_view->risk_report.item_count != 0 || function_view->risk_report.value_count != 5) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-call-clobber-risk function lookup mismatch\n");
        ok = 0;
    }
    if (value_ssa_machine_call_clobber_risk_report_find_value(&function_view->risk_report, 0, NULL, &item)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-call-clobber-risk unexpected risky value in main\n");
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_machine_call_clobber_risk_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_machine_protection_agenda(void) {
    ValueSsaProgram program;
    ValueSsaAllocationPrep prep;
    ValueSsaMachineProtectionAgenda agenda;
    const ValueSsaMachineProtectionAgendaItem *item = NULL;
    char *actual_text = NULL;
    ValueSsaError error;
    size_t call_live_value = (size_t)-1;
    size_t post_call_value = (size_t)-1;
    size_t value_count = 0;
    size_t item_count = 0;
    size_t crossing_count = 0;
    size_t repeated_count = 0;
    size_t hot_count = 0;
    int ok = 1;

    if (!build_alloc_call_density_prep_program(&program, &call_live_value, &post_call_value, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: protection-agenda setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_prep_init(&prep);
    value_ssa_machine_protection_agenda_init(&agenda);
    if (!value_ssa_compute_allocation_prep(&program.functions[1], NULL, NULL, NULL, NULL, &prep, &error) ||
        !value_ssa_compute_function_flat_machine_protection_agenda(
            &program.functions[1], 1, 1, &prep, &agenda, &error) ||
        !value_ssa_dump_machine_protection_agenda(&agenda, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: protection-agenda build failed: %s\n", error.message);
        free(actual_text);
        value_ssa_machine_protection_agenda_free(&agenda);
        value_ssa_allocation_prep_free(&prep);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text,
            "machine-protection-agenda values=3 items=1 crossing=0 repeated=1 hot=0\n"
            "ssa.0 reg=r0 class=repeated priority=114 call_cross=1 calls=1 use_calls=1 loops=0 use_loops=0 xblock=0 remat=1\n") != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: protection-agenda dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }
    if (!value_ssa_machine_protection_agenda_get_summary(
            &agenda, &value_count, &item_count, &crossing_count, &repeated_count, &hot_count) ||
        value_count != 3 || item_count != 1 || crossing_count != 0 || repeated_count != 1 || hot_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: protection-agenda summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_machine_protection_agenda_find_value(&agenda, call_live_value, NULL, &item) ||
        !item || item->risk_class != VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED ||
        item->protection_priority != 114 || item->single_block_live_range != 0 || item->rematerializable != 1) {
        fprintf(stderr, "[value-ssa-machine] FAIL: protection-agenda item lookup mismatch\n");
        ok = 0;
    }
    if (value_ssa_machine_protection_agenda_find_value(&agenda, post_call_value, NULL, &item)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: protection-agenda should not include non-crossing value\n");
        ok = 0;
    }

    free(actual_text);
    value_ssa_machine_protection_agenda_free(&agenda);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_program_machine_protection_report(void) {
    ValueSsaProgram program;
    ValueSsaProgramMachineProtectionReport report;
    const ValueSsaFunctionMachineProtectionView *function_view = NULL;
    const size_t *function_indices = NULL;
    char *actual_text = NULL;
    ValueSsaError error;
    size_t function_count = 0;
    size_t total_value_count = 0;
    size_t total_item_count = 0;
    size_t filtered_count = 0;
    size_t function_index = (size_t)-1;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-protection setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_machine_protection_report_init(&report);
    if (!value_ssa_compute_program_flat_machine_protection_report(&program, 2, 2, &report, &error) ||
        !value_ssa_dump_program_machine_protection_report(&report, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-protection build failed: %s\n", error.message);
        free(actual_text);
        value_ssa_program_machine_protection_report_free(&report);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text,
            "program-machine-protection-report functions=2 values=10 items=0 functions_with_items=0 crossing=0 repeated=0 hot=0\n"
            "function main items=0 values=5 crossing=0 repeated=0 hot=0\n"
            "machine-protection-agenda values=5 items=0 crossing=0 repeated=0 hot=0\n"
            "function spill items=0 values=5 crossing=0 repeated=0 hot=0\n"
            "machine-protection-agenda values=5 items=0 crossing=0 repeated=0 hot=0\n") != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: program-protection dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }
    if (!value_ssa_program_machine_protection_report_get_summary(
            &report, &function_count, &total_value_count, &total_item_count) ||
        function_count != 2 || total_value_count != 10 || total_item_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-protection summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_protection_report_get_functions_with_items(&report, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-protection item filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_protection_report_get_functions_with_risk_class(
            &report, VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-protection class filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_protection_report_get_function_view_by_name(
            &report, "main", &function_index, &function_view) ||
        function_index != 0 || !function_view || function_view->agenda.item_count != 0 || function_view->agenda.value_count != 5) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-protection function lookup mismatch\n");
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_machine_protection_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_machine_register_protection_pressure_view(void) {
    ValueSsaProgram program;
    ValueSsaAllocationPrep prep;
    ValueSsaMachineRegisterProtectionPressureView view;
    const ValueSsaMachineRegisterProtectionPressureItem *item = NULL;
    char *actual_text = NULL;
    ValueSsaError error;
    size_t call_live_value = (size_t)-1;
    size_t post_call_value = (size_t)-1;
    size_t machine_register_count = 0;
    size_t pressured_register_count = 0;
    size_t total_value_count = 0;
    size_t total_item_count = 0;
    size_t total_priority = 0;
    size_t crossing_count = 0;
    size_t repeated_count = 0;
    size_t hot_count = 0;
    int ok = 1;

    if (!build_alloc_call_density_prep_program(&program, &call_live_value, &post_call_value, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: register-pressure setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_prep_init(&prep);
    value_ssa_machine_register_protection_pressure_view_init(&view);
    if (!value_ssa_compute_allocation_prep(&program.functions[1], NULL, NULL, NULL, NULL, &prep, &error) ||
        !value_ssa_compute_function_flat_machine_register_protection_pressure_view(
            &program.functions[1], 1, 1, &prep, &view, &error) ||
        !value_ssa_dump_machine_register_protection_pressure_view(&view, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: register-pressure build failed: %s\n", error.message);
        free(actual_text);
        value_ssa_machine_register_protection_pressure_view_free(&view);
        value_ssa_allocation_prep_free(&prep);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text,
            "machine-register-protection-pressure regs=1 pressured=1 values=3 items=1 total_priority=114 crossing=0 repeated=1 hot=0\n"
            "reg r0 items=1 total_priority=114 max_priority=114 crossing=0 repeated=1 hot=0 values: ssa.0\n") != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: register-pressure dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }
    if (!value_ssa_machine_register_protection_pressure_view_get_summary(
            &view,
            &machine_register_count,
            &pressured_register_count,
            &total_value_count,
            &total_item_count,
            &total_priority,
            &crossing_count,
            &repeated_count,
            &hot_count) ||
        machine_register_count != 1 || pressured_register_count != 1 || total_value_count != 3 ||
        total_item_count != 1 || total_priority != 114 || crossing_count != 0 || repeated_count != 1 || hot_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: register-pressure summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_machine_register_protection_pressure_view_get_register_item(&view, 0, &item) ||
        !item || !item->machine_register_name || strcmp(item->machine_register_name, "r0") != 0 ||
        item->item_count != 1 || item->total_protection_priority != 114 ||
        item->max_protection_priority != 114 || !item->value_ids || item->value_ids[0] != call_live_value) {
        fprintf(stderr, "[value-ssa-machine] FAIL: register-pressure item lookup mismatch\n");
        ok = 0;
    }

    free(actual_text);
    value_ssa_machine_register_protection_pressure_view_free(&view);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_machine_register_protection_pressure_view_preserves_public_register_ids(void) {
    ValueSsaProgram program;
    ValueSsaAllocationPrep prep;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaFunctionMachineAllocationView machine_view;
    ValueSsaMachineRegisterProtectionPressureView view;
    ValueSsaMachineRegisterBank bank;
    const ValueSsaMachineRegisterProtectionPressureItem *item = NULL;
    ValueSsaError error;
    size_t call_live_value = (size_t)-1;
    int ok = 1;

    if (!build_alloc_call_density_pressure_program(&program, &call_live_value, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-pressure setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_prep_init(&prep);
    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_function_machine_allocation_view_init(&machine_view);
    value_ssa_machine_register_protection_pressure_view_init(&view);
    value_ssa_machine_register_bank_init(&bank);
    if (!build_non_identity_machine_register_bank(&bank) ||
        !value_ssa_compute_allocation_prep(&program.functions[1], NULL, NULL, NULL, NULL, &prep, &error) ||
        !prepare_manual_call_pressure_result(&result, &program) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, &error) ||
        !value_ssa_program_allocation_layout_attach_program_context(&layout, &program, &error) ||
        !value_ssa_build_function_machine_allocation_view(&layout.function_layouts[1], &bank, &machine_view, &error) ||
        !value_ssa_build_machine_register_protection_pressure_view(&machine_view, &prep, &view, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-pressure build failed: %s\n", error.message);
        value_ssa_machine_register_protection_pressure_view_free(&view);
        value_ssa_function_machine_allocation_view_free(&machine_view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_allocation_prep_free(&prep);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_machine_register_protection_pressure_view_get_register_item(&view, 10, &item) ||
        !item || item->machine_register_id != 10 || !item->machine_register_name ||
        strcmp(item->machine_register_name, "a10") != 0 ||
        item->item_count != 1 || !item->value_ids || item->value_ids[0] != call_live_value) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-pressure public register-id lookup mismatch\n");
        ok = 0;
    }

    value_ssa_machine_register_protection_pressure_view_free(&view);
    value_ssa_function_machine_allocation_view_free(&machine_view);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_program_machine_register_protection_pressure_report(void) {
    ValueSsaProgram program;
    ValueSsaProgramMachineRegisterProtectionPressureReport report;
    const ValueSsaFunctionMachineRegisterProtectionPressureView *function_view = NULL;
    const size_t *function_indices = NULL;
    char *actual_text = NULL;
    ValueSsaError error;
    size_t function_count = 0;
    size_t total_value_count = 0;
    size_t total_item_count = 0;
    size_t total_priority = 0;
    size_t total_pressured_register_count = 0;
    size_t filtered_count = 0;
    size_t function_index = (size_t)-1;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-register-pressure setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_machine_register_protection_pressure_report_init(&report);
    if (!value_ssa_compute_program_flat_machine_register_protection_pressure_report(&program, 2, 2, &report, &error) ||
        !value_ssa_dump_program_machine_register_protection_pressure_report(&report, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-register-pressure build failed: %s\n", error.message);
        free(actual_text);
        value_ssa_program_machine_register_protection_pressure_report_free(&report);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text,
            "program-machine-register-protection-pressure functions=2 values=10 items=0 total_priority=0 pressured_regs=0 pressure_funcs=0 crossing=0 repeated=0 hot=0\n"
            "function main pressured=0 values=5 items=0 total_priority=0 crossing=0 repeated=0 hot=0\n"
            "machine-register-protection-pressure regs=2 pressured=0 values=5 items=0 total_priority=0 crossing=0 repeated=0 hot=0\n"
            "function spill pressured=0 values=5 items=0 total_priority=0 crossing=0 repeated=0 hot=0\n"
            "machine-register-protection-pressure regs=2 pressured=0 values=5 items=0 total_priority=0 crossing=0 repeated=0 hot=0\n") != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: program-register-pressure dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }
    if (!value_ssa_program_machine_register_protection_pressure_report_get_summary(
            &report,
            &function_count,
            &total_value_count,
            &total_item_count,
            &total_priority,
            &total_pressured_register_count) ||
        function_count != 2 || total_value_count != 10 || total_item_count != 0 ||
        total_priority != 0 || total_pressured_register_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-register-pressure summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_register_protection_pressure_report_get_functions_with_pressure(
            &report, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-register-pressure filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_register_protection_pressure_report_get_functions_with_risk_class(
            &report, VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-register-pressure class filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_register_protection_pressure_report_get_function_view_by_name(
            &report, "main", &function_index, &function_view) ||
        function_index != 0 || !function_view ||
        function_view->pressure_view.pressured_register_count != 0 || function_view->pressure_view.total_value_count != 5) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-register-pressure function lookup mismatch\n");
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_machine_register_protection_pressure_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_function_machine_register_protection_hotspot_view(void) {
    ValueSsaProgram program;
    ValueSsaAllocationPrep prep;
    ValueSsaFunctionMachineRegisterProtectionHotspotView view;
    const ValueSsaMachineRegisterProtectionHotspotItem *hotspot = NULL;
    const char *function_name = NULL;
    char *actual_text = NULL;
    ValueSsaError error;
    size_t call_live_value = (size_t)-1;
    size_t post_call_value = (size_t)-1;
    size_t total_item_count = 0;
    size_t total_value_count = 0;
    int has_hotspot = 0;
    int ok = 1;

    if (!build_alloc_call_density_prep_program(&program, &call_live_value, &post_call_value, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: hotspot-view setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_prep_init(&prep);
    value_ssa_function_machine_register_protection_hotspot_view_init(&view);
    if (!value_ssa_compute_allocation_prep(&program.functions[1], NULL, NULL, NULL, NULL, &prep, &error) ||
        !value_ssa_compute_function_flat_machine_register_protection_hotspot_view(
            &program.functions[1], 1, 1, &prep, &view, &error) ||
        !value_ssa_dump_function_machine_register_protection_hotspot_view(&view, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: hotspot-view build failed: %s\n", error.message);
        free(actual_text);
        value_ssa_function_machine_register_protection_hotspot_view_free(&view);
        value_ssa_allocation_prep_free(&prep);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text,
            "function-register-protection-hotspot call_density reg=r0 items=1 total_priority=114 max_priority=114 crossing=0 repeated=1 hot=0\n") != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: hotspot-view dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }
    if (!value_ssa_function_machine_register_protection_hotspot_view_get_summary(
            &view, &function_name, &has_hotspot, &total_item_count, &total_value_count, &hotspot) ||
        !function_name || strcmp(function_name, "call_density") != 0 ||
        !has_hotspot || total_item_count != 1 || total_value_count != 1 ||
        !hotspot || !hotspot->machine_register_name || strcmp(hotspot->machine_register_name, "r0") != 0 ||
        hotspot->total_protection_priority != 114 || hotspot->item_count != 1) {
        fprintf(stderr, "[value-ssa-machine] FAIL: hotspot-view summary mismatch\n");
        ok = 0;
    }

    free(actual_text);
    value_ssa_function_machine_register_protection_hotspot_view_free(&view);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_program_machine_register_protection_hotspot_report(void) {
    ValueSsaProgram program;
    ValueSsaProgramMachineRegisterProtectionHotspotReport report;
    const ValueSsaFunctionMachineRegisterProtectionHotspotView *function_view = NULL;
    const size_t *function_indices = NULL;
    char *actual_text = NULL;
    ValueSsaError error;
    size_t function_count = 0;
    size_t total_value_count = 0;
    size_t total_item_count = 0;
    size_t total_hotspot_priority = 0;
    size_t total_hotspot_item_count = 0;
    size_t filtered_count = 0;
    size_t function_index = (size_t)-1;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-hotspot setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_machine_register_protection_hotspot_report_init(&report);
    if (!value_ssa_compute_program_flat_machine_register_protection_hotspot_report(&program, 2, 2, &report, &error) ||
        !value_ssa_dump_program_machine_register_protection_hotspot_report(&report, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-hotspot build failed: %s\n", error.message);
        free(actual_text);
        value_ssa_program_machine_register_protection_hotspot_report_free(&report);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text,
            "program-machine-register-protection-hotspot functions=2 values=10 items=0 hotspot_priority=0 hotspot_items=0 hotspot_funcs=0\n"
            "function-register-protection-hotspot main hotspot=none\n"
            "function-register-protection-hotspot spill hotspot=none\n") != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: program-hotspot dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }
    if (!value_ssa_program_machine_register_protection_hotspot_report_get_summary(
            &report,
            &function_count,
            &total_value_count,
            &total_item_count,
            &total_hotspot_priority,
            &total_hotspot_item_count) ||
        function_count != 2 || total_value_count != 10 || total_item_count != 0 ||
        total_hotspot_priority != 0 || total_hotspot_item_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-hotspot summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_register_protection_hotspot_report_get_functions_with_hotspots(
            &report, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-hotspot filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_register_protection_hotspot_report_get_function_view_by_name(
            &report, "main", &function_index, &function_view) ||
        function_index != 0 || !function_view || function_view->has_hotspot != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-hotspot function lookup mismatch\n");
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_machine_register_protection_hotspot_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_function_machine_preservation_hint_view(void) {
    ValueSsaProgram program;
    ValueSsaAllocationPrep prep;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaFunctionMachineAllocationView machine_view;
    ValueSsaMachineRegisterProtectionPressureView pressure_view;
    ValueSsaMachineRegisterBank bank;
    ValueSsaMachineRegisterDesc *grown_registers = NULL;
    ValueSsaFunctionMachinePreservationHintView view;
    const ValueSsaMachinePreservationHintCandidate *candidate = NULL;
    const char *function_name = NULL;
    char *actual_text = NULL;
    ValueSsaError error;
    size_t call_live_value = (size_t)-1;
    size_t protected_item_count = 0;
    size_t total_value_count = 0;
    size_t total_hint_priority = 0;
    int has_hint = 0;
    int ok = 1;

    if (!build_alloc_call_density_pressure_program(&program, &call_live_value, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation-hint setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_prep_init(&prep);
    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_function_machine_allocation_view_init(&machine_view);
    value_ssa_machine_register_protection_pressure_view_init(&pressure_view);
    value_ssa_machine_register_bank_init(&bank);
    value_ssa_function_machine_preservation_hint_view_init(&view);
    if (!build_caller_callee_machine_register_bank(&bank)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation-hint bank setup failed\n");
        value_ssa_function_machine_preservation_hint_view_free(&view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
        value_ssa_function_machine_allocation_view_free(&machine_view);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_allocation_prep_free(&prep);
        value_ssa_program_free(&program);
        return 0;
    }
    grown_registers = (ValueSsaMachineRegisterDesc *)realloc(bank.registers, 3 * sizeof(ValueSsaMachineRegisterDesc));
    if (!grown_registers) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation-hint bank growth failed\n");
        value_ssa_function_machine_preservation_hint_view_free(&view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
        value_ssa_function_machine_allocation_view_free(&machine_view);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_allocation_prep_free(&prep);
        value_ssa_program_free(&program);
        return 0;
    }
    bank.registers = grown_registers;
    bank.register_count = 3;
    bank.registers[2].register_id = 2;
    bank.registers[2].name = "r2";
    bank.registers[2].register_class = VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL;
    bank.registers[2].allocatable = 1u;
    bank.registers[2].caller_clobbered = 0u;
    bank.registers[2].callee_preserved = 1u;

    if (!value_ssa_compute_allocation_prep(&program.functions[1], NULL, NULL, NULL, NULL, &prep, &error) ||
        !prepare_manual_call_pressure_result(&result, &program) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, &error) ||
        !value_ssa_program_allocation_layout_attach_program_context(&layout, &program, &error) ||
        !value_ssa_build_function_machine_allocation_view(&layout.function_layouts[1], &bank, &machine_view, &error) ||
        !value_ssa_build_machine_register_protection_pressure_view(&machine_view, &prep, &pressure_view, &error) ||
        !value_ssa_build_function_machine_preservation_hint_view(
            "call_pressure", &machine_view, &pressure_view, &bank, &view, &error) ||
        !value_ssa_dump_function_machine_preservation_hint_view(&view, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation-hint build failed: %s\n", error.message);
        free(actual_text);
        value_ssa_function_machine_preservation_hint_view_free(&view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
        value_ssa_function_machine_allocation_view_free(&machine_view);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_allocation_prep_free(&prep);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text,
            "function-machine-preservation-hint call_pressure move=r0->r2 candidates=2 items=1 total_priority=114 max_priority=114 crossing=0 repeated=1 hot=0\n"
            "candidate 0 move=r0->r2 reason=target-less-occupied target_items=1 target_priority=0 target_max=0 items=1 total_priority=114 max_priority=114\n"
            "candidate 1 move=r0->r1 reason=target-less-occupied target_items=3 target_priority=0 target_max=0 items=1 total_priority=114 max_priority=114\n") != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: preservation-hint dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }
    if (!value_ssa_function_machine_preservation_hint_view_get_summary(
            &view,
            &function_name,
            &has_hint,
            &protected_item_count,
            &total_value_count,
            &total_hint_priority) ||
        !function_name || strcmp(function_name, "call_pressure") != 0 ||
        !has_hint || protected_item_count != 1 || total_value_count != 1 || total_hint_priority != 114 ||
        !view.source_machine_register_name || strcmp(view.source_machine_register_name, "r0") != 0 ||
        !view.suggested_machine_register_name || strcmp(view.suggested_machine_register_name, "r2") != 0 ||
        view.candidate_count != 2) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation-hint summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_function_machine_preservation_hint_view_get_candidate_at(&view, 1, &candidate) ||
        !candidate || !candidate->suggested_machine_register_name ||
        strcmp(candidate->suggested_machine_register_name, "r1") != 0 ||
        candidate->protected_total_priority != 114 ||
        !candidate->primary_reason || strcmp(candidate->primary_reason, "target-less-occupied") != 0 ||
        candidate->suggested_register_item_count != 3 ||
        candidate->suggested_register_total_priority != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation-hint candidate lookup mismatch\n");
        ok = 0;
    }

    free(actual_text);
    value_ssa_function_machine_preservation_hint_view_free(&view);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
    value_ssa_function_machine_allocation_view_free(&machine_view);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_function_machine_preservation_hint_view_preserves_public_register_ids(void) {
    ValueSsaProgram program;
    ValueSsaAllocationPrep prep;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaFunctionMachineAllocationView machine_view;
    ValueSsaMachineRegisterProtectionPressureView pressure_view;
    ValueSsaMachineRegisterBank bank;
    ValueSsaMachineRegisterDesc *grown_registers = NULL;
    ValueSsaFunctionMachinePreservationHintView view;
    const ValueSsaMachinePreservationHintCandidate *candidate = NULL;
    ValueSsaError error;
    size_t call_live_value = (size_t)-1;
    int ok = 1;

    if (!build_alloc_call_density_pressure_program(&program, &call_live_value, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-preservation setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_prep_init(&prep);
    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_function_machine_allocation_view_init(&machine_view);
    value_ssa_machine_register_protection_pressure_view_init(&pressure_view);
    value_ssa_machine_register_bank_init(&bank);
    value_ssa_function_machine_preservation_hint_view_init(&view);
    if (!build_non_identity_machine_register_bank(&bank)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-preservation bank setup failed\n");
        value_ssa_function_machine_preservation_hint_view_free(&view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
        value_ssa_function_machine_allocation_view_free(&machine_view);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_allocation_prep_free(&prep);
        value_ssa_program_free(&program);
        return 0;
    }
    grown_registers = (ValueSsaMachineRegisterDesc *)realloc(bank.registers, 3 * sizeof(ValueSsaMachineRegisterDesc));
    if (!grown_registers) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-preservation bank growth failed\n");
        value_ssa_function_machine_preservation_hint_view_free(&view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
        value_ssa_function_machine_allocation_view_free(&machine_view);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_allocation_prep_free(&prep);
        value_ssa_program_free(&program);
        return 0;
    }
    bank.registers = grown_registers;
    bank.register_count = 3;
    bank.registers[2].register_id = 30;
    bank.registers[2].name = "p30";
    bank.registers[2].register_class = VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL;
    bank.registers[2].allocatable = 1u;
    bank.registers[2].caller_clobbered = 0u;
    bank.registers[2].callee_preserved = 1u;

    if (!value_ssa_compute_allocation_prep(&program.functions[1], NULL, NULL, NULL, NULL, &prep, &error) ||
        !prepare_manual_call_pressure_result(&result, &program) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, &error) ||
        !value_ssa_program_allocation_layout_attach_program_context(&layout, &program, &error) ||
        !value_ssa_build_function_machine_allocation_view(&layout.function_layouts[1], &bank, &machine_view, &error) ||
        !value_ssa_build_machine_register_protection_pressure_view(&machine_view, &prep, &pressure_view, &error) ||
        !value_ssa_build_function_machine_preservation_hint_view(
            "call_pressure", &machine_view, &pressure_view, &bank, &view, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-preservation build failed: %s\n", error.message);
        value_ssa_function_machine_preservation_hint_view_free(&view);
        value_ssa_machine_register_bank_free(&bank);
        value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
        value_ssa_function_machine_allocation_view_free(&machine_view);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_allocation_prep_free(&prep);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!view.has_hint ||
        view.source_machine_register_id != 10 ||
        !view.source_machine_register_name || strcmp(view.source_machine_register_name, "a10") != 0 ||
        view.suggested_machine_register_id != 30 ||
        !view.suggested_machine_register_name || strcmp(view.suggested_machine_register_name, "p30") != 0 ||
        !value_ssa_function_machine_preservation_hint_view_get_candidate_at(&view, 0, &candidate) ||
        !candidate || candidate->machine_register_id != 10 ||
        candidate->suggested_machine_register_id != 30) {
        fprintf(stderr, "[value-ssa-machine] FAIL: non-identity-preservation public register-id mismatch\n");
        ok = 0;
    }

    value_ssa_function_machine_preservation_hint_view_free(&view);
    value_ssa_machine_register_bank_free(&bank);
    value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
    value_ssa_function_machine_allocation_view_free(&machine_view);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_function_machine_preservation_hint_reason_respects_pressure_order(void) {
    ValueSsaFunctionMachineAllocationView machine_view;
    ValueSsaMachineRegisterProtectionPressureView pressure_view;
    ValueSsaMachineRegisterBank bank;
    ValueSsaFunctionMachinePreservationHintView view;
    const ValueSsaMachinePreservationHintCandidate *candidate = NULL;
    ValueSsaError error;
    int ok = 1;

    value_ssa_function_machine_allocation_view_init(&machine_view);
    value_ssa_machine_register_protection_pressure_view_init(&pressure_view);
    value_ssa_machine_register_bank_init(&bank);
    value_ssa_function_machine_preservation_hint_view_init(&view);
    memset(&error, 0, sizeof(error));

    bank.register_count = 2;
    bank.registers = (ValueSsaMachineRegisterDesc *)calloc(2, sizeof(ValueSsaMachineRegisterDesc));
    machine_view.value_count = 2;
    machine_view.machine_register_count = 2;
    machine_view.placements =
        (ValueSsaMachineAllocatedValuePlacement *)calloc(2, sizeof(ValueSsaMachineAllocatedValuePlacement));
    machine_view.used_machine_register_count = 2;
    machine_view.machine_register_group_count = 2;
    machine_view.used_machine_register_ids = (size_t *)malloc(2 * sizeof(size_t));
    machine_view.machine_register_groups =
        (ValueSsaMachineRegisterValueGroup *)calloc(2, sizeof(ValueSsaMachineRegisterValueGroup));
    pressure_view.machine_register_count = 2;
    pressure_view.pressured_register_count = 2;
    pressure_view.items =
        (ValueSsaMachineRegisterProtectionPressureItem *)calloc(2, sizeof(ValueSsaMachineRegisterProtectionPressureItem));
    if (!bank.registers || !machine_view.placements || !machine_view.used_machine_register_ids ||
        !machine_view.machine_register_groups || !pressure_view.items) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation-reason manual setup failed\n");
        ok = 0;
        goto cleanup;
    }

    bank.registers[0].register_id = 10;
    bank.registers[0].name = "src";
    bank.registers[0].register_class = VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL;
    bank.registers[0].allocatable = 1u;
    bank.registers[0].caller_clobbered = 1u;
    bank.registers[0].callee_preserved = 0u;
    bank.registers[1].register_id = 20;
    bank.registers[1].name = "dst";
    bank.registers[1].register_class = VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL;
    bank.registers[1].allocatable = 1u;
    bank.registers[1].caller_clobbered = 0u;
    bank.registers[1].callee_preserved = 1u;

    machine_view.used_machine_register_ids[0] = 10;
    machine_view.used_machine_register_ids[1] = 20;
    machine_view.machine_register_groups[0].machine_register_id = 10;
    machine_view.machine_register_groups[0].machine_register_name = "src";
    machine_view.machine_register_groups[0].value_count = 1;
    machine_view.machine_register_groups[0].value_ids = (size_t *)malloc(sizeof(size_t));
    machine_view.machine_register_groups[1].machine_register_id = 20;
    machine_view.machine_register_groups[1].machine_register_name = "dst";
    machine_view.machine_register_groups[1].value_count = 0;
    machine_view.machine_register_groups[1].value_ids = NULL;
    if (!machine_view.machine_register_groups[0].value_ids) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation-reason group setup failed\n");
        ok = 0;
        goto cleanup;
    }
    machine_view.machine_register_groups[0].value_ids[0] = 0;

    pressure_view.items[0].machine_register_id = 10;
    pressure_view.items[0].machine_register_name = "src";
    pressure_view.items[0].item_count = 2;
    pressure_view.items[0].total_protection_priority = 100;
    pressure_view.items[0].max_protection_priority = 60;
    pressure_view.items[0].risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED] = 2;
    pressure_view.items[0].value_ids = (size_t *)malloc(2 * sizeof(size_t));
    pressure_view.items[1].machine_register_id = 20;
    pressure_view.items[1].machine_register_name = "dst";
    pressure_view.items[1].item_count = 1;
    pressure_view.items[1].total_protection_priority = 100;
    pressure_view.items[1].max_protection_priority = 100;
    pressure_view.items[1].risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED] = 1;
    pressure_view.items[1].value_ids = (size_t *)malloc(sizeof(size_t));
    if (!pressure_view.items[0].value_ids || !pressure_view.items[1].value_ids) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation-reason pressure setup failed\n");
        ok = 0;
        goto cleanup;
    }
    pressure_view.items[0].value_ids[0] = 0;
    pressure_view.items[0].value_ids[1] = 1;
    pressure_view.items[1].value_ids[0] = 1;
    pressure_view.total_item_count = 3;
    pressure_view.total_protection_priority = 200;
    pressure_view.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED] = 3;

    if (!value_ssa_build_function_machine_preservation_hint_view(
            "manual", &machine_view, &pressure_view, &bank, &view, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation-reason build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }
    if (!view.has_hint ||
        view.primary_reason != VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED ||
        !value_ssa_function_machine_preservation_hint_view_get_candidate_at(&view, 0, &candidate) ||
        !candidate || !candidate->primary_reason ||
        strcmp(candidate->primary_reason, "target-less-occupied") != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: preservation-reason classification mismatch\n");
        ok = 0;
    }

cleanup:
    value_ssa_function_machine_preservation_hint_view_free(&view);
    value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
    value_ssa_function_machine_allocation_view_free(&machine_view);
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

static int test_value_ssa_program_machine_preservation_hint_report(void) {
    ValueSsaProgram program;
    ValueSsaProgramMachinePreservationHintReport report;
    const ValueSsaFunctionMachinePreservationHintView *function_view = NULL;
    const size_t *function_indices = NULL;
    char *actual_text = NULL;
    ValueSsaError error;
    size_t function_count = 0;
    size_t total_value_count = 0;
    size_t total_item_count = 0;
    size_t total_hint_priority = 0;
    size_t reason_count = 0;
    size_t filtered_count = 0;
    size_t function_index = (size_t)-1;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-preservation-hint setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_machine_preservation_hint_report_init(&report);
    if (!value_ssa_compute_program_flat_machine_preservation_hint_report(&program, 2, 2, &report, &error) ||
        !value_ssa_dump_program_machine_preservation_hint_report(&report, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-preservation-hint build failed: %s\n", error.message);
        free(actual_text);
        value_ssa_program_machine_preservation_hint_report_free(&report);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text,
            "program-machine-preservation-hint functions=2 values=10 items=0 hint_priority=0 hint_funcs=0\n"
            "function-machine-preservation-hint main hint=none\n"
            "function-machine-preservation-hint spill hint=none\n") != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: program-preservation-hint dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }
    if (!value_ssa_program_machine_preservation_hint_report_get_summary(
            &report,
            &function_count,
            &total_value_count,
            &total_item_count,
            &total_hint_priority) ||
        function_count != 2 || total_value_count != 10 || total_item_count != 0 || total_hint_priority != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-preservation-hint summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_preservation_hint_report_get_functions_with_hints(
            &report, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-preservation-hint filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_preservation_hint_report_get_reason_count(
            &report, VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED, &reason_count) ||
        reason_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-preservation-hint reason summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_preservation_hint_report_get_functions_with_reason(
            &report, VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-preservation-hint reason filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_preservation_hint_report_get_function_view_by_name(
            &report, "main", &function_index, &function_view) ||
        function_index != 0 || !function_view || function_view->has_hint != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: program-preservation-hint function lookup mismatch\n");
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_machine_preservation_hint_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_program_machine_planning_report(void) {
    ValueSsaProgram program;
    ValueSsaProgramMachinePlanningReport report;
    const ValueSsaFunctionMachinePlanningView *function_view = NULL;
    const size_t *function_indices = NULL;
    char *actual_text = NULL;
    ValueSsaError error;
    size_t function_count = 0;
    size_t total_value_count = 0;
    size_t total_pressure_item_count = 0;
    size_t total_pressure_priority = 0;
    size_t total_hotspot_priority = 0;
    size_t total_hint_priority = 0;
    size_t reason_count = 0;
    size_t filtered_count = 0;
    size_t function_index = (size_t)-1;
    const char *function_name = NULL;
    size_t pressure_item_count = 0;
    size_t pressure_priority = 0;
    int has_hotspot = 0;
    int has_hint = 0;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning-report setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_machine_planning_report_init(&report);
    if (!value_ssa_compute_program_flat_machine_planning_report(&program, 2, 2, &report, &error) ||
        !value_ssa_dump_program_machine_planning_report(&report, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning-report build failed: %s\n", error.message);
        free(actual_text);
        value_ssa_program_machine_planning_report_free(&report);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strcmp(actual_text,
            "program-machine-planning functions=2 values=10 pressure_items=0 pressure_priority=0 hotspot_priority=0 hint_priority=0 pressure_funcs=0 hotspot_funcs=0 hint_funcs=0\n"
            "function-machine-planning main\n"
            "machine-register-protection-pressure regs=2 pressured=0 values=5 items=0 total_priority=0 crossing=0 repeated=0 hot=0\n"
            "function-register-protection-hotspot main hotspot=none\n"
            "function-machine-preservation-hint main hint=none\n"
            "function-machine-planning spill\n"
            "machine-register-protection-pressure regs=2 pressured=0 values=5 items=0 total_priority=0 crossing=0 repeated=0 hot=0\n"
            "function-register-protection-hotspot spill hotspot=none\n"
            "function-machine-preservation-hint spill hint=none\n") != 0) {
        fprintf(stderr,
            "[value-ssa-machine] FAIL: planning-report dump mismatch\nactual:\n%s\n",
            actual_text);
        ok = 0;
    }
    if (!value_ssa_program_machine_planning_report_get_summary(
            &report,
            &function_count,
            &total_value_count,
            &total_pressure_item_count,
            &total_pressure_priority,
            &total_hotspot_priority,
            &total_hint_priority) ||
        function_count != 2 || total_value_count != 10 || total_pressure_item_count != 0 ||
        total_pressure_priority != 0 || total_hotspot_priority != 0 || total_hint_priority != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning-report summary mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_planning_report_get_functions_with_pressure(&report, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning-report pressure filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_planning_report_get_pressure_reason_count(
            &report, VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED, &reason_count) ||
        reason_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning-report pressure reason mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_planning_report_get_functions_with_pressure_reason(
            &report, VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning-report pressure reason filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_planning_report_get_functions_with_hotspots(&report, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning-report hotspot filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_planning_report_get_functions_with_hints(&report, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning-report hint filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_planning_report_get_hint_reason_count(
            &report, VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED, &reason_count) ||
        reason_count != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning-report hint reason mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_planning_report_get_functions_with_hint_reason(
            &report, VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED, &filtered_count, &function_indices) ||
        filtered_count != 0 || function_indices != NULL) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning-report hint reason filter mismatch\n");
        ok = 0;
    }
    if (!value_ssa_program_machine_planning_report_get_function_view_by_name(
            &report, "main", &function_index, &function_view) ||
        function_index != 0 || !function_view ||
        !value_ssa_function_machine_planning_view_get_summary(
            function_view,
            &function_name,
            &total_value_count,
            &pressure_item_count,
            &pressure_priority,
            &has_hotspot,
            &has_hint) ||
        !function_name || strcmp(function_name, "main") != 0 ||
        total_value_count != 5 || pressure_item_count != 0 || pressure_priority != 0 ||
        has_hotspot != 0 || has_hint != 0) {
        fprintf(stderr, "[value-ssa-machine] FAIL: planning-report function summary mismatch\n");
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_machine_planning_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_value_ssa_build_flat_machine_register_bank();
    ok &= test_value_ssa_build_function_machine_allocation_view();
    ok &= test_value_ssa_build_program_machine_allocation_view();
    ok &= test_value_ssa_machine_register_bank_query_surface();
    ok &= test_value_ssa_program_machine_allocation_view_query_surface();
    ok &= test_value_ssa_program_machine_allocation_view_preserves_public_register_ids();
    ok &= test_value_ssa_allocate_rewrite_machine_report_artifact_dump();
    ok &= test_value_ssa_machine_dump_rejects_malformed_bank();
    ok &= test_value_ssa_machine_dump_rejects_malformed_function_view();
    ok &= test_value_ssa_machine_dump_rejects_malformed_program_view();
    ok &= test_value_ssa_machine_dump_rejects_malformed_call_clobber_report();
    ok &= test_value_ssa_machine_dump_rejects_malformed_protection_report();
    ok &= test_value_ssa_machine_summary_queries_clear_outputs_on_failure();
    ok &= test_value_ssa_allocate_and_rewrite_machine_report_entrypoints();
    ok &= test_value_ssa_machine_view_entrypoints();
    ok &= test_value_ssa_machine_call_clobber_risk_report();
    ok &= test_value_ssa_program_machine_call_clobber_risk_report();
    ok &= test_value_ssa_machine_protection_agenda();
    ok &= test_value_ssa_program_machine_protection_report();
    ok &= test_value_ssa_machine_register_protection_pressure_view();
    ok &= test_value_ssa_machine_register_protection_pressure_view_preserves_public_register_ids();
    ok &= test_value_ssa_program_machine_register_protection_pressure_report();
    ok &= test_value_ssa_function_machine_register_protection_hotspot_view();
    ok &= test_value_ssa_program_machine_register_protection_hotspot_report();
    ok &= test_value_ssa_function_machine_preservation_hint_view();
    ok &= test_value_ssa_function_machine_preservation_hint_view_preserves_public_register_ids();
    ok &= test_value_ssa_function_machine_preservation_hint_reason_respects_pressure_order();
    ok &= test_value_ssa_program_machine_preservation_hint_report();
    ok &= test_value_ssa_program_machine_planning_report();
    if (!ok) {
        return 1;
    }

    printf("[value-ssa-machine] PASS\n");
    return 0;
}
