#include "machine/layout.h"

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

static int expect_dump(const MachineLayoutProgram *program, const char *expected_text) {
    char *actual_text = NULL;
    MachineLayoutError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_layout_dump_program(program, &actual_text, &error)) {
        fprintf(stderr, "[machine-layout] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-layout] FAIL: dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int expect_report_dump(const MachineLayoutReport *report, const char *expected_text) {
    char *actual_text = NULL;
    MachineLayoutError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_layout_dump_report(report, &actual_text, &error)) {
        fprintf(stderr, "[machine-layout] FAIL: report dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-layout] FAIL: report dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int build_machine_ir_layout_smoke_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 2u;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(2u, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0u;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1u;
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
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.load_slot = machine_ir_slot_local(0u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(1u);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_STORE_GLOBAL;
    instruction.as.store.slot = machine_ir_slot_global(0u);
    instruction.as.store.value = machine_ir_operand_register(1u);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(1u), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_layout_call_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrOperand args[1];

    machine_ir_program_init(program);
    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }
    function->spill_slot_count = 1u;

    memset(&instruction, 0, sizeof(instruction));
    memset(args, 0, sizeof(args));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_spill_slot(0u);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1u;
    instruction.as.call.args = args;
    args[0] = machine_ir_operand_immediate(7);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int build_machine_ir_layout_compare_branch_imm_program(MachineIrProgram *program, MachineIrError *error) {
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;

    machine_ir_program_init(program);
    program->register_bank.register_count = 1u;
    program->register_bank.registers = (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!program->register_bank.registers) {
        return 0;
    }
    program->register_bank.registers[0].register_id = 0u;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, error) ||
        !machine_ir_function_append_block(function, NULL, NULL, error) ||
        !machine_ir_function_append_block(function, NULL, NULL, error)) {
        machine_ir_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.binary.op = MACHINE_IR_BINARY_LT;
    instruction.as.binary.lhs = machine_ir_operand_register(0u);
    instruction.as.binary.rhs = machine_ir_operand_immediate(4);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0u), 1u, 2u, error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(0), error)) {
        machine_ir_program_free(program);
        return 0;
    }

    return 1;
}

