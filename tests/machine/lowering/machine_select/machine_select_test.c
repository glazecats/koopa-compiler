#include "machine/select.h"

#include "machine/ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_text(const char *text) {
    size_t length;
    char *copy;

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

static int expect_dump(const MachineSelectProgram *program, const char *expected_text) {
    char *actual_text = NULL;
    MachineSelectError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_select_dump_program(program, &actual_text, &error)) {
        fprintf(stderr, "[machine-select] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-select] FAIL: dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int expect_report_dump(const MachineSelectLowerReport *report, const char *expected_text) {
    char *actual_text = NULL;
    MachineSelectError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_select_dump_lower_report_artifact(report, &actual_text, &error)) {
        fprintf(stderr, "[machine-select] FAIL: report dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-select] FAIL: report dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int build_machine_ir_smoke_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_global(program, "g", NULL, error) ||
        !machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, error) ||
        !machine_ir_function_append_block(function, NULL, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(1), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_select_lower_machine_ir_smoke(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: machine-ir smoke setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    if (!machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: lower from machine-ir failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = alui.0 reg.0, 1\n"
        "    store_global global.0, reg.1\n"
        "    ret reg.1\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_rejects_phi_input(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineSelectProgram select_program;
    MachineIrError machine_error;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_block_append_phi(&function->blocks[0], machine_ir_operand_register(0), NULL, NULL, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: phi setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: lowering should reject phi-bearing machine_ir input\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_summary_surface(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    const MachineSelectFunction *function = NULL;
    const MachineSelectFunction *named_function = NULL;
    const MachineSelectBasicBlock *block = NULL;
    MachineSelectFunctionSummary summary;
    MachineSelectFunctionSummary named_summary;
    MachineSelectFunctionSummary artifact_summary;
    MachineSelectTerminatorKind terminator_kind = MACHINE_SELECT_TERM_JUMP;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t function_index = (size_t)-1;
    const char *name = NULL;
    size_t block_count = 0;
    size_t block_id = (size_t)-1;
    size_t op_count = 0;
    size_t spill_slot_count = 0;
    int has_terminator = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: summary setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_program_get_summary(&select_program, &register_count, &global_count, &function_count) ||
        register_count != 2 || global_count != 1 || function_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: program summary mismatch\n");
        ok = 0;
    }
    if (!machine_select_program_get_function_by_name(&select_program, "main", &function_index, &named_function) ||
        function_index != 0 || !named_function) {
        fprintf(stderr, "[machine-select] FAIL: program function-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_program_get_function_artifact(&select_program, 0, &function, &artifact_summary) ||
        !function ||
        artifact_summary.op_count != 3 ||
        artifact_summary.load_local_count != 1 ||
        artifact_summary.store_global_count != 1 ||
        artifact_summary.return_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: program function artifact mismatch\n");
        ok = 0;
    }
    if (!machine_select_program_get_function_summary_by_name(&select_program, "main", &function_index, &named_summary) ||
        function_index != 0 ||
        named_summary.op_count != 3 ||
        named_summary.load_local_count != 1 ||
        named_summary.store_global_count != 1 ||
        named_summary.return_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: program function-summary-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_program_get_function_artifact_by_name(
            &select_program, "main", &function_index, &named_function, &named_summary) ||
        function_index != 0 ||
        !named_function ||
        named_summary.op_count != 3 ||
        named_summary.load_local_count != 1 ||
        named_summary.store_global_count != 1 ||
        named_summary.return_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: program function-artifact-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_program_get_function(&select_program, 0, &function) ||
        !machine_select_function_get_summary(function, &name, NULL, NULL, NULL, &block_count, &spill_slot_count) ||
        !name || strcmp(name, "main") != 0 || block_count != 1 || spill_slot_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: function summary mismatch\n");
        ok = 0;
    }
    if (!machine_select_function_get_block(function, 0, &block) ||
        !block ||
        !machine_select_basic_block_get_summary(block, &block_id, &op_count, &has_terminator, &terminator_kind) ||
        block_id != 0 || op_count != 3 || !has_terminator || terminator_kind != MACHINE_SELECT_TERM_RETURN) {
        fprintf(stderr, "[machine-select] FAIL: block summary mismatch\n");
        ok = 0;
    }
    if (!machine_select_function_compute_summary(function, &summary) ||
        summary.op_count != 3 ||
        summary.call_count != 0 ||
        summary.load_local_count != 1 ||
        summary.store_global_count != 1 ||
        summary.return_count != 1 ||
        summary.return_imm_count != 0 ||
        summary.return_spill_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: computed function summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_distinguishes_cmp_ops(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.load_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=2 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    reg.0 = cmp.10 reg.0, reg.1\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_distinguishes_alu_immediate_ops(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: alui setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: alui setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: alui lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.0 = alui.0 reg.0, 9\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_distinguishes_cmp_immediate_ops(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpi setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpi setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpi lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.0 = cmpi.10 reg.0, 4\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_normalizes_commutative_immediate_to_rhs(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: commutative-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: commutative-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(7);
    instruction.as.binary.rhs = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: commutative-imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.0 = alui.0 reg.0, 7\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_keeps_noncommutative_lhs_immediate_out_of_imm_family(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: noncommutative-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: noncommutative-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_SUB;
    instruction.as.binary.lhs = machine_ir_operand_immediate(7);
    instruction.as.binary.rhs = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: noncommutative-imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.0 = alu.1 7, reg.0\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_materializes_constant_binary_results(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: constant-binary setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_immediate(10);
    instruction.as.binary.rhs = machine_ir_operand_immediate(32);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: constant-binary setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_immediate(7);
    instruction.as.binary.rhs = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(1), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: constant-binary lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 1\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.materialize_imm_count != 0 ||
        summary.alu_count != 0 ||
        summary.alu_imm_count != 0 ||
        summary.cmp_count != 0 ||
        summary.cmp_imm_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: constant-binary summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_removes_shadowed_trivial_defs(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: shadowed-trivial-def setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: shadowed-trivial-def setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(6);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: shadowed-trivial-def lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 6\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_canonicalized_bridge(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_register(0), &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_lower_canonicalized_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized bridge lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 7\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_materializes_immediates(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(42);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 42\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_distinguishes_immediate_store_families(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: immediate-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_LOCAL;
    instruction.as.store.slot = machine_ir_slot_local(0);
    instruction.as.store.value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: immediate-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: immediate-store lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    store_locali local.0, 7\n"
        "    store_globali global.0, 9\n"
        "    reti 0\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.store_local_count != 0 ||
        summary.store_local_imm_count != 1 ||
        summary.store_global_count != 0 ||
        summary.store_global_imm_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: immediate-store summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_forwards_tail_copy_into_last_store(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-store-copy setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-store-copy setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-store-copy setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-store-copy lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    store_global global.0, reg.0\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_forwards_tail_imm_into_last_alu(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 3;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[2].register_id = 2;
    machine_program.register_bank.registers[2].name = dup_text("r2");
    machine_program.register_bank.registers[2].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-alu-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-alu-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(9);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-alu-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(2), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-alu-imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.2 = alui.0 reg.0, 9\n"
        "    ret reg.2\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_forwards_tail_imm_into_last_call(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-call-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-call-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "sink";
    instruction.as.call.args = &instruction.result;
    instruction.as.call.arg_count = 1;
    instruction.as.call.args[0] = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: tail-call-imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    call_voidi sink(5)\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_forwards_multihop_copy_chain_into_store(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.register_bank.register_count = 3;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[2].register_id = 2;
    machine_program.register_bank.registers[2].name = dup_text("r2");
    machine_program.register_bank.registers[2].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2);
    instruction.as.mov_value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-store setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(2);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-store lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    store_global global.0, reg.0\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_forwards_multihop_imm_chain_into_call(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrOperand call_args[1];
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 3;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(3, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;
    machine_program.register_bank.registers[2].register_id = 2;
    machine_program.register_bank.registers[2].name = dup_text("r2");
    machine_program.register_bank.registers[2].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(2);
    instruction.as.mov_value = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "sink";
    call_args[0] = machine_ir_operand_register(2);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: multihop-call lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    call_voidi sink(5)\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_builds_from_machine_ir_report(void) {
    ValueSsaProgram ssa_program;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;
    ValueSsaError ssa_error;
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&ssa_error, 0, sizeof(ssa_error));
    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    value_ssa_program_init(&ssa_program);
    machine_ir_allocate_rewrite_report_init(&report);
    machine_select_program_init(&select_program);

    if (!value_ssa_program_append_function(&ssa_program, "main", 1, &function, &ssa_error) ||
        !function ||
        !value_ssa_function_append_block(function, NULL, &block, &ssa_error)) {
        fprintf(stderr, "[machine-select] FAIL: report bridge setup failed: %s\n", ssa_error.message);
        value_ssa_program_free(&ssa_program);
        machine_ir_allocate_rewrite_report_free(&report);
        machine_select_program_free(&select_program);
        return 0;
    }
    value_id = value_ssa_function_allocate_value(function);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value_id);
    instruction.as.mov_value = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(block, &instruction, &ssa_error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value_id), &ssa_error) ||
        !machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
            &ssa_program, 1, 1, &report, &machine_error) ||
        !machine_select_build_program_from_machine_ir_report(&report, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report bridge setup failed\n");
        value_ssa_program_free(&ssa_program);
        machine_ir_allocate_rewrite_report_free(&report);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reti 5\n");

    value_ssa_program_free(&ssa_program);
    machine_ir_allocate_rewrite_report_free(&report);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_lowers_compare_branch_terminator(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.load_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmp-branch lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=2 locals=2 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = load_local local.1\n"
        "    cmpbr.10 reg.0, reg.1, bb.1, bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_lowers_compare_branch_immediate_terminator(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpbri setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpbri setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpbri setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    if (!machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: cmpbri lowering failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    cmpbri.10 reg.0, 4, bb.1, bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_normalizes_compare_branch_lhs_immediate(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-cmpbri setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-cmpbri setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_LT;
    instruction.as.binary.lhs = machine_ir_operand_immediate(4);
    instruction.as.binary.rhs = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: normalized-cmpbri lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    cmpbri.14 reg.0, 4, bb.1, bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_terminator_query_surface(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    const MachineSelectFunction *function_view = NULL;
    const MachineSelectFunction *report_function_view = NULL;
    const MachineSelectBasicBlock *block_view = NULL;
    const MachineSelectBasicBlock *report_block_view = NULL;
    const MachineSelectBlockShapeSummary *block_summary = NULL;
    const MachineSelectBlockShapeSummary *artifact_block_summary = NULL;
    MachineSelectBlockShapeSummary raw_block_summary;
    MachineSelectTerminatorSummary terminator_summary;
    MachineSelectTerminatorSummary artifact_terminator_summary;
    size_t function_index = 0;
    size_t block_index = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    machine_select_lower_report_init(&report);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: terminator-query setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: terminator-query setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.load_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: terminator-query setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error) ||
        !machine_select_build_report_from_program(&select_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: terminator-query lowering/build failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_program_get_function_by_name(&select_program, "main", NULL, &function_view) ||
        !function_view ||
        !machine_select_program_get_block_by_function_name_and_block_id(
            &select_program, "main", 0, &function_index, &block_index, &function_view, &block_view) ||
        function_index != 0 ||
        block_index != 0 ||
        !function_view ||
        !block_view ||
        !machine_select_program_get_block_artifact_by_function_name_and_block_id(
            &select_program,
            "main",
            0,
            &function_index,
            &block_index,
            &function_view,
            &block_view,
            &raw_block_summary,
            &artifact_terminator_summary) ||
        function_index != 0 ||
        block_index != 0 ||
        raw_block_summary.block_id != 0 ||
        raw_block_summary.op_count != 2 ||
        !raw_block_summary.has_compare_branch_terminator ||
        artifact_terminator_summary.kind != MACHINE_SELECT_TERM_COMPARE_BRANCH ||
        artifact_terminator_summary.primary_target_block_id != 1 ||
        artifact_terminator_summary.secondary_target_block_id != 2 ||
        !machine_select_program_get_block_summary_by_function_name_and_block_id(
            &select_program, "main", 0, &function_index, &block_index, &raw_block_summary) ||
        function_index != 0 ||
        block_index != 0 ||
        raw_block_summary.block_id != 0 ||
        raw_block_summary.op_count != 2 ||
        !raw_block_summary.has_compare_branch_terminator ||
        !machine_select_program_get_block_terminator_summary_by_function_name_and_block_id(
            &select_program, "main", 0, &function_index, &block_index, &terminator_summary) ||
        function_index != 0 ||
        block_index != 0 ||
        !machine_select_function_get_block(function_view, 0, &block_view) ||
        !block_view ||
        terminator_summary.kind != MACHINE_SELECT_TERM_COMPARE_BRANCH ||
        !terminator_summary.has_primary_target ||
        terminator_summary.primary_target_block_id != 1 ||
        !terminator_summary.has_secondary_target ||
        terminator_summary.secondary_target_block_id != 2 ||
        !terminator_summary.has_compare ||
        terminator_summary.compare_op != MACHINE_IR_BINARY_EQ ||
        terminator_summary.compare_lhs.kind != MACHINE_SELECT_OPERAND_REGISTER ||
        terminator_summary.compare_lhs.machine_register_id != 0 ||
        terminator_summary.compare_rhs.kind != MACHINE_SELECT_OPERAND_REGISTER ||
        terminator_summary.compare_rhs.machine_register_id != 1) {
        fprintf(stderr, "[machine-select] FAIL: raw terminator summary mismatch\n");
        ok = 0;
    }

    if (!machine_select_lower_report_get_block_summary_artifact_by_function_name_and_block_id(
            &report, "main", 0, &function_index, &block_index, &block_summary) ||
        function_index != 0 ||
        block_index != 0 ||
        !block_summary ||
        block_summary->block_id != 0 ||
        !machine_select_lower_report_get_block_artifact_by_function_name_and_block_id(
            &report,
            "main",
            0,
            &function_index,
            &block_index,
            &report_function_view,
            &report_block_view,
            &artifact_block_summary,
            &artifact_terminator_summary) ||
        function_index != 0 ||
        block_index != 0 ||
        !report_function_view ||
        !report_block_view ||
        !artifact_block_summary ||
        artifact_block_summary->block_id != 0 ||
        artifact_terminator_summary.kind != MACHINE_SELECT_TERM_COMPARE_BRANCH ||
        artifact_terminator_summary.primary_target_block_id != 1 ||
        artifact_terminator_summary.secondary_target_block_id != 2 ||
        !machine_select_lower_report_get_block_by_function_name_and_block_id(
            &report, "main", 0, &function_index, &block_index, &report_function_view, &report_block_view) ||
        function_index != 0 ||
        block_index != 0 ||
        !report_function_view ||
        !report_block_view ||
        report_block_view->id != 0 ||
        !block_summary->has_compare_branch_terminator ||
        !machine_select_lower_report_get_block_terminator_summary(&report, 0, 0, &terminator_summary) ||
        terminator_summary.kind != MACHINE_SELECT_TERM_COMPARE_BRANCH ||
        terminator_summary.primary_target_block_id != 1 ||
        terminator_summary.secondary_target_block_id != 2 ||
        !machine_select_lower_report_get_block_terminator_summary_by_id(
            &report, 0, 1, &block_index, &terminator_summary) ||
        block_index != 1 ||
        terminator_summary.kind != MACHINE_SELECT_TERM_RETURN_IMM ||
        !terminator_summary.has_return_value ||
        terminator_summary.return_value.kind != MACHINE_SELECT_OPERAND_IMMEDIATE ||
        terminator_summary.return_value.immediate != 1 ||
        !machine_select_lower_report_get_block_terminator_summary_by_function_name_and_block_id(
            &report, "main", 2, &function_index, &block_index, &terminator_summary) ||
        function_index != 0 ||
        block_index != 2 ||
        terminator_summary.kind != MACHINE_SELECT_TERM_RETURN_IMM ||
        terminator_summary.return_value.immediate != 0) {
        fprintf(stderr, "[machine-select] FAIL: report terminator query mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_folds_constant_branches_to_jump(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_immediate(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: const-branch lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 0\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.plain_branch_count != 0 ||
        summary.compare_branch_count != 0 ||
        summary.compare_branch_imm_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: const-branch summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_folds_constant_compare_branches_to_jump(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: constant-cmp-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_LT;
    instruction.as.binary.lhs = machine_ir_operand_immediate(1);
    instruction.as.binary.rhs = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(7), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(9), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: constant-cmp-branch lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reti 7\n"
        "  bb.2:\n"
        "    reti 9\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.materialize_imm_count != 0 ||
        summary.compare_branch_count != 0 ||
        summary.compare_branch_imm_count != 0 ||
        summary.plain_branch_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: constant-cmp-branch summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_folds_materialized_boolean_branches_to_jump(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: materialized-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(11), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(22), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: materialized-branch lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.1\n"
        "  bb.1:\n"
        "    reti 11\n"
        "  bb.2:\n"
        "    reti 22\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.materialize_imm_count != 0 ||
        summary.compare_branch_count != 0 ||
        summary.compare_branch_imm_count != 0 ||
        summary.plain_branch_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: materialized-branch summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_removes_dead_load_before_folded_branch(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-load-before-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-load-before-branch setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.mov_value = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(1), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: dead-load-before-branch lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    jmp bb.2\n"
        "  bb.1:\n"
        "    reti 1\n"
        "  bb.2:\n"
        "    reti 2\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.op_count != 0 ||
        summary.load_local_count != 0 ||
        summary.materialize_imm_count != 0 ||
        summary.plain_branch_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: dead-load-before-branch summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_cleanup_propagates_multiconsumer_immediate(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrOperand call_args[1];
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multiconsumer-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(5);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multiconsumer-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "sink";
    call_args[0] = machine_ir_operand_register(0);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: multiconsumer-imm setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0);
    instruction.as.store.value = machine_ir_operand_register(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: multiconsumer-imm lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    call_voidi sink(5)\n"
        "    store_globali global.0, 5\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_lowers_value_call_with_immediate_arg_family(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrOperand call_args[1];
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: call-imm-family setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "f";
    call_args[0] = machine_ir_operand_immediate(7);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: call-imm-family lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = calli f(7)\n"
        "    ret reg.0\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.call_count != 0 ||
        summary.call_imm_count != 1 ||
        summary.call_void_count != 0 ||
        summary.call_void_imm_count != 0 ||
        summary.return_count != 1 ||
        summary.return_imm_count != 0 ||
        summary.return_spill_count != 0) {
        fprintf(stderr, "[machine-select] FAIL: call-imm-family summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_lowers_spill_result_call_families(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrOperand call_args[1];
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineSelectFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-call-family setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.call.callee_name = "g";
    call_args[0] = machine_ir_operand_immediate(9);
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_spill_slot(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: spill-call-family lowering setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=1\n"
        "  bb.0:\n"
        "    spill.0 = calli_spill g(9)\n"
        "    retspill spill.0\n");

    if (!machine_select_function_compute_summary(&select_program.functions[0], &summary) ||
        summary.call_count != 0 ||
        summary.call_imm_count != 0 ||
        summary.call_spill_count != 0 ||
        summary.call_imm_spill_count != 1 ||
        summary.return_count != 0 ||
        summary.return_imm_count != 0 ||
        summary.return_spill_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: spill-call-family summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_distinguishes_void_calls(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: void-call setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "sink";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: void-call lowering failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        return 0;
    }

    ok = expect_dump(
        &select_program,
        "machine_select\n"
        "function main params=0 locals=0 spills=0\n"
        "  bb.0:\n"
        "    call_void sink()\n"
        "    reti 0\n");

    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_builds_report_artifact(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    MachineSelectTargetPolicySummary program_policy_summary;
    const MachineSelectTargetPolicySummary *report_policy_summary = NULL;
    const MachineSelectFunctionSummary *summary = NULL;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    memset(&program_policy_summary, 0, sizeof(program_policy_summary));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    machine_select_lower_report_init(&report);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error) ||
        !machine_select_build_report_from_program(&select_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report artifact build failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_lower_report_get_function_summary_artifact(&report, 0, &summary) ||
        !machine_select_program_get_target_policy_summary(&select_program, &program_policy_summary) ||
        !machine_select_lower_report_get_target_policy_summary_artifact(&report, &report_policy_summary) ||
        !machine_select_verify_current_riscv32_preview_compatibility(&select_program, &select_error) ||
        !machine_select_lower_report_verify_current_riscv32_preview_compatibility(&report, &select_error) ||
        !machine_select_verify_current_riscv32_preview_bytes_compatibility(&select_program, &select_error) ||
        !machine_select_lower_report_verify_current_riscv32_preview_bytes_compatibility(&report, &select_error) ||
        !summary ||
        program_policy_summary.current_riscv32_preview_logical_register_cap != 8u ||
        !program_policy_summary.supports_early_immediate_legalization ||
        !program_policy_summary.supports_compare_branch_fusion ||
        !program_policy_summary.preserves_spill_operands_for_later_materialization ||
        !program_policy_summary.preserves_global_slot_ops_for_later_address_formation ||
        !report_policy_summary ||
        report_policy_summary->current_riscv32_preview_logical_register_cap != 8u ||
        !report_policy_summary->supports_early_immediate_legalization ||
        !report_policy_summary->supports_compare_branch_fusion ||
        !report_policy_summary->preserves_spill_operands_for_later_materialization ||
        !report_policy_summary->preserves_global_slot_ops_for_later_address_formation ||
        report.function_summary_count != 1 ||
        summary->op_count != 3 ||
        summary->call_count != 0 ||
        summary->load_local_count != 1 ||
        summary->alu_imm_count != 1 ||
        summary->alu_count != 0 ||
        summary->materialize_imm_count != 0 ||
        summary->blocks_with_calls_count != 0 ||
        summary->blocks_with_memory_ops_count != 1 ||
        summary->branch_block_count != 0 ||
        summary->jump_block_count != 0 ||
        summary->return_block_count != 1 ||
        !machine_select_build_report_from_program_dump(&select_program, &actual_text, &select_error) ||
        !actual_text ||
        !strstr(actual_text, "machine_select report registers=2 globals=1 functions=1")) {
        fprintf(stderr, "[machine-select] FAIL: report artifact summary mismatch\n");
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_rejects_riscv32_preview_incompatible_register_bank(void) {
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    int ok = 1;
    size_t register_index;

    memset(&select_error, 0, sizeof(select_error));
    machine_select_program_init(&select_program);

    select_program.register_bank.register_count = 9u;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(9u, sizeof(MachineSelectRegisterDesc));
    if (!select_program.register_bank.registers) {
        return 0;
    }
    for (register_index = 0u; register_index < 9u; ++register_index) {
        select_program.register_bank.registers[register_index].register_id = register_index;
        select_program.register_bank.registers[register_index].name = dup_text("r");
        select_program.register_bank.registers[register_index].allocatable = 1u;
        if (!select_program.register_bank.registers[register_index].name) {
            machine_select_program_free(&select_program);
            return 0;
        }
    }

    if (machine_select_verify_current_riscv32_preview_compatibility(&select_program, &select_error) ||
        strstr(select_error.message, "MACHINE-SELECT-420") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: oversized riscv32-preview register bank was not rejected: %s\n",
            select_error.message);
        ok = 0;
    }

    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_rejects_riscv32_preview_bytes_incompatible_branch_range(void) {
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    size_t op_index;
    int ok = 1;

    memset(&select_error, 0, sizeof(select_error));
    machine_select_program_init(&select_program);

    select_program.register_bank.register_count = 1u;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1u, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1u;
    select_program.function_capacity = 1u;
    select_program.functions = (MachineSelectFunction *)calloc(1u, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        return 0;
    }
    select_program.register_bank.registers[0].register_id = 0u;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;
    if (!select_program.register_bank.registers[0].name) {
        machine_select_program_free(&select_program);
        return 0;
    }

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 3u;
    select_program.functions[0].block_capacity = 3u;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(3u, sizeof(MachineSelectBasicBlock));
    if (!select_program.functions[0].name || !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0u;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0u);
    select_program.functions[0].blocks[0].terminator.as.branch.then_target = 2u;
    select_program.functions[0].blocks[0].terminator.as.branch.else_target = 1u;

    select_program.functions[0].blocks[1].id = 1u;
    select_program.functions[0].blocks[1].op_count = 513u;
    select_program.functions[0].blocks[1].op_capacity = 513u;
    select_program.functions[0].blocks[1].ops = (MachineSelectOp *)calloc(513u, sizeof(MachineSelectOp));
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(0u);
    if (!select_program.functions[0].blocks[1].ops) {
        machine_select_program_free(&select_program);
        return 0;
    }
    for (op_index = 0u; op_index < 513u; ++op_index) {
        select_program.functions[0].blocks[1].ops[op_index].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
        select_program.functions[0].blocks[1].ops[op_index].has_result = 1;
        select_program.functions[0].blocks[1].ops[op_index].result = machine_select_operand_register(0u);
        select_program.functions[0].blocks[1].ops[op_index].as.copy_value = machine_select_operand_immediate(5000);
    }

    select_program.functions[0].blocks[2].id = 2u;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[2].terminator.as.return_value = machine_select_operand_immediate(0u);

    if (machine_select_verify_current_riscv32_preview_bytes_compatibility(&select_program, &select_error) ||
        strstr(select_error.message, "MACHINE-SELECT-423") == NULL ||
        strstr(select_error.message, "MACHINE-LAYOUT-140") == NULL ||
        strstr(select_error.message, "MACHINE-EMIT-140") == NULL ||
        strstr(select_error.message, "MACHINE-ENCODE-125") == NULL ||
        strstr(select_error.message, "MACHINE-BYTES-344") == NULL) {
        fprintf(stderr,
            "[machine-select] FAIL: preview bytes-compatibility branch-range reject mismatch: %s\n",
            select_error.message);
        ok = 0;
    }

    machine_select_program_free(&select_program);
    return ok;
}

static int test_machine_select_report_refresh_surface(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectProgram select_program;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    const MachineSelectFunctionSummary *summary = NULL;
    char *actual_text = NULL;
    size_t total_block_summary_count = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_program_init(&select_program);
    machine_select_lower_report_init(&report);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error) ||
        !machine_select_lower_program_from_machine_ir(&machine_program, &select_program, &select_error) ||
        !machine_select_build_report_from_program(&select_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report refresh setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    report.program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    report.program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(99);
    if (!machine_select_lower_report_refresh(&report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report refresh failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_program_free(&select_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_lower_report_get_summary(&report,
            NULL,
            NULL,
            NULL,
            &total_block_summary_count,
            NULL,
            NULL,
            NULL,
            NULL) ||
        total_block_summary_count != 1 ||
        !machine_select_lower_report_get_function_summary_artifact(&report, 0, &summary) ||
        !summary ||
        summary->return_count != 0 ||
        summary->return_imm_count != 1 ||
        !machine_select_dump_lower_report_artifact(&report, &actual_text, &select_error) ||
        !actual_text ||
        !strstr(actual_text, "reti=1") ||
        !strstr(actual_text, "reti 99")) {
        fprintf(stderr, "[machine-select] FAIL: report refresh surface mismatch\n");
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&machine_program);
    machine_select_program_free(&select_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_report_query_surface(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    const MachineSelectProgram *report_program = NULL;
    const MachineSelectFunction *report_function = NULL;
    const MachineSelectFunction *filtered_function = NULL;
    const MachineSelectFunction *artifact_function = NULL;
    const MachineSelectFunctionSummary *summary = NULL;
    const MachineSelectFunctionSummary *named_summary = NULL;
    const MachineSelectFunctionSummary *filtered_summary = NULL;
    const MachineSelectFunctionSummary *artifact_summary_ptr = NULL;
    const MachineSelectBlockShapeSummary *block_summary = NULL;
    const MachineSelectBlockShapeSummary *block_summary_by_id = NULL;
    const size_t *indices = NULL;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t functions_with_calls = 0;
    size_t functions_with_spills = 0;
    size_t functions_with_memory_ops = 0;
    size_t functions_with_branches = 0;
    size_t function_index = 0;
    size_t filtered_index = 0;
    size_t block_index = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_lower_report_init(&report);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_global(&machine_program, "g", NULL, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: report query setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }
    function->spill_slot_count = 1;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_GLOBAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_global(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: report query setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0);
    instruction.as.call.callee_name = dup_text("sink");
    if (!instruction.as.call.callee_name ||
        !machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 0, 0, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: report query setup failed: %s\n", machine_error.message);
        free(instruction.as.call.callee_name);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_build_report_from_machine_ir_program(&machine_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report query build failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_lower_report_get_summary(&report,
            &register_count,
            &global_count,
            &function_count,
            NULL,
            &functions_with_calls,
            &functions_with_spills,
            &functions_with_memory_ops,
            &functions_with_branches) ||
        register_count != 1 || global_count != 1 || function_count != 1 ||
        functions_with_calls != 1 || functions_with_spills != 1 ||
        functions_with_memory_ops != 1 || functions_with_branches != 1) {
        fprintf(stderr, "[machine-select] FAIL: report summary query mismatch\n");
        ok = 0;
    }

    if (!machine_select_lower_report_get_program(&report, &report_program) ||
        !report_program ||
        !machine_select_lower_report_get_function_artifact(&report, 0, &artifact_function, &artifact_summary_ptr) ||
        !artifact_function ||
        !artifact_summary_ptr ||
        artifact_summary_ptr->call_spill_count != 1 ||
        !machine_select_lower_report_get_function_by_name(&report, "main", &function_index, &report_function) ||
        function_index != 0 || !report_function ||
        !machine_select_lower_report_get_function_artifact_by_name(
            &report, "main", &function_index, &artifact_function, &artifact_summary_ptr) ||
        function_index != 0 ||
        !artifact_function ||
        !artifact_summary_ptr ||
        artifact_summary_ptr->call_spill_count != 1 ||
        !machine_select_lower_report_get_function_summary_artifact(&report, 0, &summary) ||
        !machine_select_lower_report_get_function_summary_artifact_by_name(
            &report, "main", &function_index, &named_summary) ||
        function_index != 0 || !named_summary ||
        !machine_select_lower_report_get_block_summary_artifact(&report, 0, 0, &block_summary) ||
        !machine_select_lower_report_get_block_summary_artifact_by_id(
            &report, 0, 0, &block_index, &block_summary_by_id) ||
        block_index != 0 ||
        !summary ||
        !block_summary ||
        !block_summary_by_id ||
        named_summary->call_spill_count != 1 ||
        summary->call_spill_count != 1 ||
        summary->load_global_count != 1 ||
        summary->plain_branch_count != 1 ||
        summary->blocks_with_calls_count != 1 ||
        summary->blocks_with_memory_ops_count != 1 ||
        summary->branch_block_count != 1 ||
        summary->jump_block_count != 0 ||
        summary->return_block_count != 0 ||
        block_summary->block_id != 0 ||
        block_summary->op_count != 2 ||
        block_summary->call_count != 1 ||
        block_summary->load_global_count != 1 ||
        block_summary->store_global_count != 0 ||
        block_summary_by_id->block_id != 0 ||
        block_summary_by_id->call_count != 1 ||
        !block_summary->has_branch_terminator ||
        block_summary->has_return_terminator ||
        !summary->has_spills) {
        fprintf(stderr, "[machine-select] FAIL: report program/function query mismatch\n");
        ok = 0;
    }

    if (!machine_select_lower_report_get_functions_with_calls(&report, &function_count, &indices) ||
        function_count != 1 || !indices || indices[0] != 0) {
        fprintf(stderr, "[machine-select] FAIL: report call filter mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_calls(
            &report, 0, &function_index, &filtered_function, &filtered_summary) ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->call_spill_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report call filter navigation mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_calls_by_name(
            &report, "main", &filtered_index, &function_index, &filtered_function, &filtered_summary) ||
        filtered_index != 0 ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->call_spill_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report call filter-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_functions_with_spills(&report, &function_count, &indices) ||
        function_count != 1 || !indices || indices[0] != 0) {
        fprintf(stderr, "[machine-select] FAIL: report spill filter mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_spills(
            &report, 0, &function_index, &filtered_function, &filtered_summary) ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        !filtered_summary->has_spills) {
        fprintf(stderr, "[machine-select] FAIL: report spill filter navigation mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_spills_by_name(
            &report, "main", &filtered_index, &function_index, &filtered_function, &filtered_summary) ||
        filtered_index != 0 ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        !filtered_summary->has_spills) {
        fprintf(stderr, "[machine-select] FAIL: report spill filter-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_functions_with_memory_ops(&report, &function_count, &indices) ||
        function_count != 1 || !indices || indices[0] != 0) {
        fprintf(stderr, "[machine-select] FAIL: report memory-op filter mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_memory_ops(
            &report, 0, &function_index, &filtered_function, &filtered_summary) ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->load_global_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report memory-op filter navigation mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_memory_ops_by_name(
            &report, "main", &filtered_index, &function_index, &filtered_function, &filtered_summary) ||
        filtered_index != 0 ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->load_global_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report memory-op filter-by-name mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_functions_with_branches(&report, &function_count, &indices) ||
        function_count != 1 || !indices || indices[0] != 0) {
        fprintf(stderr, "[machine-select] FAIL: report branch filter mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_branches(
            &report, 0, &function_index, &filtered_function, &filtered_summary) ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->plain_branch_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report branch filter navigation mismatch\n");
        ok = 0;
    }
    if (!machine_select_lower_report_get_function_with_branches_by_name(
            &report, "main", &filtered_index, &function_index, &filtered_function, &filtered_summary) ||
        filtered_index != 0 ||
        function_index != 0 ||
        !filtered_function ||
        !filtered_summary ||
        filtered_summary->plain_branch_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: report branch filter-by-name mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_report_block_filter_surface(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    const MachineSelectBasicBlock *block = NULL;
    const MachineSelectBlockShapeSummary *block_summary = NULL;
    MachineSelectTerminatorSummary terminator_summary;
    size_t function_index = 0;
    size_t block_index = 0;
    size_t count = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_lower_report_init(&report);

    machine_program.register_bank.register_count = 2;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(2, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;
    machine_program.register_bank.registers[1].register_id = 1;
    machine_program.register_bank.registers[1].name = dup_text("r1");
    machine_program.register_bank.registers[1].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_local(function, "b", 1, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: block-filter setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: block-filter setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1);
    instruction.as.load_slot = machine_ir_slot_local(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: block-filter setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_register(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), &machine_error) ||
        !machine_select_build_report_from_machine_ir_program(&machine_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: block-filter lowering/build failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_lower_report_get_function_block_filter_count(
            &report, 0, MACHINE_SELECT_BLOCK_FILTER_MEMORY_OPS, &count) ||
        count != 1 ||
        !machine_select_lower_report_get_function_block_filter_count(
            &report, 0, MACHINE_SELECT_BLOCK_FILTER_RETURNS, &count) ||
        count != 2 ||
        !machine_select_lower_report_get_function_block_filter_count(
            &report, 0, MACHINE_SELECT_BLOCK_FILTER_BRANCHES, &count) ||
        count != 0 ||
        !machine_select_lower_report_get_function_block_filter_count(
            &report, 0, MACHINE_SELECT_BLOCK_FILTER_COMPARE_BRANCHES, &count) ||
        count != 1) {
        fprintf(stderr, "[machine-select] FAIL: block-filter count mismatch\n");
        ok = 0;
    }

    if (!machine_select_lower_report_get_function_filtered_block_summary(
            &report, 0, MACHINE_SELECT_BLOCK_FILTER_COMPARE_BRANCHES, 0, &block_index, &block_summary) ||
        block_index != 0 ||
        !block_summary ||
        block_summary->block_id != 0 ||
        !block_summary->has_compare_branch_terminator ||
        !machine_select_lower_report_get_function_filtered_block_artifact(
            &report,
            0,
            MACHINE_SELECT_BLOCK_FILTER_COMPARE_BRANCHES,
            0,
            &block_index,
            &block,
            &block_summary,
            &terminator_summary) ||
        block_index != 0 ||
        !block ||
        !block_summary ||
        block_summary->block_id != 0 ||
        terminator_summary.kind != MACHINE_SELECT_TERM_COMPARE_BRANCH ||
        terminator_summary.primary_target_block_id != 1 ||
        terminator_summary.secondary_target_block_id != 2 ||
        !machine_select_lower_report_get_function_filtered_block_summary_by_name(
            &report, "main", MACHINE_SELECT_BLOCK_FILTER_RETURNS, 1, &function_index, &block_index, &block_summary) ||
        function_index != 0 ||
        block_index != 2 ||
        !block_summary ||
        block_summary->block_id != 2 ||
        !machine_select_lower_report_get_function_filtered_block_artifact_by_name(
            &report,
            "main",
            MACHINE_SELECT_BLOCK_FILTER_RETURNS,
            1,
            &function_index,
            &block_index,
            &block,
            &block_summary,
            &terminator_summary) ||
        function_index != 0 ||
        block_index != 2 ||
        !block ||
        !block_summary ||
        block_summary->block_id != 2 ||
        terminator_summary.kind != MACHINE_SELECT_TERM_RETURN_IMM ||
        !terminator_summary.has_return_value ||
        terminator_summary.return_value.immediate != 0 ||
        !block_summary->has_return_terminator) {
        fprintf(stderr, "[machine-select] FAIL: block-filter summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_report_dump_surface(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_lower_report_init(&report);

    if (!build_machine_ir_smoke_program(&machine_program, &machine_error) ||
        !machine_select_build_report_from_machine_ir_program(&machine_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: report dump setup failed\n");
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    ok = expect_report_dump(
        &report,
        "machine_select report registers=2 globals=1 functions=1 with_calls=0 with_spills=0 with_memory_ops=1 with_branches=0\n"
        "target_policy preview_reg_cap=8 imm_legalization=1 cmpbr_fusion=1 preserves_spills=1 preserves_global_slots=1\n"
        "functions_with_calls:\n"
        "functions_with_spills:\n"
        "functions_with_memory_ops: 0\n"
        "functions_with_branches:\n"
        "function_summaries:\n"
        "  fn.0 main blocks=1 ops=3 calls=0 spills=0 load_local=1 store_local=0 store_locali=0 load_global=0 store_global=1 store_globali=0 ret=1 reti=0 retspill=0 br=0 cmpbr=0 cmpbri=0 blocks_with_calls=0 blocks_with_memory_ops=1 branch_blocks=0 jump_blocks=0 return_blocks=1\n"
        "    bb.0 ops=3 calls=0 load_local=1 store_local=0 store_locali=0 load_global=0 store_global=1 store_globali=0 term_ret=1 term_jmp=0 term_br=0 term_cmpbr=0\n"
        "\n"
        "machine_select\n"
        "function main params=1 locals=1 spills=0\n"
        "  bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    reg.1 = alui.0 reg.0, 1\n"
        "    store_global global.0, reg.1\n"
        "    ret reg.1\n");

    machine_ir_program_free(&machine_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_canonicalized_report_surface(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    const MachineSelectFunctionSummary *summary = NULL;
    size_t function_index = 0;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&select_error, 0, sizeof(select_error));
    machine_ir_program_init(&machine_program);
    machine_select_lower_report_init(&report);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.mov_value = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0), &machine_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_build_report_from_canonicalized_machine_ir_program(&machine_program, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report build failed: %s\n", select_error.message);
        machine_ir_program_free(&machine_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_lower_report_get_function_summary_artifact_by_name(&report, "main", &function_index, &summary) ||
        function_index != 0 ||
        !summary ||
        summary->block_count != 1) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report summary mismatch\n");
        ok = 0;
    }
    if (!machine_select_dump_report_from_canonicalized_machine_ir_program(&machine_program, &actual_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report dump failed: %s\n", select_error.message);
        ok = 0;
    } else if (!strstr(actual_text, "functions=1") ||
        !strstr(actual_text, "fn.0 main blocks=1") ||
        strstr(actual_text, "bb.1")) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized report dump mismatch\nactual:\n%s\n", actual_text);
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&machine_program);
    machine_select_lower_report_free(&report);
    return ok;
}

static int test_machine_select_builds_canonicalized_flat_report_from_value_ssa(void) {
    ValueSsaProgram ssa_program;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *block = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;
    ValueSsaError ssa_error;
    MachineSelectLowerReport report;
    MachineSelectError select_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&ssa_error, 0, sizeof(ssa_error));
    memset(&select_error, 0, sizeof(select_error));
    value_ssa_program_init(&ssa_program);
    machine_select_lower_report_init(&report);

    if (!value_ssa_program_append_function(&ssa_program, "main", 1, &function, &ssa_error) ||
        !function ||
        !value_ssa_function_append_block(function, NULL, &block, &ssa_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized flat report setup failed: %s\n", ssa_error.message);
        value_ssa_program_free(&ssa_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    value_id = value_ssa_function_allocate_value(function);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value_id);
    instruction.as.mov_value = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(block, &instruction, &ssa_error) ||
        !value_ssa_block_set_return(block, value_ssa_value_id(value_id), &ssa_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized flat report setup failed: %s\n", ssa_error.message);
        value_ssa_program_free(&ssa_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
            &ssa_program, 1, 1, &report, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized flat report build failed: %s\n", select_error.message);
        value_ssa_program_free(&ssa_program);
        machine_select_lower_report_free(&report);
        return 0;
    }

    if (!machine_select_dump_lower_report_artifact(&report, &actual_text, &select_error)) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized flat report dump failed: %s\n", select_error.message);
        ok = 0;
    } else if (!strstr(actual_text, "functions=1") || !strstr(actual_text, "reti 5")) {
        fprintf(stderr, "[machine-select] FAIL: canonicalized flat report dump mismatch\nactual:\n%s\n", actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_free(&ssa_program);
    machine_select_lower_report_free(&report);
    return ok;
}

int main(void) {
    if (!test_machine_select_lower_machine_ir_smoke()) {
        return 1;
    }
    if (!test_machine_select_rejects_phi_input()) {
        return 1;
    }
    if (!test_machine_select_summary_surface()) {
        return 1;
    }
    if (!test_machine_select_distinguishes_cmp_ops()) {
        return 1;
    }
    if (!test_machine_select_distinguishes_alu_immediate_ops()) {
        return 1;
    }
    if (!test_machine_select_distinguishes_cmp_immediate_ops()) {
        return 1;
    }
    if (!test_machine_select_normalizes_commutative_immediate_to_rhs()) {
        return 1;
    }
    if (!test_machine_select_keeps_noncommutative_lhs_immediate_out_of_imm_family()) {
        return 1;
    }
    if (!test_machine_select_materializes_constant_binary_results()) {
        return 1;
    }
    if (!test_machine_select_cleanup_removes_shadowed_trivial_defs()) {
        return 1;
    }
    if (!test_machine_select_canonicalized_bridge()) {
        return 1;
    }
    if (!test_machine_select_materializes_immediates()) {
        return 1;
    }
    if (!test_machine_select_distinguishes_immediate_store_families()) {
        return 1;
    }
    if (!test_machine_select_cleanup_forwards_tail_copy_into_last_store()) {
        return 1;
    }
    if (!test_machine_select_cleanup_forwards_tail_imm_into_last_alu()) {
        return 1;
    }
    if (!test_machine_select_cleanup_forwards_tail_imm_into_last_call()) {
        return 1;
    }
    if (!test_machine_select_cleanup_forwards_multihop_copy_chain_into_store()) {
        return 1;
    }
    if (!test_machine_select_cleanup_forwards_multihop_imm_chain_into_call()) {
        return 1;
    }
    if (!test_machine_select_builds_from_machine_ir_report()) {
        return 1;
    }
    if (!test_machine_select_lowers_compare_branch_terminator()) {
        return 1;
    }
    if (!test_machine_select_lowers_compare_branch_immediate_terminator()) {
        return 1;
    }
    if (!test_machine_select_normalizes_compare_branch_lhs_immediate()) {
        return 1;
    }
    if (!test_machine_select_terminator_query_surface()) {
        return 1;
    }
    if (!test_machine_select_folds_constant_branches_to_jump()) {
        return 1;
    }
    if (!test_machine_select_folds_constant_compare_branches_to_jump()) {
        return 1;
    }
    if (!test_machine_select_folds_materialized_boolean_branches_to_jump()) {
        return 1;
    }
    if (!test_machine_select_cleanup_removes_dead_load_before_folded_branch()) {
        return 1;
    }
    if (!test_machine_select_cleanup_propagates_multiconsumer_immediate()) {
        return 1;
    }
    if (!test_machine_select_lowers_value_call_with_immediate_arg_family()) {
        return 1;
    }
    if (!test_machine_select_lowers_spill_result_call_families()) {
        return 1;
    }
    if (!test_machine_select_distinguishes_void_calls()) {
        return 1;
    }
    if (!test_machine_select_builds_report_artifact()) {
        return 1;
    }
    if (!test_machine_select_rejects_riscv32_preview_incompatible_register_bank()) {
        return 1;
    }
    if (!test_machine_select_rejects_riscv32_preview_bytes_incompatible_branch_range()) {
        return 1;
    }
    if (!test_machine_select_report_refresh_surface()) {
        return 1;
    }
    if (!test_machine_select_report_query_surface()) {
        return 1;
    }
    if (!test_machine_select_report_block_filter_surface()) {
        return 1;
    }
    if (!test_machine_select_report_dump_surface()) {
        return 1;
    }
    if (!test_machine_select_canonicalized_report_surface()) {
        return 1;
    }
    if (!test_machine_select_builds_canonicalized_flat_report_from_value_ssa()) {
        return 1;
    }
    printf("[machine-select] PASS\n");
    return 0;
}
