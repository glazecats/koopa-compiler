#include "value_ssa.h"
#include "value_ssa_interp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int build_straight_line_program(ValueSsaProgram *program, ValueSsaError *error) {
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
    instruction.as.store.slot = value_ssa_slot_global(global->id);
    instruction.as.store.value = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_phi_program(ValueSsaProgram *program, ValueSsaError *error) {
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

static int build_internal_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *callee = NULL;
    ValueSsaFunction *main_fn = NULL;
    ValueSsaBasicBlock *callee_block = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    size_t callee_local_id;
    size_t callee_value0;
    size_t callee_value1;
    size_t main_value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "inc", 1, &callee, error) ||
        !value_ssa_function_append_local(callee, "x", 1, &callee_local_id, error) ||
        !value_ssa_function_append_block(callee, NULL, &callee_block, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_fn, error) ||
        !value_ssa_function_append_block(main_fn, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    callee_value0 = value_ssa_function_allocate_value(callee);
    callee_value1 = value_ssa_function_allocate_value(callee);
    main_value0 = value_ssa_function_allocate_value(main_fn);
    if (callee_value0 == (size_t)-1 || callee_value1 == (size_t)-1 || main_value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(callee_value0);
    instruction.as.load_slot = value_ssa_slot_local(callee_local_id);
    if (!value_ssa_block_append_instruction(callee_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(callee_value1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(callee_value0);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(callee_block, &instruction, error) ||
        !value_ssa_block_set_return(callee_block, value_ssa_value_id(callee_value1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(main_value0);
    instruction.as.call.callee_name = "inc";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (ValueSsaValueRef *)malloc(sizeof(ValueSsaValueRef));
    if (!instruction.as.call.args) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = value_ssa_value_immediate(41);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(main_value0), error)) {
        free(instruction.as.call.args);
        value_ssa_program_free(program);
        return 0;
    }
    free(instruction.as.call.args);

    return 1;
}

static int build_external_call_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *main_fn = NULL;
    ValueSsaBasicBlock *main_block = NULL;
    ValueSsaInstruction instruction;
    size_t main_value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "ext", 0, &decl, error) ||
        !value_ssa_program_append_function(program, "main", 1, &main_fn, error) ||
        !value_ssa_function_append_block(main_fn, NULL, &main_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    main_value0 = value_ssa_function_allocate_value(main_fn);
    if (main_value0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(main_value0);
    instruction.as.call.callee_name = "ext";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (ValueSsaValueRef *)malloc(sizeof(ValueSsaValueRef));
    if (!instruction.as.call.args) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.as.call.args[0] = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(main_block, &instruction, error) ||
        !value_ssa_block_set_return(main_block, value_ssa_value_id(main_value0), error)) {
        free(instruction.as.call.args);
        value_ssa_program_free(program);
        return 0;
    }
    free(instruction.as.call.args);

    return 1;
}

static int build_invalid_uninitialized_local_load_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t local_a_id;
    size_t value0;

    value_ssa_program_init(program);

    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_local(function, "a", 0, &local_a_id, error) ||
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
    instruction.as.load_slot = value_ssa_slot_local(local_a_id);
    if (!value_ssa_block_append_instruction(block, &instruction, error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int external_double(const ValueSsaProgram *program,
    const char *callee_name,
    const long long *args,
    size_t arg_count,
    int has_result,
    long long *out_result,
    long long *global_values,
    size_t global_count,
    void *user_data,
    ValueSsaInterpError *error) {
    (void)program;
    (void)global_values;
    (void)global_count;
    (void)user_data;
    (void)error;

    if (!callee_name || strcmp(callee_name, "ext") != 0 || arg_count != 1 || !has_result || !out_result) {
        return 0;
    }

    *out_result = args[0] * 2;
    return 1;
}

static int test_value_ssa_interp_executes_straight_line_program(void) {
    ValueSsaProgram program;
    ValueSsaError build_error;
    ValueSsaInterpResult result;
    ValueSsaInterpError error;
    long long args[] = {5};
    int ok;

    if (!build_straight_line_program(&program, &build_error)) {
        return 0;
    }

    value_ssa_interp_result_init(&result);
    ok = value_ssa_interp_execute_main(&program, args, 1, NULL, &result, &error) &&
        result.return_value == 6 &&
        result.global_count == 1 &&
        result.global_values[0] == 6;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-interp] FAIL: straight-line execution failed: %s\n",
            error.message);
    }

    value_ssa_interp_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_interp_executes_phi_program(void) {
    ValueSsaProgram program;
    ValueSsaError build_error;
    ValueSsaInterpResult result;
    ValueSsaInterpError error;
    long long args[] = {0};
    int ok;

    if (!build_phi_program(&program, &build_error)) {
        return 0;
    }

    value_ssa_interp_result_init(&result);
    ok = value_ssa_interp_execute_main(&program, args, 1, NULL, &result, &error) && result.return_value == 20;
    if (!ok) {
        fprintf(stderr, "[value-ssa-interp] FAIL: phi execution failed: %s\n", error.message);
    }

    value_ssa_interp_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_interp_executes_internal_call_program(void) {
    ValueSsaProgram program;
    ValueSsaError build_error;
    ValueSsaInterpResult result;
    ValueSsaInterpError error;
    int ok;

    if (!build_internal_call_program(&program, &build_error)) {
        return 0;
    }

    value_ssa_interp_result_init(&result);
    ok = value_ssa_interp_execute_main(&program, NULL, 0, NULL, &result, &error) && result.return_value == 42;
    if (!ok) {
        fprintf(stderr, "[value-ssa-interp] FAIL: internal call execution failed: %s\n", error.message);
    }

    value_ssa_interp_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_interp_executes_external_call_program(void) {
    ValueSsaProgram program;
    ValueSsaError build_error;
    ValueSsaInterpResult result;
    ValueSsaInterpError error;
    ValueSsaInterpOptions options;
    int ok;

    if (!build_external_call_program(&program, &build_error)) {
        return 0;
    }

    value_ssa_interp_options_init(&options);
    options.extern_call = external_double;
    value_ssa_interp_result_init(&result);
    ok = value_ssa_interp_execute_main(&program, NULL, 0, &options, &result, &error) &&
        result.return_value == 18;
    if (!ok) {
        fprintf(stderr, "[value-ssa-interp] FAIL: external call execution failed: %s\n", error.message);
    }

    value_ssa_interp_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_interp_rejects_uninitialized_local_load(void) {
    ValueSsaProgram program;
    ValueSsaError build_error;
    ValueSsaInterpResult result;
    ValueSsaInterpError error;
    int ok;

    if (!build_invalid_uninitialized_local_load_program(&program, &build_error)) {
        return 0;
    }

    value_ssa_interp_result_init(&result);
    ok = !value_ssa_interp_execute_main(&program, NULL, 0, NULL, &result, &error) &&
        strstr(error.message, "VALUE-SSA-INTERP-022") != NULL;
    if (!ok) {
        fprintf(stderr,
            "[value-ssa-interp] FAIL: expected uninitialized local rejection, got: %s\n",
            error.message);
    }

    value_ssa_interp_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_value_ssa_interp_executes_straight_line_program();
    ok &= test_value_ssa_interp_executes_phi_program();
    ok &= test_value_ssa_interp_executes_internal_call_program();
    ok &= test_value_ssa_interp_executes_external_call_program();
    ok &= test_value_ssa_interp_rejects_uninitialized_local_load();
    if (!ok) {
        return 1;
    }

    printf("[value-ssa-interp] PASS\n");
    return 0;
}