static int test_machine_layout_lowers_fallthrough_branch_from_machine_ir(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

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
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: lowering failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=1 locals=1 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.1:\n"
        "    reti 1\n"
        "  layout.2 -> bb.2:\n"
        "    reti 2\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 3 ||
        summary.op_count != 1 ||
        summary.compare_branch_imm_fallthrough_count != 1 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_lowers_addr_function_from_machine_ir(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *callee = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1u;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0u;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "callee", 0, &callee, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_ADDR_FUNCTION;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0u);
    instruction.as.addr_function_name = dup_text("callee");
    if (!instruction.as.addr_function_name) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_register(0u), &machine_error) ||
        !machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        free(instruction.as.addr_function_name);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.addr_function_name);

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function callee params=0 locals=0 spills=0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reg.0 = addr_function callee\n"
        "    ret reg.0\n");

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_lowers_void_return_from_machine_select(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineSelectError select_error;
    MachineLayoutError layout_error;
    int ok = 1;

    memset(&select_error, 0, sizeof(select_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.functions) {
        return 0;
    }

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 1;
    select_program.functions[0].block_capacity = 1;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(1, sizeof(MachineSelectBasicBlock));
    if (!select_program.functions[0].name || !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_RETURN;
    select_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_none();

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: void-return lowering failed: %s\n", layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    ret\n");

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_rejects_load_store_slot_kind_mismatch(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutProgram mutated_program;
    MachineLayoutError layout_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);
    machine_layout_program_init(&mutated_program);

    if (!build_machine_ir_layout_smoke_program(&machine_program, &machine_error) ||
        !machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: slot-kind mismatch setup failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&mutated_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_clone_program(&layout_program, &mutated_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: load-slot mismatch clone failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&mutated_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    mutated_program.functions[0].blocks[0].ops[0].as.load_slot = machine_select_slot_global(0u);
    memset(&layout_error, 0, sizeof(layout_error));
    if (machine_layout_verify_program(&mutated_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: verifier accepted load-local/global-slot mismatch\n");
        ok = 0;
    }

    machine_layout_program_free(&mutated_program);
    machine_layout_program_init(&mutated_program);
    if (!machine_layout_clone_program(&layout_program, &mutated_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: store-slot mismatch clone failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&mutated_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    mutated_program.functions[0].blocks[0].ops[2].as.store.slot = machine_select_slot_local(0u);
    memset(&layout_error, 0, sizeof(layout_error));
    if (machine_layout_verify_program(&mutated_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: verifier accepted store-global/local-slot mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&mutated_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_rejects_call_result_family_mismatch(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutProgram mutated_program;
    MachineLayoutError layout_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);
    machine_layout_program_init(&mutated_program);

    if (!build_machine_ir_layout_call_program(&machine_program, &machine_error) ||
        !machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: call-result mismatch setup failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&mutated_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_clone_program(&layout_program, &mutated_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: call-result mismatch clone failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&mutated_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    mutated_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_IMM;
    memset(&layout_error, 0, sizeof(layout_error));
    if (machine_layout_verify_program(&mutated_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: verifier accepted register-result call family mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&mutated_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_rejects_compare_branch_imm_shape_mismatch(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutProgram mutated_program;
    MachineLayoutError layout_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);
    machine_layout_program_init(&mutated_program);

    if (!build_machine_ir_layout_compare_branch_imm_program(&machine_program, &machine_error) ||
        !machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: cmpbri mismatch setup failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&mutated_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_clone_program(&layout_program, &mutated_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: cmpbri mismatch clone failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&mutated_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (mutated_program.functions[0].blocks[0].terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM) {
        mutated_program.functions[0].blocks[0].terminator.as.compare_branch.rhs = machine_select_operand_register(0u);
    } else {
        mutated_program.functions[0].blocks[0].terminator.as.compare_branch_fallthrough.rhs =
            machine_select_operand_register(0u);
    }
    memset(&layout_error, 0, sizeof(layout_error));
    if (machine_layout_verify_program(&mutated_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: verifier accepted compare-branch-imm rhs non-immediate mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&mutated_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_report_query_dump_rejects_missing_function_table(void) {
    MachineIrProgram machine_program;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutReport report;
    MachineLayoutError layout_error;
    const MachineLayoutFunction *function_view = NULL;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);
    machine_layout_report_init(&report);

    if (!build_machine_ir_layout_smoke_program(&machine_program, &machine_error) ||
        !machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error) ||
        !machine_layout_build_report_from_program(&layout_program, &report, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: malformed report setup failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        machine_layout_report_free(&report);
        return 0;
    }

    free(report.program.functions);
    report.program.functions = NULL;
    memset(&layout_error, 0, sizeof(layout_error));
    if (machine_layout_report_get_function_by_name(&report, "main", NULL, &function_view) ||
        machine_layout_dump_report(&report, &actual_text, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: malformed report query/dump should fail on missing function table\n");
        ok = 0;
    }

    free(actual_text);
    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    machine_layout_report_free(&report);
    return ok;
}

static int test_machine_layout_bridge_respects_upstream_jump_canonicalization(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[0], 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(7), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: jump setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: jump lowering failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 7\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 1 ||
        summary.fallthrough_count != 0 ||
        summary.return_imm_count != 1) {
        fprintf(stderr, "[machine-layout] FAIL: canonicalized jump summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_reorders_blocks_to_create_branch_fallthrough(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

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
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge branch-reorder setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge branch-reorder setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge branch-reorder lowering failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=1 locals=1 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    brft.t reg.0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reti 2\n"
        "  layout.2 -> bb.1:\n"
        "    reti 1\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 3 ||
        summary.op_count != 1 ||
        summary.branch_fallthrough_count != 1 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 0 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge branch-reorder summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_reorders_blocks_to_create_branch_fallthrough_for_compare_family(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

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
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare branch-reorder setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare branch-reorder setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare branch-reorder setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare branch-reorder lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=1 locals=1 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reg.0 = load_local local.0\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reti 2\n"
        "  layout.2 -> bb.1:\n"
        "    reti 1\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 3 ||
        summary.op_count != 1 ||
        summary.compare_branch_imm_fallthrough_count != 1 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 0 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare branch-reorder summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_lowers_ops_and_fallthrough_from_machine_select(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.global_count = 1;
    select_program.global_capacity = 1;
    select_program.globals = (MachineSelectGlobal *)calloc(1, sizeof(MachineSelectGlobal));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.globals || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.globals[0].id = 0;
    select_program.globals[0].name = dup_text("g");

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 2;
    select_program.functions[0].block_capacity = 2;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(2, sizeof(MachineSelectBasicBlock));
    if (!select_program.globals[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].op_count = 1;
    select_program.functions[0].blocks[0].op_capacity = 1;
    select_program.functions[0].blocks[0].ops = (MachineSelectOp *)calloc(1, sizeof(MachineSelectOp));
    if (!select_program.functions[0].blocks[0].ops) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    select_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
    select_program.functions[0].blocks[0].ops[0].as.store.slot = machine_select_slot_global(0);
    select_program.functions[0].blocks[0].ops[0].as.store.value = machine_select_operand_immediate(9);
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[0].terminator.as.jump_target = 1;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(7);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: select lowering failed: %s\n", layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    store_globali global.0, 9\n"
        "    fallthrough layout.1\n"
        "  layout.1 -> bb.1:\n"
        "    reti 7\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 2 ||
        summary.op_count != 1 ||
        summary.fallthrough_count != 1 ||
        summary.return_imm_count != 1) {
        fprintf(stderr, "[machine-layout] FAIL: direct select summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_query_surface(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutTargetPolicySummary policy_summary;
    const MachineLayoutFunction *function = NULL;
    const MachineLayoutFunction *named_function = NULL;
    const MachineLayoutBlock *block = NULL;
    MachineLayoutBlockSummary block_summary;
    const char *name = NULL;
    int has_body = 0;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t function_index = 0;
    size_t parameter_count = 0;
    size_t local_count = 0;
    size_t block_count = 0;
    size_t spill_slot_count = 0;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    memset(&policy_summary, 0, sizeof(policy_summary));
    memset(&block_summary, 0, sizeof(block_summary));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.global_count = 1;
    select_program.global_capacity = 1;
    select_program.globals = (MachineSelectGlobal *)calloc(1, sizeof(MachineSelectGlobal));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.globals || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.globals[0].id = 0u;
    select_program.globals[0].name = dup_text("g");
    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].parameter_count = 1u;
    select_program.functions[0].local_count = 1u;
    select_program.functions[0].local_capacity = 1u;
    select_program.functions[0].locals = (MachineSelectLocal *)calloc(1u, sizeof(MachineSelectLocal));
    select_program.functions[0].block_count = 2u;
    select_program.functions[0].block_capacity = 2u;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(2u, sizeof(MachineSelectBasicBlock));
    if (!select_program.globals[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].locals ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    select_program.functions[0].locals[0].id = 0u;
    select_program.functions[0].locals[0].source_name = dup_text("a");
    select_program.functions[0].locals[0].is_parameter = 1;
    if (!select_program.functions[0].locals[0].source_name) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0u;
    select_program.functions[0].blocks[0].op_count = 1u;
    select_program.functions[0].blocks[0].op_capacity = 1u;
    select_program.functions[0].blocks[0].ops = (MachineSelectOp *)calloc(1u, sizeof(MachineSelectOp));
    if (!select_program.functions[0].blocks[0].ops) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    select_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
    select_program.functions[0].blocks[0].ops[0].as.store.slot = machine_select_slot_global(0);
    select_program.functions[0].blocks[0].ops[0].as.store.value = machine_select_operand_immediate(9);
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[0].terminator.as.jump_target = 1u;

    select_program.functions[0].blocks[1].id = 1u;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(7);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: query surface lowering failed: %s\n", layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_program_get_summary(&layout_program, &register_count, &global_count, &function_count) ||
        register_count != 0u || global_count != 1u || function_count != 1u ||
        !machine_layout_program_get_target_policy_summary(&layout_program, &policy_summary) ||
        policy_summary.select_policy.current_riscv32_preview_logical_register_cap != 8u ||
        !policy_summary.preserves_spill_operands_for_later_materialization ||
        !policy_summary.preserves_global_slot_ops_for_later_address_formation ||
        !policy_summary.preserves_fallthrough_terminator_shapes ||
        !machine_layout_program_get_function(&layout_program, 0u, &function) || !function ||
        !machine_layout_program_get_function_by_name(&layout_program, "main", &function_index, &named_function) ||
        function_index != 0u || !named_function ||
        !machine_layout_function_get_summary(
            function, &name, &has_body, &parameter_count, &local_count, &block_count, &spill_slot_count) ||
        !name || strcmp(name, "main") != 0 || !has_body || parameter_count != 1u ||
        local_count != 1u || block_count != 2u || spill_slot_count != 0u ||
        !machine_layout_function_get_block(function, 0u, &block) || !block ||
        !machine_layout_block_get_summary(block, &block_summary) ||
        block_summary.layout_index != 0u ||
        block_summary.original_block_id != 0u ||
        block_summary.op_count != 1u ||
        !block_summary.has_terminator ||
        block_summary.terminator_kind != MACHINE_LAYOUT_TERM_FALLTHROUGH) {
        fprintf(stderr, "[machine-layout] FAIL: query surface mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_report_surface(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutReport report;
    MachineLayoutError layout_error;
    MachineLayoutTargetPolicySummary policy_summary;
    const MachineLayoutTargetPolicySummary *report_policy_summary = NULL;
    const MachineLayoutProgram *report_program = NULL;
    const MachineLayoutFunction *report_function = NULL;
    const MachineLayoutFunctionSummary *function_summary = NULL;
    const MachineLayoutBlockSummary *block_summary = NULL;
    const size_t *function_indices = NULL;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t total_block_summary_count = 0;
    size_t functions_with_fallthrough = 0;
    size_t functions_with_branches = 0;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    memset(&policy_summary, 0, sizeof(policy_summary));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);
    machine_layout_report_init(&report);

    select_program.global_count = 1;
    select_program.global_capacity = 1;
    select_program.globals = (MachineSelectGlobal *)calloc(1u, sizeof(MachineSelectGlobal));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1u, sizeof(MachineSelectFunction));
    if (!select_program.globals || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        machine_layout_report_free(&report);
        return 0;
    }
    select_program.globals[0].id = 0u;
    select_program.globals[0].name = dup_text("g");
    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 2u;
    select_program.functions[0].block_capacity = 2u;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(2u, sizeof(MachineSelectBasicBlock));
    if (!select_program.globals[0].name || !select_program.functions[0].name || !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        machine_layout_report_free(&report);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0u;
    select_program.functions[0].blocks[0].op_count = 1u;
    select_program.functions[0].blocks[0].op_capacity = 1u;
    select_program.functions[0].blocks[0].ops = (MachineSelectOp *)calloc(1u, sizeof(MachineSelectOp));
    if (!select_program.functions[0].blocks[0].ops) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        machine_layout_report_free(&report);
        return 0;
    }
    select_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
    select_program.functions[0].blocks[0].ops[0].as.store.slot = machine_select_slot_global(0u);
    select_program.functions[0].blocks[0].ops[0].as.store.value = machine_select_operand_immediate(9);
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[0].terminator.as.jump_target = 1u;

    select_program.functions[0].blocks[1].id = 1u;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(7);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error) ||
        !machine_layout_build_report_from_program(&layout_program, &report, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: report surface setup failed: %s\n", layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        machine_layout_report_free(&report);
        return 0;
    }

    if (!machine_layout_program_get_target_policy_summary(&layout_program, &policy_summary) ||
        !machine_layout_report_get_summary(
            &report,
            &register_count,
            &global_count,
            &function_count,
            &total_block_summary_count,
            &functions_with_fallthrough,
            &functions_with_branches) ||
        register_count != 0u || global_count != 1u || function_count != 1u ||
        total_block_summary_count != 2u || functions_with_fallthrough != 1u || functions_with_branches != 0u ||
        !machine_layout_report_get_target_policy_summary_artifact(&report, &report_policy_summary) ||
        !report_policy_summary ||
        report_policy_summary->select_policy.current_riscv32_preview_logical_register_cap != 8u ||
        !machine_layout_report_verify_current_riscv32_preview_compatibility(&report, &layout_error) ||
        !machine_layout_report_get_program(&report, &report_program) || !report_program ||
        !machine_layout_report_get_function_by_name(&report, "main", NULL, &report_function) || !report_function ||
        !machine_layout_report_get_function_summary_artifact(&report, 0u, &function_summary) || !function_summary ||
        function_summary->fallthrough_count != 1u || function_summary->return_imm_count != 1u ||
        !machine_layout_report_get_block_summary(&report, 0u, 0u, &block_summary) || !block_summary ||
        block_summary->layout_index != 0u ||
        !machine_layout_report_get_functions_with_fallthrough(&report, &function_count, &function_indices) ||
        function_count != 1u || !function_indices || function_indices[0] != 0u ||
        !machine_layout_report_get_functions_with_branches(&report, &function_count, &function_indices) ||
        function_count != 0u ||
        !expect_report_dump(
            &report,
            "machine_layout-report registers=0 globals=1 functions=1 total_block_summaries=2 fallthrough_funcs=1 branch_funcs=0\n"
            "target_policy preview_reg_cap=8 preserves_spills=1 preserves_global_slots=1 preserves_fallthrough=1\n"
            "functions-with-fallthrough: 0\n"
            "functions-with-branches:\n"
            "function-summaries:\n"
            "  fn.0 main blocks=2 ops=1 jump=0 fallthrough=1 br=0 brft=0 cmpbr=0 cmpbri=0 cmpbrft=0 cmpbrift=0 ret=0 reti=1 retspill=0\n"
            "    layout.0 bb.0 ops=1 has_term=1 term=3\n"
            "    layout.1 bb.1 ops=0 has_term=1 term=1\n"
            "\n"
            "machine_layout\n"
            "function main params=0 locals=0 spills=0\n"
            "  layout.0 -> bb.0:\n"
            "    store_globali global.0, 9\n"
            "    fallthrough layout.1\n"
            "  layout.1 -> bb.1:\n"
            "    reti 7\n")) {
        fprintf(stderr, "[machine-layout] FAIL: report surface mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    machine_layout_report_free(&report);
    return ok;
}

static int test_machine_layout_rejects_riscv32_preview_incompatible_register_bank(void) {
    MachineLayoutProgram program;
    MachineLayoutError error;
    MachineLayoutTargetPolicySummary policy_summary;
    size_t register_index;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    memset(&policy_summary, 0, sizeof(policy_summary));
    machine_layout_program_init(&program);

    program.register_bank.register_count = 9u;
    program.register_bank.registers =
        (MachineLayoutRegisterDesc *)calloc(9u, sizeof(MachineLayoutRegisterDesc));
    program.function_count = 1u;
    program.function_capacity = 1u;
    program.functions = (MachineLayoutFunction *)calloc(1u, sizeof(MachineLayoutFunction));
    if (!program.register_bank.registers || !program.functions) {
        machine_layout_program_free(&program);
        return 0;
    }
    for (register_index = 0u; register_index < 9u; ++register_index) {
        program.register_bank.registers[register_index].register_id = register_index;
        program.register_bank.registers[register_index].name = dup_text("r");
        program.register_bank.registers[register_index].allocatable = 1u;
        if (!program.register_bank.registers[register_index].name) {
            machine_layout_program_free(&program);
            return 0;
        }
    }

    program.functions[0].name = dup_text("main");
    program.functions[0].has_body = 1;
    program.functions[0].block_count = 1u;
    program.functions[0].block_capacity = 1u;
    program.functions[0].blocks = (MachineLayoutBlock *)calloc(1u, sizeof(MachineLayoutBlock));
    if (!program.functions[0].name || !program.functions[0].blocks) {
        machine_layout_program_free(&program);
        return 0;
    }
    program.functions[0].blocks[0].layout_index = 0u;
    program.functions[0].blocks[0].op_count = 1u;
    program.functions[0].blocks[0].op_capacity = 1u;
    program.functions[0].blocks[0].ops = (MachineLayoutOp *)calloc(1u, sizeof(MachineLayoutOp));
    if (!program.functions[0].blocks[0].ops) {
        machine_layout_program_free(&program);
        return 0;
    }
    program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
    program.functions[0].blocks[0].ops[0].has_result = 1;
    program.functions[0].blocks[0].ops[0].result = machine_select_operand_register(8u);
    program.functions[0].blocks[0].ops[0].as.copy_value = machine_select_operand_immediate(4);
    program.functions[0].blocks[0].has_terminator = 1;
    program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_register(8u);

    if (!machine_layout_program_get_target_policy_summary(&program, &policy_summary) ||
        policy_summary.select_policy.current_riscv32_preview_logical_register_cap != 8u ||
        !policy_summary.preserves_spill_operands_for_later_materialization ||
        !policy_summary.preserves_global_slot_ops_for_later_address_formation ||
        !policy_summary.preserves_fallthrough_terminator_shapes) {
        fprintf(stderr, "[machine-layout] FAIL: target policy summary mismatch\n");
        ok = 0;
    }
    if (machine_layout_verify_current_riscv32_preview_compatibility(&program, &error) ||
        strstr(error.message, "MACHINE-LAYOUT-138") == NULL) {
        fprintf(stderr,
            "[machine-layout] FAIL: oversized riscv32-preview register bank was not rejected: %s\n",
            error.message);
        ok = 0;
    }

    machine_layout_program_free(&program);
    return ok;
}

static int test_machine_layout_rejects_riscv32_preview_bytes_incompatible_branch_range(void) {
    MachineLayoutProgram program;
    MachineLayoutError error;
    size_t op_index;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    machine_layout_program_init(&program);

    program.register_bank.register_count = 1u;
    program.register_bank.registers =
        (MachineLayoutRegisterDesc *)calloc(1u, sizeof(MachineLayoutRegisterDesc));
    program.function_count = 1u;
    program.function_capacity = 1u;
    program.functions = (MachineLayoutFunction *)calloc(1u, sizeof(MachineLayoutFunction));
    if (!program.register_bank.registers || !program.functions) {
        machine_layout_program_free(&program);
        return 0;
    }
    program.register_bank.registers[0].register_id = 0u;
    program.register_bank.registers[0].name = dup_text("r0");
    program.register_bank.registers[0].allocatable = 1u;

    program.functions[0].name = dup_text("main");
    program.functions[0].has_body = 1;
    program.functions[0].block_count = 3u;
    program.functions[0].block_capacity = 3u;
    program.functions[0].blocks = (MachineLayoutBlock *)calloc(3u, sizeof(MachineLayoutBlock));
    if (!program.register_bank.registers[0].name ||
        !program.functions[0].name ||
        !program.functions[0].blocks) {
        machine_layout_program_free(&program);
        return 0;
    }

    program.functions[0].blocks[0].layout_index = 0u;
    program.functions[0].blocks[0].original_block_id = 0u;
    program.functions[0].blocks[0].has_terminator = 1;
    program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_BRANCH;
    program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0u);
    program.functions[0].blocks[0].terminator.as.branch.then_target = 2u;
    program.functions[0].blocks[0].terminator.as.branch.else_target = 1u;

    program.functions[0].blocks[1].layout_index = 1u;
    program.functions[0].blocks[1].original_block_id = 1u;
    program.functions[0].blocks[1].op_count = 513u;
    program.functions[0].blocks[1].op_capacity = 513u;
    program.functions[0].blocks[1].ops = (MachineLayoutOp *)calloc(513u, sizeof(MachineLayoutOp));
    program.functions[0].blocks[1].has_terminator = 1;
    program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(0u);
    if (!program.functions[0].blocks[1].ops) {
        machine_layout_program_free(&program);
        return 0;
    }
    for (op_index = 0u; op_index < 513u; ++op_index) {
        program.functions[0].blocks[1].ops[op_index].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
        program.functions[0].blocks[1].ops[op_index].has_result = 1;
        program.functions[0].blocks[1].ops[op_index].result = machine_select_operand_register(0u);
        program.functions[0].blocks[1].ops[op_index].as.copy_value = machine_select_operand_immediate(5000);
    }

    program.functions[0].blocks[2].layout_index = 2u;
    program.functions[0].blocks[2].original_block_id = 2u;
    program.functions[0].blocks[2].has_terminator = 1;
    program.functions[0].blocks[2].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    program.functions[0].blocks[2].terminator.as.return_value = machine_select_operand_immediate(0u);

    if (machine_layout_verify_current_riscv32_preview_bytes_compatibility(&program, &error) ||
        strstr(error.message, "MACHINE-LAYOUT-140") == NULL ||
        strstr(error.message, "MACHINE-EMIT-140") == NULL ||
        strstr(error.message, "MACHINE-ENCODE-125") == NULL ||
        strstr(error.message, "MACHINE-BYTES-344") == NULL) {
        fprintf(stderr,
            "[machine-layout] FAIL: preview bytes-compatibility branch-range reject mismatch: %s\n",
            error.message);
        ok = 0;
    }

    machine_layout_program_free(&program);
    return ok;
}

static int test_machine_layout_reorders_blocks_to_create_branch_fallthrough(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 3;
    select_program.functions[0].block_capacity = 3;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(3, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(1);

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[2].terminator.as.return_value = machine_select_operand_immediate(2);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: branch reorder lowering failed: %s\n", layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reti 2\n"
        "  layout.2 -> bb.1:\n"
        "    reti 1\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 3 ||
        summary.branch_fallthrough_count != 1 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: branch reorder summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_reorders_blocks_to_create_branch_fallthrough_for_compare_family(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 3;
    select_program.functions[0].block_capacity = 3;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(3, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(1);

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[2].terminator.as.return_value = machine_select_operand_immediate(2);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: compare branch reorder lowering failed: %s\n", layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reti 2\n"
        "  layout.2 -> bb.1:\n"
        "    reti 1\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 3 ||
        summary.compare_branch_imm_fallthrough_count != 1 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: compare branch reorder summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_single_predecessor_fallthrough_chain(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 5;
    select_program.functions[0].block_capacity = 5;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(5, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.branch.then_target = 2;
    select_program.functions[0].blocks[0].terminator.as.branch.else_target = 1;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[2].terminator.as.return_value = machine_select_operand_immediate(2);

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[3].terminator.as.return_value = machine_select_operand_immediate(3);

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[4].terminator.as.jump_target = 3;

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: single-predecessor preference lowering failed: %s\n", layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.1:\n"
        "    jmp layout.3\n"
        "  layout.2 -> bb.2:\n"
        "    reti 2\n"
        "  layout.3 -> bb.3:\n"
        "    reti 3\n"
        "  layout.4 -> bb.4:\n"
        "    jmp layout.3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 5 ||
        summary.branch_fallthrough_count != 1 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 2 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: single-predecessor preference summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_single_predecessor_fallthrough_chain_for_compare_family(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 5;
    select_program.functions[0].block_capacity = 5;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(5, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.then_target = 2;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.else_target = 1;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[2].terminator.as.return_value = machine_select_operand_immediate(2);

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[3].terminator.as.return_value = machine_select_operand_immediate(3);

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[4].terminator.as.jump_target = 3;

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: compare single-predecessor preference lowering failed: %s\n",
            layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.1:\n"
        "    jmp layout.3\n"
        "  layout.2 -> bb.2:\n"
        "    reti 2\n"
        "  layout.3 -> bb.3:\n"
        "    reti 3\n"
        "  layout.4 -> bb.4:\n"
        "    jmp layout.3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 5 ||
        summary.compare_branch_imm_fallthrough_count != 1 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 2 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: compare single-predecessor preference summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_single_predecessor_fallthrough_chain(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge single-predecessor setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge single-predecessor setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(0);

    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(10)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(30)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_immediate(3), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge single-predecessor setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge single-predecessor lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reg.0 = calli sink(0)\n"
        "    brft.t reg.0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reti 2\n"
        "  layout.2 -> bb.1:\n"
        "    reg.0 = calli sink(10)\n"
        "    reg.0 = calli sink(30)\n"
        "    reti 3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 3 ||
        summary.op_count != 3 ||
        summary.branch_fallthrough_count != 1 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 0 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge single-predecessor summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_single_predecessor_fallthrough_chain_for_compare_family(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrInstruction compare_instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare single-predecessor setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare single-predecessor setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(0);

    memset(&compare_instruction, 0, sizeof(compare_instruction));
    compare_instruction.kind = MACHINE_IR_INSTR_BINARY;
    compare_instruction.has_result = 1;
    compare_instruction.result = machine_ir_operand_register(0);
    compare_instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    compare_instruction.as.binary.lhs = machine_ir_operand_register(0);
    compare_instruction.as.binary.rhs = machine_ir_operand_immediate(0);

    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[0], &compare_instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(10)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &compare_instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(30)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_immediate(3), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare single-predecessor setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare single-predecessor lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reg.0 = calli sink(0)\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reti 2\n"
        "  layout.2 -> bb.1:\n"
        "    reg.0 = calli sink(10)\n"
        "    reg.0 = calli sink(30)\n"
        "    reti 3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 3 ||
        summary.op_count != 3 ||
        summary.compare_branch_imm_fallthrough_count != 1 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 0 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare single-predecessor summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_longer_trace_span_when_local_scores_tie(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 6;
    select_program.functions[0].block_capacity = 6;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(6, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.branch.then_target = 2;
    select_program.functions[0].blocks[0].terminator.as.branch.else_target = 1;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[2].terminator.as.jump_target = 4;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[3].terminator.as.jump_target = 5;

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[4].terminator.as.return_value = machine_select_operand_immediate(4);

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[5].terminator.as.return_value = machine_select_operand_immediate(5);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: trace-span preference lowering failed: %s\n", layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.1:\n"
        "    fallthrough layout.2\n"
        "  layout.2 -> bb.3:\n"
        "    fallthrough layout.3\n"
        "  layout.3 -> bb.5:\n"
        "    reti 5\n"
        "  layout.4 -> bb.2:\n"
        "    fallthrough layout.5\n"
        "  layout.5 -> bb.4:\n"
        "    reti 4\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 6 ||
        summary.branch_fallthrough_count != 1 ||
        summary.fallthrough_count != 3 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: trace-span preference summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_longer_trace_span_when_local_scores_tie_for_compare_family(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 6;
    select_program.functions[0].block_capacity = 6;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(6, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.then_target = 2;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.else_target = 1;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[2].terminator.as.jump_target = 4;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[3].terminator.as.jump_target = 5;

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[4].terminator.as.return_value = machine_select_operand_immediate(4);

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[5].terminator.as.return_value = machine_select_operand_immediate(5);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: compare trace-span preference lowering failed: %s\n",
            layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.1:\n"
        "    fallthrough layout.2\n"
        "  layout.2 -> bb.3:\n"
        "    fallthrough layout.3\n"
        "  layout.3 -> bb.5:\n"
        "    reti 5\n"
        "  layout.4 -> bb.2:\n"
        "    fallthrough layout.5\n"
        "  layout.5 -> bb.4:\n"
        "    reti 4\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 6 ||
        summary.compare_branch_imm_fallthrough_count != 1 ||
        summary.fallthrough_count != 3 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: compare trace-span preference summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_longer_trace_span_when_local_scores_tie(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge trace-span tie setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge trace-span tie setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(10);

    if (!machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(0), 3, 4, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(20)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[2], machine_ir_operand_register(0), 5, 6, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_immediate(3), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_immediate(4), &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(50)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[5], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[5], 7, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[6], machine_ir_operand_immediate(6), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[7], machine_ir_operand_immediate(7), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge trace-span tie setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge trace-span tie lowering failed: %s\n", layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reg.0 = calli sink(20)\n"
        "    brft.t reg.0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.6:\n"
        "    reti 6\n"
        "  layout.3 -> bb.5:\n"
        "    reg.0 = calli sink(50)\n"
        "    reti 7\n"
        "  layout.4 -> bb.1:\n"
        "    reg.0 = calli sink(10)\n"
        "    brft.t reg.0, taken=layout.6, fallthrough=layout.5\n"
        "  layout.5 -> bb.4:\n"
        "    reti 4\n"
        "  layout.6 -> bb.3:\n"
        "    reti 3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 7 ||
        summary.op_count != 3 ||
        summary.branch_fallthrough_count != 3 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 0 ||
        summary.return_imm_count != 4) {
        fprintf(stderr, "[machine-layout] FAIL: bridge trace-span tie summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_longer_trace_span_when_local_scores_tie_for_compare_family(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrInstruction compare_instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare trace-span tie setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare trace-span tie setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare trace-span tie setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(10);

    memset(&compare_instruction, 0, sizeof(compare_instruction));
    compare_instruction.kind = MACHINE_IR_INSTR_BINARY;
    compare_instruction.has_result = 1;
    compare_instruction.result = machine_ir_operand_register(0);
    compare_instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    compare_instruction.as.binary.lhs = machine_ir_operand_register(0);
    compare_instruction.as.binary.rhs = machine_ir_operand_immediate(0);

    if (!machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &compare_instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(0), 3, 4, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(20)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[2], &compare_instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[2], machine_ir_operand_register(0), 5, 6, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_immediate(3), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_immediate(4), &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(50)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[5], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[5], 7, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[6], machine_ir_operand_immediate(6), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[7], machine_ir_operand_immediate(7), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare trace-span tie setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare trace-span tie lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reg.0 = calli sink(20)\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.6:\n"
        "    reti 6\n"
        "  layout.3 -> bb.5:\n"
        "    reg.0 = calli sink(50)\n"
        "    reti 7\n"
        "  layout.4 -> bb.1:\n"
        "    reg.0 = calli sink(10)\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.6, fallthrough=layout.5\n"
        "  layout.5 -> bb.4:\n"
        "    reti 4\n"
        "  layout.6 -> bb.3:\n"
        "    reti 3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 7 ||
        summary.op_count != 3 ||
        summary.compare_branch_imm_fallthrough_count != 3 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 0 ||
        summary.return_imm_count != 4) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare trace-span tie summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_ready_continuation_over_attached_unready_trace(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-cont-vs-attached setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-cont-vs-attached setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(10);

    if (!machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(20)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[2], machine_ir_operand_register(0), 5, 4, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(30)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 4, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(40)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[4], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[4], 7, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(60)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[5], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[5], 6, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(70)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[6], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[6], machine_ir_operand_immediate(7), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[7], machine_ir_operand_immediate(8), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-cont-vs-attached setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-cont-vs-attached lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reg.0 = calli sink(20)\n"
        "    brft.f reg.0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.4:\n"
        "    reg.0 = calli sink(60)\n"
        "    reg.0 = calli sink(70)\n"
        "    reti 7\n"
        "  layout.3 -> bb.3:\n"
        "    reg.0 = calli sink(40)\n"
        "    reti 8\n"
        "  layout.4 -> bb.1:\n"
        "    reg.0 = calli sink(10)\n"
        "    reg.0 = calli sink(30)\n"
        "    jmp layout.3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 5 ||
        summary.op_count != 6 ||
        summary.branch_fallthrough_count != 2 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 1 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-cont-vs-attached summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_ready_continuation_over_attached_unready_trace_for_compare_family(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrInstruction compare_instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-cont-vs-attached setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-cont-vs-attached setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-cont-vs-attached setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(10);

    memset(&compare_instruction, 0, sizeof(compare_instruction));
    compare_instruction.kind = MACHINE_IR_INSTR_BINARY;
    compare_instruction.has_result = 1;
    compare_instruction.result = machine_ir_operand_register(0);
    compare_instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    compare_instruction.as.binary.lhs = machine_ir_operand_register(0);
    compare_instruction.as.binary.rhs = machine_ir_operand_immediate(0);

    if (!machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(20)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[2], &compare_instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[2], machine_ir_operand_register(0), 5, 4, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(30)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 4, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(40)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[4], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[4], 7, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(60)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[5], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[5], 6, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(70)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[6], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[6], machine_ir_operand_immediate(7), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[7], machine_ir_operand_immediate(8), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-cont-vs-attached setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-cont-vs-attached lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reg.0 = calli sink(20)\n"
        "    cmpbrift.f.10 reg.0, 0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.4:\n"
        "    reg.0 = calli sink(60)\n"
        "    reg.0 = calli sink(70)\n"
        "    reti 7\n"
        "  layout.3 -> bb.3:\n"
        "    reg.0 = calli sink(40)\n"
        "    reti 8\n"
        "  layout.4 -> bb.1:\n"
        "    reg.0 = calli sink(10)\n"
        "    reg.0 = calli sink(30)\n"
        "    jmp layout.3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 5 ||
        summary.op_count != 6 ||
        summary.compare_branch_imm_fallthrough_count != 2 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 1 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-cont-vs-attached summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_ready_continuation_over_attached_unready_trace(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 9;
    select_program.functions[0].block_capacity = 9;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(9, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[2].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[2].terminator.as.branch.then_target = 5;
    select_program.functions[0].blocks[2].terminator.as.branch.else_target = 4;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[3].terminator.as.jump_target = 4;

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[4].terminator.as.jump_target = 8;

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[5].terminator.as.jump_target = 6;

    select_program.functions[0].blocks[6].id = 6;
    select_program.functions[0].blocks[6].has_terminator = 1;
    select_program.functions[0].blocks[6].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[6].terminator.as.jump_target = 7;

    select_program.functions[0].blocks[7].id = 7;
    select_program.functions[0].blocks[7].has_terminator = 1;
    select_program.functions[0].blocks[7].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[7].terminator.as.return_value = machine_select_operand_immediate(7);

    select_program.functions[0].blocks[8].id = 8;
    select_program.functions[0].blocks[8].has_terminator = 1;
    select_program.functions[0].blocks[8].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[8].terminator.as.return_value = machine_select_operand_immediate(8);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: ready-cont-vs-attached lowering failed: %s\n",
            layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.7, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    brft.f reg.0, taken=layout.5, fallthrough=layout.2\n"
        "  layout.2 -> bb.5:\n"
        "    fallthrough layout.3\n"
        "  layout.3 -> bb.6:\n"
        "    fallthrough layout.4\n"
        "  layout.4 -> bb.7:\n"
        "    reti 7\n"
        "  layout.5 -> bb.4:\n"
        "    fallthrough layout.6\n"
        "  layout.6 -> bb.8:\n"
        "    reti 8\n"
        "  layout.7 -> bb.1:\n"
        "    fallthrough layout.8\n"
        "  layout.8 -> bb.3:\n"
        "    jmp layout.5\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 9 ||
        summary.branch_fallthrough_count != 2 ||
        summary.fallthrough_count != 4 ||
        summary.jump_count != 1 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: ready-cont-vs-attached summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_ready_continuation_over_attached_unready_trace_for_compare_family(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 9;
    select_program.functions[0].block_capacity = 9;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(9, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[2].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[2].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[2].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[2].terminator.as.compare_branch.then_target = 5;
    select_program.functions[0].blocks[2].terminator.as.compare_branch.else_target = 4;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[3].terminator.as.jump_target = 4;

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[4].terminator.as.jump_target = 8;

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[5].terminator.as.jump_target = 6;

    select_program.functions[0].blocks[6].id = 6;
    select_program.functions[0].blocks[6].has_terminator = 1;
    select_program.functions[0].blocks[6].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[6].terminator.as.jump_target = 7;

    select_program.functions[0].blocks[7].id = 7;
    select_program.functions[0].blocks[7].has_terminator = 1;
    select_program.functions[0].blocks[7].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[7].terminator.as.return_value = machine_select_operand_immediate(7);

    select_program.functions[0].blocks[8].id = 8;
    select_program.functions[0].blocks[8].has_terminator = 1;
    select_program.functions[0].blocks[8].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[8].terminator.as.return_value = machine_select_operand_immediate(8);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: compare ready-cont-vs-attached lowering failed: %s\n",
            layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.7, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    cmpbrift.f.10 reg.0, 0, taken=layout.5, fallthrough=layout.2\n"
        "  layout.2 -> bb.5:\n"
        "    fallthrough layout.3\n"
        "  layout.3 -> bb.6:\n"
        "    fallthrough layout.4\n"
        "  layout.4 -> bb.7:\n"
        "    reti 7\n"
        "  layout.5 -> bb.4:\n"
        "    fallthrough layout.6\n"
        "  layout.6 -> bb.8:\n"
        "    reti 8\n"
        "  layout.7 -> bb.1:\n"
        "    fallthrough layout.8\n"
        "  layout.8 -> bb.3:\n"
        "    jmp layout.5\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 9 ||
        summary.compare_branch_imm_fallthrough_count != 2 ||
        summary.fallthrough_count != 4 ||
        summary.jump_count != 1 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: compare ready-cont-vs-attached summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_defers_shared_merge_tail_until_after_branch_arms(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 4;
    select_program.functions[0].block_capacity = 4;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(4, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[2].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[3].terminator.as.return_value = machine_select_operand_immediate(3);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: shared-merge deferral lowering failed: %s\n", layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    jmp layout.3\n"
        "  layout.2 -> bb.1:\n"
        "    fallthrough layout.3\n"
        "  layout.3 -> bb.3:\n"
        "    reti 3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 4 ||
        summary.branch_fallthrough_count != 1 ||
        summary.fallthrough_count != 1 ||
        summary.jump_count != 1 ||
        summary.return_imm_count != 1) {
        fprintf(stderr, "[machine-layout] FAIL: shared-merge deferral summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_defers_shared_merge_tail_for_compare_branch(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 4;
    select_program.functions[0].block_capacity = 4;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(4, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[2].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[3].terminator.as.return_value = machine_select_operand_immediate(3);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: compare shared-merge deferral lowering failed: %s\n", layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    jmp layout.3\n"
        "  layout.2 -> bb.1:\n"
        "    fallthrough layout.3\n"
        "  layout.3 -> bb.3:\n"
        "    reti 3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 4 ||
        summary.compare_branch_imm_fallthrough_count != 1 ||
        summary.fallthrough_count != 1 ||
        summary.jump_count != 1 ||
        summary.return_imm_count != 1) {
        fprintf(stderr, "[machine-layout] FAIL: compare shared-merge deferral summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_ready_merge_seed_over_longer_unready_trace(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 10;
    select_program.functions[0].block_capacity = 10;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(10, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[2].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[2].terminator.as.branch.then_target = 4;
    select_program.functions[0].blocks[2].terminator.as.branch.else_target = 6;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[3].terminator.as.return_value = machine_select_operand_immediate(3);

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[4].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[5].terminator.as.jump_target = 8;

    select_program.functions[0].blocks[6].id = 6;
    select_program.functions[0].blocks[6].has_terminator = 1;
    select_program.functions[0].blocks[6].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[6].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[6].terminator.as.branch.then_target = 5;
    select_program.functions[0].blocks[6].terminator.as.branch.else_target = 7;

    select_program.functions[0].blocks[7].id = 7;
    select_program.functions[0].blocks[7].has_terminator = 1;
    select_program.functions[0].blocks[7].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[7].terminator.as.jump_target = 8;

    select_program.functions[0].blocks[8].id = 8;
    select_program.functions[0].blocks[8].has_terminator = 1;
    select_program.functions[0].blocks[8].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[8].terminator.as.jump_target = 9;

    select_program.functions[0].blocks[9].id = 9;
    select_program.functions[0].blocks[9].has_terminator = 1;
    select_program.functions[0].blocks[9].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[9].terminator.as.return_value = machine_select_operand_immediate(9);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: ready-merge seed lowering failed: %s\n", layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.6, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    brft.t reg.0, taken=layout.5, fallthrough=layout.2\n"
        "  layout.2 -> bb.6:\n"
        "    brft.t reg.0, taken=layout.4, fallthrough=layout.3\n"
        "  layout.3 -> bb.7:\n"
        "    jmp layout.8\n"
        "  layout.4 -> bb.5:\n"
        "    jmp layout.8\n"
        "  layout.5 -> bb.4:\n"
        "    jmp layout.7\n"
        "  layout.6 -> bb.1:\n"
        "    fallthrough layout.7\n"
        "  layout.7 -> bb.3:\n"
        "    reti 3\n"
        "  layout.8 -> bb.8:\n"
        "    fallthrough layout.9\n"
        "  layout.9 -> bb.9:\n"
        "    reti 9\n"
        );

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 10 ||
        summary.branch_fallthrough_count != 3 ||
        summary.fallthrough_count != 2 ||
        summary.jump_count != 3 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: ready-merge seed summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_ready_merge_seed_over_longer_unready_trace_for_compare_family(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 10;
    select_program.functions[0].block_capacity = 10;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(10, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[2].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[2].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[2].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[2].terminator.as.compare_branch.then_target = 4;
    select_program.functions[0].blocks[2].terminator.as.compare_branch.else_target = 6;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[3].terminator.as.return_value = machine_select_operand_immediate(3);

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[4].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[5].terminator.as.jump_target = 8;

    select_program.functions[0].blocks[6].id = 6;
    select_program.functions[0].blocks[6].has_terminator = 1;
    select_program.functions[0].blocks[6].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[6].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[6].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[6].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[6].terminator.as.compare_branch.then_target = 5;
    select_program.functions[0].blocks[6].terminator.as.compare_branch.else_target = 7;

    select_program.functions[0].blocks[7].id = 7;
    select_program.functions[0].blocks[7].has_terminator = 1;
    select_program.functions[0].blocks[7].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[7].terminator.as.jump_target = 8;

    select_program.functions[0].blocks[8].id = 8;
    select_program.functions[0].blocks[8].has_terminator = 1;
    select_program.functions[0].blocks[8].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[8].terminator.as.jump_target = 9;

    select_program.functions[0].blocks[9].id = 9;
    select_program.functions[0].blocks[9].has_terminator = 1;
    select_program.functions[0].blocks[9].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[9].terminator.as.return_value = machine_select_operand_immediate(9);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: compare ready-merge seed lowering failed: %s\n",
            layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.6, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.5, fallthrough=layout.2\n"
        "  layout.2 -> bb.6:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.4, fallthrough=layout.3\n"
        "  layout.3 -> bb.7:\n"
        "    jmp layout.8\n"
        "  layout.4 -> bb.5:\n"
        "    jmp layout.8\n"
        "  layout.5 -> bb.4:\n"
        "    jmp layout.7\n"
        "  layout.6 -> bb.1:\n"
        "    fallthrough layout.7\n"
        "  layout.7 -> bb.3:\n"
        "    reti 3\n"
        "  layout.8 -> bb.8:\n"
        "    fallthrough layout.9\n"
        "  layout.9 -> bb.9:\n"
        "    reti 9\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 10 ||
        summary.compare_branch_imm_fallthrough_count != 3 ||
        summary.fallthrough_count != 2 ||
        summary.jump_count != 3 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: compare ready-merge seed summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_layout_tail_ready_merge_when_multiple_ready_merges_exist(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 9;
    select_program.functions[0].block_capacity = 9;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(9, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.branch.else_target = 4;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[1].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[1].terminator.as.branch.then_target = 3;
    select_program.functions[0].blocks[1].terminator.as.branch.else_target = 2;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[2].terminator.as.jump_target = 8;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[3].terminator.as.jump_target = 8;

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[4].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[4].terminator.as.branch.then_target = 6;
    select_program.functions[0].blocks[4].terminator.as.branch.else_target = 5;

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[5].terminator.as.jump_target = 7;

    select_program.functions[0].blocks[6].id = 6;
    select_program.functions[0].blocks[6].has_terminator = 1;
    select_program.functions[0].blocks[6].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[6].terminator.as.jump_target = 7;

    select_program.functions[0].blocks[7].id = 7;
    select_program.functions[0].blocks[7].has_terminator = 1;
    select_program.functions[0].blocks[7].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[7].terminator.as.return_value = machine_select_operand_immediate(7);

    select_program.functions[0].blocks[8].id = 8;
    select_program.functions[0].blocks[8].has_terminator = 1;
    select_program.functions[0].blocks[8].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[8].terminator.as.return_value = machine_select_operand_immediate(8);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: multi-ready-merge tail preference lowering failed: %s\n",
            layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.4:\n"
        "    brft.t reg.0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.5:\n"
        "    jmp layout.8\n"
        "  layout.3 -> bb.6:\n"
        "    jmp layout.8\n"
        "  layout.4 -> bb.1:\n"
        "    brft.t reg.0, taken=layout.6, fallthrough=layout.5\n"
        "  layout.5 -> bb.2:\n"
        "    jmp layout.7\n"
        "  layout.6 -> bb.3:\n"
        "    fallthrough layout.7\n"
        "  layout.7 -> bb.8:\n"
        "    reti 8\n"
        "  layout.8 -> bb.7:\n"
        "    reti 7\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 9 ||
        summary.branch_fallthrough_count != 3 ||
        summary.fallthrough_count != 1 ||
        summary.jump_count != 3 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: multi-ready-merge tail preference summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_layout_tail_ready_merge_when_multiple_ready_merges_exist_for_compare_family(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 9;
    select_program.functions[0].block_capacity = 9;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(9, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.else_target = 4;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[1].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[1].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[1].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[1].terminator.as.compare_branch.then_target = 3;
    select_program.functions[0].blocks[1].terminator.as.compare_branch.else_target = 2;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[2].terminator.as.jump_target = 8;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[3].terminator.as.jump_target = 8;

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[4].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[4].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[4].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[4].terminator.as.compare_branch.then_target = 6;
    select_program.functions[0].blocks[4].terminator.as.compare_branch.else_target = 5;

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[5].terminator.as.jump_target = 7;

    select_program.functions[0].blocks[6].id = 6;
    select_program.functions[0].blocks[6].has_terminator = 1;
    select_program.functions[0].blocks[6].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[6].terminator.as.jump_target = 7;

    select_program.functions[0].blocks[7].id = 7;
    select_program.functions[0].blocks[7].has_terminator = 1;
    select_program.functions[0].blocks[7].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[7].terminator.as.return_value = machine_select_operand_immediate(7);

    select_program.functions[0].blocks[8].id = 8;
    select_program.functions[0].blocks[8].has_terminator = 1;
    select_program.functions[0].blocks[8].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[8].terminator.as.return_value = machine_select_operand_immediate(8);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: compare multi-ready-merge tail preference lowering failed: %s\n",
            layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.4:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.5:\n"
        "    jmp layout.8\n"
        "  layout.3 -> bb.6:\n"
        "    jmp layout.8\n"
        "  layout.4 -> bb.1:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.6, fallthrough=layout.5\n"
        "  layout.5 -> bb.2:\n"
        "    jmp layout.7\n"
        "  layout.6 -> bb.3:\n"
        "    fallthrough layout.7\n"
        "  layout.7 -> bb.8:\n"
        "    reti 8\n"
        "  layout.8 -> bb.7:\n"
        "    reti 7\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 9 ||
        summary.compare_branch_imm_fallthrough_count != 3 ||
        summary.fallthrough_count != 1 ||
        summary.jump_count != 3 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: compare multi-ready-merge tail preference summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_fresher_ready_continuation_over_older_longer_trace(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 8;
    select_program.functions[0].block_capacity = 8;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(8, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 7;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[2].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[2].terminator.as.branch.then_target = 4;
    select_program.functions[0].blocks[2].terminator.as.branch.else_target = 3;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[3].terminator.as.jump_target = 5;

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[4].terminator.as.return_value = machine_select_operand_immediate(4);

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[5].terminator.as.jump_target = 6;

    select_program.functions[0].blocks[6].id = 6;
    select_program.functions[0].blocks[6].has_terminator = 1;
    select_program.functions[0].blocks[6].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[6].terminator.as.return_value = machine_select_operand_immediate(6);

    select_program.functions[0].blocks[7].id = 7;
    select_program.functions[0].blocks[7].has_terminator = 1;
    select_program.functions[0].blocks[7].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[7].terminator.as.return_value = machine_select_operand_immediate(7);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: ready-continuation freshness lowering failed: %s\n",
            layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.6, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    brft.t reg.0, taken=layout.5, fallthrough=layout.2\n"
        "  layout.2 -> bb.3:\n"
        "    fallthrough layout.3\n"
        "  layout.3 -> bb.5:\n"
        "    fallthrough layout.4\n"
        "  layout.4 -> bb.6:\n"
        "    reti 6\n"
        "  layout.5 -> bb.4:\n"
        "    reti 4\n"
        "  layout.6 -> bb.1:\n"
        "    fallthrough layout.7\n"
        "  layout.7 -> bb.7:\n"
        "    reti 7\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 8 ||
        summary.branch_fallthrough_count != 2 ||
        summary.fallthrough_count != 3 ||
        summary.return_imm_count != 3) {
        fprintf(stderr, "[machine-layout] FAIL: ready-continuation freshness summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_prefers_fresher_ready_continuation_for_compare_family(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 8;
    select_program.functions[0].block_capacity = 8;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(8, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 7;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[2].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[2].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[2].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[2].terminator.as.compare_branch.then_target = 4;
    select_program.functions[0].blocks[2].terminator.as.compare_branch.else_target = 3;

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[3].terminator.as.jump_target = 5;

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[4].terminator.as.return_value = machine_select_operand_immediate(4);

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[5].terminator.as.jump_target = 6;

    select_program.functions[0].blocks[6].id = 6;
    select_program.functions[0].blocks[6].has_terminator = 1;
    select_program.functions[0].blocks[6].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[6].terminator.as.return_value = machine_select_operand_immediate(6);

    select_program.functions[0].blocks[7].id = 7;
    select_program.functions[0].blocks[7].has_terminator = 1;
    select_program.functions[0].blocks[7].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[7].terminator.as.return_value = machine_select_operand_immediate(7);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: compare ready-continuation freshness lowering failed: %s\n",
            layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.6, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.5, fallthrough=layout.2\n"
        "  layout.2 -> bb.3:\n"
        "    fallthrough layout.3\n"
        "  layout.3 -> bb.5:\n"
        "    fallthrough layout.4\n"
        "  layout.4 -> bb.6:\n"
        "    reti 6\n"
        "  layout.5 -> bb.4:\n"
        "    reti 4\n"
        "  layout.6 -> bb.1:\n"
        "    fallthrough layout.7\n"
        "  layout.7 -> bb.7:\n"
        "    reti 7\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 8 ||
        summary.compare_branch_imm_fallthrough_count != 2 ||
        summary.fallthrough_count != 3 ||
        summary.return_imm_count != 3) {
        fprintf(stderr, "[machine-layout] FAIL: compare ready-continuation freshness summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_fresher_ready_continuation_over_older_longer_trace(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

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
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 7, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[2], machine_ir_operand_register(0), 4, 3, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 5, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_immediate(4), &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[5], 6, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[6], machine_ir_operand_immediate(6), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[7], machine_ir_operand_immediate(7), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-continuation freshness setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-continuation freshness lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    brft.t reg.0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.4:\n"
        "    reti 6\n"
        "  layout.3 -> bb.3:\n"
        "    reti 4\n"
        "  layout.4 -> bb.1:\n"
        "    reti 7\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 5 ||
        summary.branch_fallthrough_count != 2 ||
        summary.fallthrough_count != 0 ||
        summary.return_imm_count != 3) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-continuation freshness summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_fresher_ready_continuation_for_compare_family(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

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
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-continuation freshness setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 7, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[2], machine_ir_operand_register(0), 4, 3, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 5, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[4], machine_ir_operand_immediate(4), &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[5], 6, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[6], machine_ir_operand_immediate(6), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[7], machine_ir_operand_immediate(7), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-continuation freshness setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-continuation freshness lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reg.0 = cmpi.10 reg.0, 0\n"
        "    brft.t reg.0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.4:\n"
        "    reti 6\n"
        "  layout.3 -> bb.3:\n"
        "    reti 4\n"
        "  layout.4 -> bb.1:\n"
        "    reti 7\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 5 ||
        summary.op_count != 1 ||
        summary.branch_fallthrough_count != 1 ||
        summary.compare_branch_imm_fallthrough_count != 1 ||
        summary.fallthrough_count != 0 ||
        summary.return_imm_count != 3) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-continuation freshness summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_avoids_counting_immediately_deferred_shared_tail_as_local_trace(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 6;
    select_program.functions[0].block_capacity = 6;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(6, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[2].terminator.as.return_value = machine_select_operand_immediate(2);

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_BRANCH;
    select_program.functions[0].blocks[3].terminator.as.branch.condition = machine_select_operand_register(0);
    select_program.functions[0].blocks[3].terminator.as.branch.then_target = 4;
    select_program.functions[0].blocks[3].terminator.as.branch.else_target = 5;

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[4].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[5].terminator.as.return_value = machine_select_operand_immediate(5);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: deferred-shared-tail local-trace lowering failed: %s\n",
            layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reti 2\n"
        "  layout.2 -> bb.1:\n"
        "    fallthrough layout.3\n"
        "  layout.3 -> bb.3:\n"
        "    brft.t reg.0, taken=layout.5, fallthrough=layout.4\n"
        "  layout.4 -> bb.5:\n"
        "    reti 5\n"
        "  layout.5 -> bb.4:\n"
        "    jmp layout.3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 6 ||
        summary.branch_fallthrough_count != 2 ||
        summary.fallthrough_count != 1 ||
        summary.jump_count != 1 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: deferred-shared-tail local-trace summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_avoids_counting_immediately_deferred_shared_tail_as_local_trace_for_compare_family(void) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&layout_error, 0, sizeof(layout_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);

    select_program.register_bank.register_count = 1;
    select_program.register_bank.registers =
        (MachineSelectRegisterDesc *)calloc(1, sizeof(MachineSelectRegisterDesc));
    select_program.function_count = 1;
    select_program.function_capacity = 1;
    select_program.functions = (MachineSelectFunction *)calloc(1, sizeof(MachineSelectFunction));
    if (!select_program.register_bank.registers || !select_program.functions) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.register_bank.registers[0].register_id = 0;
    select_program.register_bank.registers[0].name = dup_text("r0");
    select_program.register_bank.registers[0].allocatable = 1u;

    select_program.functions[0].name = dup_text("main");
    select_program.functions[0].has_body = 1;
    select_program.functions[0].block_count = 6;
    select_program.functions[0].block_capacity = 6;
    select_program.functions[0].blocks = (MachineSelectBasicBlock *)calloc(6, sizeof(MachineSelectBasicBlock));
    if (!select_program.register_bank.registers[0].name ||
        !select_program.functions[0].name ||
        !select_program.functions[0].blocks) {
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    select_program.functions[0].blocks[0].id = 0;
    select_program.functions[0].blocks[0].has_terminator = 1;
    select_program.functions[0].blocks[0].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[0].terminator.as.compare_branch.then_target = 1;
    select_program.functions[0].blocks[0].terminator.as.compare_branch.else_target = 2;

    select_program.functions[0].blocks[1].id = 1;
    select_program.functions[0].blocks[1].has_terminator = 1;
    select_program.functions[0].blocks[1].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[1].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[2].id = 2;
    select_program.functions[0].blocks[2].has_terminator = 1;
    select_program.functions[0].blocks[2].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[2].terminator.as.return_value = machine_select_operand_immediate(2);

    select_program.functions[0].blocks[3].id = 3;
    select_program.functions[0].blocks[3].has_terminator = 1;
    select_program.functions[0].blocks[3].terminator.kind = MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM;
    select_program.functions[0].blocks[3].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    select_program.functions[0].blocks[3].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    select_program.functions[0].blocks[3].terminator.as.compare_branch.rhs = machine_select_operand_immediate(0);
    select_program.functions[0].blocks[3].terminator.as.compare_branch.then_target = 4;
    select_program.functions[0].blocks[3].terminator.as.compare_branch.else_target = 5;

    select_program.functions[0].blocks[4].id = 4;
    select_program.functions[0].blocks[4].has_terminator = 1;
    select_program.functions[0].blocks[4].terminator.kind = MACHINE_SELECT_TERM_JUMP;
    select_program.functions[0].blocks[4].terminator.as.jump_target = 3;

    select_program.functions[0].blocks[5].id = 5;
    select_program.functions[0].blocks[5].has_terminator = 1;
    select_program.functions[0].blocks[5].terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
    select_program.functions[0].blocks[5].terminator.as.return_value = machine_select_operand_immediate(5);

    if (!machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: compare deferred-shared-tail local-trace lowering failed: %s\n",
            layout_error.message);
        machine_select_program_free(&select_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reti 2\n"
        "  layout.2 -> bb.1:\n"
        "    fallthrough layout.3\n"
        "  layout.3 -> bb.3:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.5, fallthrough=layout.4\n"
        "  layout.4 -> bb.5:\n"
        "    reti 5\n"
        "  layout.5 -> bb.4:\n"
        "    jmp layout.3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[0], &summary) ||
        summary.block_count != 6 ||
        summary.compare_branch_imm_fallthrough_count != 2 ||
        summary.fallthrough_count != 1 ||
        summary.jump_count != 1 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: compare deferred-shared-tail local-trace summary mismatch\n");
        ok = 0;
    }

    machine_select_program_free(&select_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_respects_upstream_canonicalization_of_effectful_shared_tail_shape(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful-shared-tail setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful-shared-tail setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.args = NULL;
    instruction.as.call.arg_count = 0;

    if (!machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[3], machine_ir_operand_register(0), 4, 5, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[4], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[4], 3, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[5], machine_ir_operand_immediate(5), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful-shared-tail setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful-shared-tail lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.f reg.0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.1:\n"
        "    reg.0 = call sink()\n"
        "    brft.f reg.0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.3:\n"
        "    reg.0 = call sink()\n"
        "    brft.t reg.0, taken=layout.2, fallthrough=layout.3\n"
        "  layout.3 -> bb.4:\n"
        "    reti 5\n"
        "  layout.4 -> bb.2:\n"
        "    reti 2\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 5 ||
        summary.op_count != 2 ||
        summary.branch_fallthrough_count != 3 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 0 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful-shared-tail summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_defers_effectful_compare_shared_tail(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful compare-shared-tail setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful compare-shared-tail setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful compare-shared-tail setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(1);

    if (!machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful compare-shared-tail setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(2);
    if (!machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 3, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_immediate(3), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful compare-shared-tail setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful compare-shared-tail lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reg.0 = calli sink(2)\n"
        "    reti 3\n"
        "  layout.2 -> bb.1:\n"
        "    reg.0 = calli sink(1)\n"
        "    reti 3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 3 ||
        summary.op_count != 2 ||
        summary.compare_branch_imm_fallthrough_count != 1 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 0 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge effectful compare-shared-tail summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_fresher_ready_shared_merge_with_effectful_arms(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-shared-merge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-shared-merge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(20);

    if (!machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 4, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(0), 3, 2, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 8, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(30)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 8, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[4], machine_ir_operand_register(0), 6, 5, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(50)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[5], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[5], 7, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(60)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[6], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[6], 7, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-shared-merge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(70);
    if (!machine_ir_block_append_instruction(&function->blocks[7], &instruction, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-shared-merge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(80);
    if (!machine_ir_block_append_instruction(&function->blocks[8], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[7], machine_ir_operand_immediate(7), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[8], machine_ir_operand_immediate(8), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-shared-merge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-shared-merge lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.4:\n"
        "    brft.t reg.0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.5:\n"
        "    reg.0 = calli sink(50)\n"
        "    jmp layout.8\n"
        "  layout.3 -> bb.6:\n"
        "    reg.0 = calli sink(60)\n"
        "    jmp layout.8\n"
        "  layout.4 -> bb.1:\n"
        "    brft.t reg.0, taken=layout.6, fallthrough=layout.5\n"
        "  layout.5 -> bb.2:\n"
        "    reg.0 = calli sink(20)\n"
        "    jmp layout.7\n"
        "  layout.6 -> bb.3:\n"
        "    reg.0 = calli sink(30)\n"
        "    fallthrough layout.7\n"
        "  layout.7 -> bb.8:\n"
        "    reg.0 = calli sink(80)\n"
        "    reti 8\n"
        "  layout.8 -> bb.7:\n"
        "    reg.0 = calli sink(70)\n"
        "    reti 7\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 9 ||
        summary.op_count != 6 ||
        summary.branch_fallthrough_count != 3 ||
        summary.fallthrough_count != 1 ||
        summary.jump_count != 3 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-shared-merge summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_fresher_ready_shared_merge_for_compare_family(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-shared-merge setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-shared-merge setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[4], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-shared-merge setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(20);

    if (!machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 4, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[1], machine_ir_operand_register(0), 3, 2, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[2], 8, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-shared-merge setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(30);
    if (!machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[3], 8, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[4], machine_ir_operand_register(0), 6, 5, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-shared-merge setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(50);
    if (!machine_ir_block_append_instruction(&function->blocks[5], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[5], 7, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-shared-merge setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(60);
    if (!machine_ir_block_append_instruction(&function->blocks[6], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[6], 7, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-shared-merge setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(70);
    if (!machine_ir_block_append_instruction(&function->blocks[7], &instruction, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-shared-merge setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(80);
    if (!machine_ir_block_append_instruction(&function->blocks[8], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[7], machine_ir_operand_immediate(7), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[8], machine_ir_operand_immediate(8), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-shared-merge setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-shared-merge lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reg.0 = cmpi.10 reg.0, 0\n"
        "    brft.t reg.0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.4:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.5:\n"
        "    reg.0 = calli sink(50)\n"
        "    jmp layout.8\n"
        "  layout.3 -> bb.6:\n"
        "    reg.0 = calli sink(60)\n"
        "    jmp layout.8\n"
        "  layout.4 -> bb.1:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.6, fallthrough=layout.5\n"
        "  layout.5 -> bb.2:\n"
        "    reg.0 = calli sink(20)\n"
        "    jmp layout.7\n"
        "  layout.6 -> bb.3:\n"
        "    reg.0 = calli sink(30)\n"
        "    fallthrough layout.7\n"
        "  layout.7 -> bb.8:\n"
        "    reg.0 = calli sink(80)\n"
        "    reti 8\n"
        "  layout.8 -> bb.7:\n"
        "    reg.0 = calli sink(70)\n"
        "    reti 7\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 9 ||
        summary.op_count != 7 ||
        summary.branch_fallthrough_count != 1 ||
        summary.compare_branch_imm_fallthrough_count != 2 ||
        summary.fallthrough_count != 1 ||
        summary.jump_count != 3 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-shared-merge summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_avoids_counting_deferred_shared_tail_as_local_trace(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(0);

    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(10)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    instruction.as.call.args[0] = machine_ir_operand_immediate(70);
    if (!machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[3], machine_ir_operand_register(0), 4, 5, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    instruction.as.call.args[0] = machine_ir_operand_immediate(40);
    if (!machine_ir_block_append_instruction(&function->blocks[4], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[4], 3, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[5], machine_ir_operand_immediate(5), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge local-trace lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reg.0 = calli sink(0)\n"
        "    brft.f reg.0, taken=layout.4, fallthrough=layout.1\n"
        "  layout.1 -> bb.1:\n"
        "    reg.0 = calli sink(10)\n"
        "    reg.0 = calli sink(70)\n"
        "    brft.f reg.0, taken=layout.3, fallthrough=layout.2\n"
        "  layout.2 -> bb.3:\n"
        "    reg.0 = calli sink(40)\n"
        "    reg.0 = calli sink(70)\n"
        "    brft.t reg.0, taken=layout.2, fallthrough=layout.3\n"
        "  layout.3 -> bb.4:\n"
        "    reti 5\n"
        "  layout.4 -> bb.2:\n"
        "    reti 2\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 5 ||
        summary.op_count != 5 ||
        summary.branch_fallthrough_count != 3 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 0 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge local-trace summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_avoids_counting_deferred_compare_shared_tail_as_local_trace(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(10);

    if (!machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    instruction.as.call.args[0] = machine_ir_operand_immediate(70);
    if (!machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[3], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(40);

    if (!machine_ir_block_set_branch(&function->blocks[3], machine_ir_operand_register(0), 4, 5, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[4], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[4], 3, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[5], machine_ir_operand_immediate(5), &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare local-trace setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare local-trace lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.2, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    reti 2\n"
        "  layout.2 -> bb.1:\n"
        "    reg.0 = calli sink(10)\n"
        "    fallthrough layout.3\n"
        "  layout.3 -> bb.3:\n"
        "    reg.0 = calli sink(70)\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.5, fallthrough=layout.4\n"
        "  layout.4 -> bb.5:\n"
        "    reti 5\n"
        "  layout.5 -> bb.4:\n"
        "    reg.0 = calli sink(40)\n"
        "    jmp layout.3\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 6 ||
        summary.op_count != 3 ||
        summary.compare_branch_imm_fallthrough_count != 2 ||
        summary.fallthrough_count != 1 ||
        summary.jump_count != 1 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare local-trace summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_ready_merge_seed_over_longer_unready_trace_with_effectful_tails(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-merge-vs-unready setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-merge-vs-unready setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(1);

    if (!machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[2], machine_ir_operand_register(0), 4, 6, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_immediate(3), &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(4)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[4], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[4], 3, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(5)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[5], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[5], 8, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[6], machine_ir_operand_register(0), 5, 7, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(7)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[7], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[7], 8, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[8], &instruction, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-merge-vs-unready setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[8], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[8], machine_ir_operand_immediate(9), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-merge-vs-unready setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-merge-vs-unready lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    brft.t reg.0, taken=layout.6, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    brft.t reg.0, taken=layout.5, fallthrough=layout.2\n"
        "  layout.2 -> bb.5:\n"
        "    brft.t reg.0, taken=layout.4, fallthrough=layout.3\n"
        "  layout.3 -> bb.6:\n"
        "    reg.0 = calli sink(7)\n"
        "    jmp layout.7\n"
        "  layout.4 -> bb.4:\n"
        "    reg.0 = calli sink(5)\n"
        "    jmp layout.7\n"
        "  layout.5 -> bb.3:\n"
        "    reg.0 = calli sink(4)\n"
        "    reti 3\n"
        "  layout.6 -> bb.1:\n"
        "    reg.0 = calli sink(1)\n"
        "    reti 3\n"
        "  layout.7 -> bb.7:\n"
        "    reg.0 = calli sink(7)\n"
        "    reti 9\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 8 ||
        summary.op_count != 5 ||
        summary.branch_fallthrough_count != 3 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 2 ||
        summary.return_imm_count != 3) {
        fprintf(stderr, "[machine-layout] FAIL: bridge ready-merge-vs-unready summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_layout_bridge_prefers_ready_merge_seed_over_longer_unready_trace_for_compare_family(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *sink_function = NULL;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineLayoutFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&layout_error, 0, sizeof(layout_error));
    machine_ir_program_init(&machine_program);
    machine_layout_program_init(&layout_program);

    machine_program.register_bank.register_count = 1;
    machine_program.register_bank.registers = (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_program.register_bank.registers) {
        return 0;
    }
    machine_program.register_bank.registers[0].register_id = 0;
    machine_program.register_bank.registers[0].name = dup_text("r0");
    machine_program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_program, "sink", 1, &sink_function, &machine_error) ||
        !machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !sink_function ||
        !function ||
        !machine_ir_function_append_block(sink_function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-merge-vs-unready setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    if (!machine_ir_block_set_return(&sink_function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-merge-vs-unready setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[2], &instruction, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[6], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-merge-vs-unready setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.call.callee_name = "sink";
    instruction.as.call.arg_count = 1;
    instruction.as.call.args = (MachineIrOperand *)calloc(1, sizeof(MachineIrOperand));
    if (!instruction.as.call.args) {
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    instruction.as.call.args[0] = machine_ir_operand_immediate(1);

    if (!machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 1, 2, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[1], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[1], 3, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[2], machine_ir_operand_register(0), 4, 6, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[3], machine_ir_operand_immediate(3), &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(4)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[4], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[4], 3, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(5)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[5], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[5], 8, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[6], machine_ir_operand_register(0), 5, 7, &machine_error) ||
        ((instruction.as.call.args[0] = machine_ir_operand_immediate(7)), 0) ||
        !machine_ir_block_append_instruction(&function->blocks[7], &instruction, &machine_error) ||
        !machine_ir_block_set_jump(&function->blocks[7], 8, &machine_error) ||
        !machine_ir_block_append_instruction(&function->blocks[8], &instruction, &machine_error)) {
        free(instruction.as.call.args);
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-merge-vs-unready setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    free(instruction.as.call.args);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_ADD;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(1);
    if (!machine_ir_block_append_instruction(&function->blocks[8], &instruction, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[8], machine_ir_operand_immediate(9), &machine_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-merge-vs-unready setup failed: %s\n",
            machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!machine_layout_lower_program_from_machine_ir(&machine_program, &layout_program, &layout_error)) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-merge-vs-unready lowering failed: %s\n",
            layout_error.message);
        machine_ir_program_free(&machine_program);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    ok = expect_dump(
        &layout_program,
        "machine_layout\n"
        "function sink params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reti 0\n"
        "function main params=0 locals=0 spills=0\n"
        "  layout.0 -> bb.0:\n"
        "    reg.0 = cmpi.10 reg.0, 0\n"
        "    brft.t reg.0, taken=layout.6, fallthrough=layout.1\n"
        "  layout.1 -> bb.2:\n"
        "    brft.t reg.0, taken=layout.5, fallthrough=layout.2\n"
        "  layout.2 -> bb.5:\n"
        "    cmpbrift.t.10 reg.0, 0, taken=layout.4, fallthrough=layout.3\n"
        "  layout.3 -> bb.6:\n"
        "    reg.0 = calli sink(7)\n"
        "    jmp layout.7\n"
        "  layout.4 -> bb.4:\n"
        "    reg.0 = calli sink(5)\n"
        "    jmp layout.7\n"
        "  layout.5 -> bb.3:\n"
        "    reg.0 = calli sink(4)\n"
        "    reti 3\n"
        "  layout.6 -> bb.1:\n"
        "    reg.0 = calli sink(1)\n"
        "    reti 3\n"
        "  layout.7 -> bb.7:\n"
        "    reg.0 = calli sink(7)\n"
        "    reti 9\n");

    if (!machine_layout_function_compute_summary(&layout_program.functions[1], &summary) ||
        summary.block_count != 8 ||
        summary.op_count != 6 ||
        summary.branch_fallthrough_count != 2 ||
        summary.compare_branch_imm_fallthrough_count != 1 ||
        summary.fallthrough_count != 0 ||
        summary.jump_count != 2 ||
        summary.return_imm_count != 3) {
        fprintf(stderr, "[machine-layout] FAIL: bridge compare ready-merge-vs-unready summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_layout_program_free(&layout_program);
    return ok;
}

int main(void) {
    if (!test_machine_layout_lowers_fallthrough_branch_from_machine_ir()) {
        return 1;
    }
    if (!test_machine_layout_lowers_addr_function_from_machine_ir()) {
        return 1;
    }
    if (!test_machine_layout_lowers_void_return_from_machine_select()) {
        return 1;
    }
    if (!test_machine_layout_rejects_load_store_slot_kind_mismatch()) {
        return 1;
    }
    if (!test_machine_layout_rejects_call_result_family_mismatch()) {
        return 1;
    }
    if (!test_machine_layout_rejects_compare_branch_imm_shape_mismatch()) {
        return 1;
    }
    if (!test_machine_layout_bridge_respects_upstream_jump_canonicalization()) {
        return 1;
    }
    if (!test_machine_layout_bridge_reorders_blocks_to_create_branch_fallthrough()) {
        return 1;
    }
    if (!test_machine_layout_bridge_reorders_blocks_to_create_branch_fallthrough_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_lowers_ops_and_fallthrough_from_machine_select()) {
        return 1;
    }
    if (!test_machine_layout_query_surface()) {
        return 1;
    }
    if (!test_machine_layout_report_surface()) {
        return 1;
    }
    if (!test_machine_layout_report_query_dump_rejects_missing_function_table()) {
        return 1;
    }
    if (!test_machine_layout_rejects_riscv32_preview_incompatible_register_bank()) {
        return 1;
    }
    if (!test_machine_layout_rejects_riscv32_preview_bytes_incompatible_branch_range()) {
        return 1;
    }
    if (!test_machine_layout_reorders_blocks_to_create_branch_fallthrough()) {
        return 1;
    }
    if (!test_machine_layout_reorders_blocks_to_create_branch_fallthrough_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_prefers_single_predecessor_fallthrough_chain()) {
        return 1;
    }
    if (!test_machine_layout_prefers_single_predecessor_fallthrough_chain_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_single_predecessor_fallthrough_chain()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_single_predecessor_fallthrough_chain_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_prefers_longer_trace_span_when_local_scores_tie()) {
        return 1;
    }
    if (!test_machine_layout_prefers_longer_trace_span_when_local_scores_tie_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_longer_trace_span_when_local_scores_tie()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_longer_trace_span_when_local_scores_tie_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_ready_continuation_over_attached_unready_trace()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_ready_continuation_over_attached_unready_trace_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_defers_shared_merge_tail_until_after_branch_arms()) {
        return 1;
    }
    if (!test_machine_layout_defers_shared_merge_tail_for_compare_branch()) {
        return 1;
    }
    if (!test_machine_layout_prefers_ready_merge_seed_over_longer_unready_trace()) {
        return 1;
    }
    if (!test_machine_layout_prefers_ready_merge_seed_over_longer_unready_trace_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_prefers_ready_continuation_over_attached_unready_trace()) {
        return 1;
    }
    if (!test_machine_layout_prefers_ready_continuation_over_attached_unready_trace_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_prefers_layout_tail_ready_merge_when_multiple_ready_merges_exist()) {
        return 1;
    }
    if (!test_machine_layout_prefers_layout_tail_ready_merge_when_multiple_ready_merges_exist_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_prefers_fresher_ready_continuation_over_older_longer_trace()) {
        return 1;
    }
    if (!test_machine_layout_prefers_fresher_ready_continuation_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_fresher_ready_continuation_over_older_longer_trace()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_fresher_ready_continuation_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_avoids_counting_immediately_deferred_shared_tail_as_local_trace()) {
        return 1;
    }
    if (!test_machine_layout_avoids_counting_immediately_deferred_shared_tail_as_local_trace_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_bridge_respects_upstream_canonicalization_of_effectful_shared_tail_shape()) {
        return 1;
    }
    if (!test_machine_layout_bridge_defers_effectful_compare_shared_tail()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_fresher_ready_shared_merge_with_effectful_arms()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_fresher_ready_shared_merge_for_compare_family()) {
        return 1;
    }
    if (!test_machine_layout_bridge_avoids_counting_deferred_shared_tail_as_local_trace()) {
        return 1;
    }
    if (!test_machine_layout_bridge_avoids_counting_deferred_compare_shared_tail_as_local_trace()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_ready_merge_seed_over_longer_unready_trace_with_effectful_tails()) {
        return 1;
    }
    if (!test_machine_layout_bridge_prefers_ready_merge_seed_over_longer_unready_trace_for_compare_family()) {
        return 1;
    }
    printf("[machine-layout] PASS\n");
    return 0;
}
