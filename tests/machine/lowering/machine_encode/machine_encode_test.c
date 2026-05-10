#include "machine/encode.h"

#include "machine/emit.h"
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

static int expect_dump(const MachineEncodeProgram *program, const char *expected_text) {
    char *actual_text = NULL;
    MachineEncodeError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_encode_dump_program(program, &actual_text, &error)) {
        fprintf(stderr, "[machine-encode] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-encode] FAIL: dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int build_emit_encode_smoke_program(MachineEmitProgram *program) {
    machine_emit_program_init(program);
    program->register_bank.register_count = 2u;
    program->register_bank.registers = (MachineEmitRegisterDesc *)calloc(2u, sizeof(MachineEmitRegisterDesc));
    program->global_count = 1u;
    program->global_capacity = 1u;
    program->globals = (MachineEmitGlobal *)calloc(1u, sizeof(MachineEmitGlobal));
    program->function_count = 1u;
    program->function_capacity = 1u;
    program->functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!program->register_bank.registers || !program->globals || !program->functions) {
        machine_emit_program_free(program);
        return 0;
    }
    program->register_bank.registers[0].register_id = 0u;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->register_bank.registers[1].register_id = 1u;
    program->register_bank.registers[1].name = dup_text("r1");
    program->register_bank.registers[1].allocatable = 1u;
    program->globals[0].id = 0u;
    program->globals[0].name = dup_text("g");
    program->functions[0].name = dup_text("main");
    program->functions[0].has_body = 1u;
    program->functions[0].parameter_count = 1u;
    program->functions[0].local_count = 1u;
    program->functions[0].spill_slot_count = 0u;
    program->functions[0].block_count = 1u;
    program->functions[0].block_capacity = 1u;
    program->functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!program->register_bank.registers[0].name || !program->register_bank.registers[1].name ||
        !program->globals[0].name || !program->functions[0].name || !program->functions[0].blocks) {
        machine_emit_program_free(program);
        return 0;
    }
    program->functions[0].blocks[0].emit_index = 0u;
    program->functions[0].blocks[0].original_layout_index = 0u;
    program->functions[0].blocks[0].original_block_id = 0u;
    program->functions[0].blocks[0].label_name = dup_text("F0.L0");
    program->functions[0].blocks[0].op_count = 3u;
    program->functions[0].blocks[0].op_capacity = 3u;
    program->functions[0].blocks[0].ops = (MachineEmitOp *)calloc(3u, sizeof(MachineEmitOp));
    if (!program->functions[0].blocks[0].label_name || !program->functions[0].blocks[0].ops) {
        machine_emit_program_free(program);
        return 0;
    }
    program->functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_LOAD_LOCAL;
    program->functions[0].blocks[0].ops[0].has_result = 1u;
    program->functions[0].blocks[0].ops[0].result = machine_select_operand_register(0u);
    program->functions[0].blocks[0].ops[0].as.load_slot = machine_select_slot_local(0u);
    program->functions[0].blocks[0].ops[1].kind = MACHINE_SELECT_OP_ALU_IMM;
    program->functions[0].blocks[0].ops[1].has_result = 1u;
    program->functions[0].blocks[0].ops[1].result = machine_select_operand_register(1u);
    program->functions[0].blocks[0].ops[1].as.binary.op = MACHINE_IR_BINARY_ADD;
    program->functions[0].blocks[0].ops[1].as.binary.lhs = machine_select_operand_register(0u);
    program->functions[0].blocks[0].ops[1].as.binary.rhs = machine_select_operand_immediate(1);
    program->functions[0].blocks[0].ops[2].kind = MACHINE_SELECT_OP_STORE_GLOBAL;
    program->functions[0].blocks[0].ops[2].as.store.slot = machine_select_slot_global(0u);
    program->functions[0].blocks[0].ops[2].as.store.value = machine_select_operand_register(1u);
    program->functions[0].blocks[0].has_terminator = 1u;
    program->functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    program->functions[0].blocks[0].terminator.as.return_value = machine_select_operand_register(1u);
    return 1;
}

static int build_emit_encode_call_program(MachineEmitProgram *program) {
    machine_emit_program_init(program);
    program->function_count = 1u;
    program->function_capacity = 1u;
    program->functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!program->functions) {
        return 0;
    }
    program->functions[0].name = dup_text("main");
    program->functions[0].has_body = 1u;
    program->functions[0].spill_slot_count = 1u;
    program->functions[0].block_count = 1u;
    program->functions[0].block_capacity = 1u;
    program->functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!program->functions[0].name || !program->functions[0].blocks) {
        machine_emit_program_free(program);
        return 0;
    }
    program->functions[0].blocks[0].emit_index = 0u;
    program->functions[0].blocks[0].original_layout_index = 0u;
    program->functions[0].blocks[0].original_block_id = 0u;
    program->functions[0].blocks[0].label_name = dup_text("F0.L0");
    program->functions[0].blocks[0].op_count = 1u;
    program->functions[0].blocks[0].op_capacity = 1u;
    program->functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1u, sizeof(MachineEmitOp));
    if (!program->functions[0].blocks[0].label_name || !program->functions[0].blocks[0].ops) {
        machine_emit_program_free(program);
        return 0;
    }
    program->functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_IMM_SPILL;
    program->functions[0].blocks[0].ops[0].has_result = 1u;
    program->functions[0].blocks[0].ops[0].result = machine_select_operand_spill_slot(0u);
    program->functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("sink");
    program->functions[0].blocks[0].ops[0].as.call.arg_count = 1u;
    program->functions[0].blocks[0].ops[0].as.call.args = (MachineEmitOperand *)calloc(1u, sizeof(MachineEmitOperand));
    if (!program->functions[0].blocks[0].ops[0].as.call.callee_name ||
        !program->functions[0].blocks[0].ops[0].as.call.args) {
        machine_emit_program_free(program);
        return 0;
    }
    program->functions[0].blocks[0].ops[0].as.call.args[0] = machine_select_operand_immediate(7);
    program->functions[0].blocks[0].has_terminator = 1u;
    program->functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    program->functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);
    return 1;
}

static int build_emit_encode_cmpbri_program(MachineEmitProgram *program) {
    machine_emit_program_init(program);
    program->register_bank.register_count = 1u;
    program->register_bank.registers = (MachineEmitRegisterDesc *)calloc(1u, sizeof(MachineEmitRegisterDesc));
    program->function_count = 1u;
    program->function_capacity = 1u;
    program->functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!program->register_bank.registers || !program->functions) {
        machine_emit_program_free(program);
        return 0;
    }
    program->register_bank.registers[0].register_id = 0u;
    program->register_bank.registers[0].name = dup_text("r0");
    program->register_bank.registers[0].allocatable = 1u;
    program->functions[0].name = dup_text("main");
    program->functions[0].has_body = 1u;
    program->functions[0].block_count = 3u;
    program->functions[0].block_capacity = 3u;
    program->functions[0].blocks = (MachineEmitBlock *)calloc(3u, sizeof(MachineEmitBlock));
    if (!program->register_bank.registers[0].name || !program->functions[0].name || !program->functions[0].blocks) {
        machine_emit_program_free(program);
        return 0;
    }
    program->functions[0].blocks[0].emit_index = 0u;
    program->functions[0].blocks[0].original_layout_index = 0u;
    program->functions[0].blocks[0].original_block_id = 0u;
    program->functions[0].blocks[0].label_name = dup_text("F0.L0");
    program->functions[0].blocks[0].has_terminator = 1u;
    program->functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH;
    program->functions[0].blocks[0].terminator.as.compare_branch_fallthrough.branch_on_true = 1u;
    program->functions[0].blocks[0].terminator.as.compare_branch_fallthrough.op = MACHINE_IR_BINARY_LT;
    program->functions[0].blocks[0].terminator.as.compare_branch_fallthrough.lhs = machine_select_operand_register(0u);
    program->functions[0].blocks[0].terminator.as.compare_branch_fallthrough.rhs = machine_select_operand_immediate(4);
    program->functions[0].blocks[0].terminator.as.compare_branch_fallthrough.taken_target = 2u;
    program->functions[0].blocks[0].terminator.as.compare_branch_fallthrough.fallthrough_target = 1u;
    program->functions[0].blocks[1].emit_index = 1u;
    program->functions[0].blocks[1].original_layout_index = 1u;
    program->functions[0].blocks[1].original_block_id = 1u;
    program->functions[0].blocks[1].label_name = dup_text("F0.L1");
    program->functions[0].blocks[1].has_terminator = 1u;
    program->functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    program->functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(1);
    program->functions[0].blocks[2].emit_index = 2u;
    program->functions[0].blocks[2].original_layout_index = 2u;
    program->functions[0].blocks[2].original_block_id = 2u;
    program->functions[0].blocks[2].label_name = dup_text("F0.L2");
    program->functions[0].blocks[2].has_terminator = 1u;
    program->functions[0].blocks[2].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    program->functions[0].blocks[2].terminator.as.return_value = machine_select_operand_immediate(0);
    if (!program->functions[0].blocks[0].label_name || !program->functions[0].blocks[1].label_name ||
        !program->functions[0].blocks[2].label_name) {
        machine_emit_program_free(program);
        return 0;
    }
    return 1;
}

static int test_machine_encode_lowers_offsets_from_machine_emit(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineEncodeError encode_error;
    MachineEncodeFunctionSummary summary;
    const MachineEncodeFunction *function_view = NULL;
    const MachineEncodeBlock *block_view = NULL;
    size_t function_index = 0;
    size_t emit_index = 0;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);

    emit_program.global_count = 1;
    emit_program.global_capacity = 1;
    emit_program.globals = (MachineEmitGlobal *)calloc(1, sizeof(MachineEmitGlobal));
    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.globals || !emit_program.functions) {
        return 0;
    }
    emit_program.globals[0].id = 0;
    emit_program.globals[0].name = dup_text("g");
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 2;
    emit_program.functions[0].block_capacity = 2;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(2, sizeof(MachineEmitBlock));
    if (!emit_program.globals[0].name || !emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1;
    emit_program.functions[0].blocks[0].op_capacity = 1;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1, sizeof(MachineEmitOp));
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
    emit_program.functions[0].blocks[0].ops[0].as.store.slot = machine_select_slot_global(0u);
    emit_program.functions[0].blocks[0].ops[0].as.store.value = machine_select_operand_immediate(9);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_FALLTHROUGH;
    emit_program.functions[0].blocks[0].terminator.as.fallthrough_target = 1;

    emit_program.functions[0].blocks[1].emit_index = 1;
    emit_program.functions[0].blocks[1].original_layout_index = 1;
    emit_program.functions[0].blocks[1].original_block_id = 1;
    emit_program.functions[0].blocks[1].label_name = dup_text("F0.L1");
    emit_program.functions[0].blocks[1].has_terminator = 1;
    emit_program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(7);

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: lower from machine-emit failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }

    ok = expect_dump(
        &encode_program,
        "machine_encode\n"
        "function main params=0 locals=0 spills=0\n"
        "  F0.L0 @0..2 term@1 ; emit.0 -> layout.0 -> bb.0\n"
        "    fallthrough F0.L1 @2\n"
        "  F0.L1 @2..3 term@2 ; emit.1 -> layout.1 -> bb.1\n"
        "    reti 7\n");

    if (!machine_encode_function_compute_summary(&encode_program.functions[0], &summary) ||
        summary.block_count != 2 || summary.unit_count != 3 || summary.op_unit_count != 1 ||
        summary.terminator_unit_count != 2 || summary.jump_count != 1 || summary.return_count != 1) {
        fprintf(stderr, "[machine-encode] FAIL: summary mismatch\n");
        ok = 0;
    }
    if (!machine_encode_program_get_function_by_name(&encode_program, "main", &function_index, &function_view) ||
        function_index != 0 || !function_view ||
        !machine_encode_program_find_block_by_label(&encode_program, "F0.L1", &function_index, &function_view, &block_view) ||
        function_index != 0 || !function_view || !block_view || block_view->start_offset != 2 ||
        !machine_encode_program_find_block_by_function_name_and_offset(
            &encode_program, "main", 2, &function_index, &emit_index, &function_view, &block_view) ||
        function_index != 0 || emit_index != 1 || !function_view || !block_view ||
        strcmp(block_view->label_name, "F0.L1") != 0 ||
        !machine_encode_function_find_block_covering_offset(function_view, 1, &emit_index, &block_view) ||
        emit_index != 0 || !block_view || strcmp(block_view->label_name, "F0.L0") != 0 ||
        !machine_encode_function_find_block_covering_offset(function_view, 2, &emit_index, &block_view) ||
        emit_index != 1 || !block_view || strcmp(block_view->label_name, "F0.L1") != 0 ||
        machine_encode_function_find_block_covering_offset(function_view, 3, NULL, NULL)) {
        fprintf(stderr, "[machine-encode] FAIL: query lookup mismatch\n");
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    return ok;
}

static int test_machine_encode_bridge_from_machine_ir(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineEncodeProgram encode_program;
    MachineEncodeError encode_error;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&encode_error, 0, sizeof(encode_error));
    machine_ir_program_init(&machine_program);
    machine_encode_program_init(&encode_program);

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
        fprintf(stderr, "[machine-encode] FAIL: bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-encode] FAIL: bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_encode_program_free(&encode_program);
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
        fprintf(stderr, "[machine-encode] FAIL: bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }

    if (!machine_encode_lower_program_from_machine_ir(&machine_program, &encode_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: bridge lower failed: %s\n", encode_error.message);
        machine_ir_program_free(&machine_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }

    ok = expect_dump(
        &encode_program,
        "machine_encode\n"
        "function main params=1 locals=1 spills=0\n"
        "  F0.L0 @0..2 term@1 ; emit.0 -> layout.0 -> bb.0\n"
        "    cmpbrift.10 taken=F0.L2 @3 fallthrough=F0.L1 @2\n"
        "  F0.L1 @2..3 term@2 ; emit.1 -> layout.1 -> bb.1\n"
        "    reti 1\n"
        "  F0.L2 @3..4 term@3 ; emit.2 -> layout.2 -> bb.2\n"
        "    reti 2\n");

    machine_ir_program_free(&machine_program);
    machine_encode_program_free(&encode_program);
    return ok;
}

static int test_machine_encode_structured_terminator_targets(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineEncodeReport report;
    MachineEncodeError encode_error;
    MachineEncodeTerminatorSummary terminator_summary;
    const MachineEncodeFunction *function_view = NULL;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_encode_report_init(&report);

    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 5;
    emit_program.functions[0].block_capacity = 5;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(5, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&report);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_BRANCH;
    emit_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_immediate(1);
    emit_program.functions[0].blocks[0].terminator.as.branch.then_target = 1;
    emit_program.functions[0].blocks[0].terminator.as.branch.else_target = 2;

    emit_program.functions[0].blocks[1].emit_index = 1;
    emit_program.functions[0].blocks[1].original_layout_index = 1;
    emit_program.functions[0].blocks[1].original_block_id = 1;
    emit_program.functions[0].blocks[1].label_name = dup_text("F0.L1");
    emit_program.functions[0].blocks[1].has_terminator = 1;
    emit_program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH;
    emit_program.functions[0].blocks[1].terminator.as.compare_branch_fallthrough.op = MACHINE_IR_BINARY_EQ;
    emit_program.functions[0].blocks[1].terminator.as.compare_branch_fallthrough.lhs =
        machine_select_operand_immediate(1);
    emit_program.functions[0].blocks[1].terminator.as.compare_branch_fallthrough.rhs =
        machine_select_operand_immediate(2);
    emit_program.functions[0].blocks[1].terminator.as.compare_branch_fallthrough.taken_target = 3;
    emit_program.functions[0].blocks[1].terminator.as.compare_branch_fallthrough.fallthrough_target = 2;
    emit_program.functions[0].blocks[1].terminator.as.compare_branch_fallthrough.branch_on_true = 1u;

    emit_program.functions[0].blocks[2].emit_index = 2;
    emit_program.functions[0].blocks[2].original_layout_index = 2;
    emit_program.functions[0].blocks[2].original_block_id = 2;
    emit_program.functions[0].blocks[2].label_name = dup_text("F0.L2");
    emit_program.functions[0].blocks[2].has_terminator = 1;
    emit_program.functions[0].blocks[2].terminator.kind = MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM;
    emit_program.functions[0].blocks[2].terminator.as.compare_branch.op = MACHINE_IR_BINARY_EQ;
    emit_program.register_bank.register_count = 1;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(1, sizeof(MachineEmitRegisterDesc));
    if (!emit_program.register_bank.registers) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&report);
        return 0;
    }
    emit_program.register_bank.registers[0].register_id = 0;
    emit_program.register_bank.registers[0].name = dup_text("r0");
    emit_program.register_bank.registers[0].allocatable = 1u;
    if (!emit_program.register_bank.registers[0].name) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&report);
        return 0;
    }
    emit_program.functions[0].blocks[2].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    emit_program.functions[0].blocks[2].terminator.as.compare_branch.rhs = machine_select_operand_immediate(4);
    emit_program.functions[0].blocks[2].terminator.as.compare_branch.then_target = 3;
    emit_program.functions[0].blocks[2].terminator.as.compare_branch.else_target = 4;

    emit_program.functions[0].blocks[3].emit_index = 3;
    emit_program.functions[0].blocks[3].original_layout_index = 3;
    emit_program.functions[0].blocks[3].original_block_id = 3;
    emit_program.functions[0].blocks[3].label_name = dup_text("F0.L3");
    emit_program.functions[0].blocks[3].has_terminator = 1;
    emit_program.functions[0].blocks[3].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[3].terminator.as.return_value = machine_select_operand_immediate(5);

    emit_program.functions[0].blocks[4].emit_index = 4;
    emit_program.functions[0].blocks[4].original_layout_index = 4;
    emit_program.functions[0].blocks[4].original_block_id = 4;
    emit_program.functions[0].blocks[4].label_name = dup_text("F0.L4");
    emit_program.functions[0].blocks[4].has_terminator = 1;
    emit_program.functions[0].blocks[4].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_encode_build_report_from_program(&encode_program, &report, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: terminator-summary setup failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&report);
        return 0;
    }

    ok &= expect_dump(
        &encode_program,
        "machine_encode\n"
        "function main params=0 locals=0 spills=0\n"
        "  F0.L0 @0..1 term@0 ; emit.0 -> layout.0 -> bb.0\n"
        "    br then=F0.L1 @1 else=F0.L2 @2\n"
        "  F0.L1 @1..2 term@1 ; emit.1 -> layout.1 -> bb.1\n"
        "    cmpbrft.10 taken=F0.L3 @3 fallthrough=F0.L2 @2\n"
        "  F0.L2 @2..3 term@2 ; emit.2 -> layout.2 -> bb.2\n"
        "    cmpbri.10 then=F0.L3 @3 else=F0.L4 @4\n"
        "  F0.L3 @3..4 term@3 ; emit.3 -> layout.3 -> bb.3\n"
        "    reti 5\n"
        "  F0.L4 @4..5 term@4 ; emit.4 -> layout.4 -> bb.4\n"
        "    ret\n");

    if (!machine_encode_program_get_function_by_name(&encode_program, "main", NULL, &function_view) || !function_view) {
        fprintf(stderr, "[machine-encode] FAIL: terminator-summary function lookup failed\n");
        ok = 0;
    }
    if (!machine_encode_function_get_block_terminator_summary(function_view, 0, &terminator_summary) ||
        terminator_summary.kind != MACHINE_LAYOUT_TERM_BRANCH ||
        !terminator_summary.has_primary_target || !terminator_summary.has_secondary_target ||
        strcmp(terminator_summary.primary_target_label_name, "F0.L1") != 0 ||
        terminator_summary.primary_target_offset != 1 ||
        strcmp(terminator_summary.secondary_target_label_name, "F0.L2") != 0 ||
        terminator_summary.secondary_target_offset != 2) {
        fprintf(stderr, "[machine-encode] FAIL: branch terminator summary mismatch\n");
        ok = 0;
    }
    if (!machine_encode_function_get_block_terminator_summary(function_view, 1, &terminator_summary) ||
        terminator_summary.kind != MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH ||
        terminator_summary.compare_op != MACHINE_IR_BINARY_EQ ||
        terminator_summary.branch_on_true != 1u ||
        strcmp(terminator_summary.primary_target_label_name, "F0.L3") != 0 ||
        terminator_summary.primary_target_offset != 3 ||
        strcmp(terminator_summary.secondary_target_label_name, "F0.L2") != 0 ||
        terminator_summary.secondary_target_offset != 2) {
        fprintf(stderr, "[machine-encode] FAIL: compare-ft terminator summary mismatch\n");
        ok = 0;
    }
    if (!machine_encode_report_get_block_terminator_summary(&report, 0, 2, &terminator_summary) ||
        terminator_summary.kind != MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM ||
        terminator_summary.compare_op != MACHINE_IR_BINARY_EQ ||
        strcmp(terminator_summary.primary_target_label_name, "F0.L3") != 0 ||
        terminator_summary.primary_target_offset != 3 ||
        strcmp(terminator_summary.secondary_target_label_name, "F0.L4") != 0 ||
        terminator_summary.secondary_target_offset != 4) {
        fprintf(stderr, "[machine-encode] FAIL: report terminator summary mismatch\n");
        ok = 0;
    }
    if (!machine_encode_function_get_block_terminator_summary(function_view, 3, &terminator_summary) ||
        terminator_summary.kind != MACHINE_LAYOUT_TERM_RETURN_IMM ||
        !terminator_summary.has_return_value ||
        terminator_summary.return_value.kind != MACHINE_SELECT_OPERAND_IMMEDIATE ||
        terminator_summary.return_value.immediate != 5) {
        fprintf(stderr, "[machine-encode] FAIL: immediate return terminator summary mismatch\n");
        ok = 0;
    }
    if (!machine_encode_report_get_block_terminator_summary(&report, 0, 4, &terminator_summary) ||
        terminator_summary.kind != MACHINE_LAYOUT_TERM_RETURN ||
        terminator_summary.has_return_value ||
        terminator_summary.return_value.kind != MACHINE_SELECT_OPERAND_NONE) {
        fprintf(stderr, "[machine-encode] FAIL: bare return terminator summary mismatch\n");
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_encode_report_free(&report);
    return ok;
}

static int test_machine_encode_report_surface(void) {
    MachineEmitProgram emit_program;
    MachineEncodeReport report;
    MachineEncodeError encode_error;
    const MachineEncodeProgram *program_view = NULL;
    const MachineEncodeFunction *function_view = NULL;
    const MachineEncodeFunctionSummary *function_summary = NULL;
    const MachineEncodeBlockSummary *block_summary = NULL;
    const size_t *function_indices = NULL;
    size_t function_index = 0;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t total_block_summary_count = 0;
    size_t counted_functions = 0;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    machine_emit_program_init(&emit_program);
    machine_encode_report_init(&report);

    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_encode_report_free(&report);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1;
    emit_program.functions[0].blocks[0].op_capacity = 1;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_encode_report_free(&report);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_VOID_IMM;
    emit_program.functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("helper");
    emit_program.functions[0].blocks[0].ops[0].as.call.arg_count = 1;
    emit_program.functions[0].blocks[0].ops[0].as.call.args = (MachineEmitOperand *)calloc(1, sizeof(MachineEmitOperand));
    if (!emit_program.functions[0].blocks[0].ops[0].as.call.callee_name ||
        !emit_program.functions[0].blocks[0].ops[0].as.call.args) {
        machine_emit_program_free(&emit_program);
        machine_encode_report_free(&report);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].as.call.args[0] = machine_select_operand_immediate(3);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_FALLTHROUGH;
    emit_program.functions[0].blocks[0].terminator.as.fallthrough_target = 0;

    if (!machine_encode_build_report_from_machine_emit_program(&emit_program, &report, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: report build failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_report_free(&report);
        return 0;
    }

    if (!machine_encode_report_get_summary(
            &report, &register_count, &global_count, &function_count, &total_block_summary_count) ||
        register_count != 0 || global_count != 0 || function_count != 1 || total_block_summary_count != 1) {
        fprintf(stderr, "[machine-encode] FAIL: report summary mismatch\n");
        ok = 0;
    }
    if (!machine_encode_report_get_program(&report, &program_view) || !program_view ||
        !machine_encode_report_get_function_by_name(&report, "main", NULL, &function_view) || !function_view) {
        fprintf(stderr, "[machine-encode] FAIL: report program/function lookup mismatch\n");
        ok = 0;
    }
    if (!machine_encode_report_get_function_summary_artifact(&report, 0, &function_summary) || !function_summary ||
        function_summary->block_count != 1 || function_summary->unit_count != 2 ||
        function_summary->call_count != 1 || function_summary->blocks_with_calls_count != 1 ||
        function_summary->fallthrough_count != 1) {
        fprintf(stderr, "[machine-encode] FAIL: report function summary mismatch\n");
        ok = 0;
    }
    if (!machine_encode_report_get_block_summary(&report, 0, 0, &block_summary) || !block_summary ||
        block_summary->start_offset != 0 || block_summary->terminator_offset != 1 ||
        block_summary->end_offset != 2 || !block_summary->label_name ||
        strcmp(block_summary->label_name, "F0.L0") != 0) {
        fprintf(stderr, "[machine-encode] FAIL: report block summary mismatch\n");
        ok = 0;
    }
    if (!machine_encode_report_find_block_summary_by_label(&report, "F0.L0", &function_index, &block_summary) ||
        function_index != 0 || !block_summary || block_summary->end_offset != 2 ||
        !machine_encode_report_find_block_summary_covering_offset(&report, 0, 0, &block_summary) ||
        !block_summary || strcmp(block_summary->label_name, "F0.L0") != 0 ||
        !machine_encode_report_find_block_summary_by_function_name_and_offset(
            &report, "main", 0, &function_index, &block_summary) ||
        function_index != 0 || !block_summary || strcmp(block_summary->label_name, "F0.L0") != 0 ||
        machine_encode_report_find_block_summary_covering_offset(&report, 0, 2, NULL)) {
        fprintf(stderr, "[machine-encode] FAIL: report lookup mismatch\n");
        ok = 0;
    }
    if (!machine_encode_report_get_functions_with_calls(&report, &counted_functions, &function_indices) ||
        counted_functions != 1 || !function_indices || function_indices[0] != 0 ||
        !machine_encode_report_get_functions_with_fallthrough(&report, &counted_functions, &function_indices) ||
        counted_functions != 1 || !function_indices || function_indices[0] != 0 ||
        !machine_encode_report_get_functions_with_branches(&report, &counted_functions, &function_indices) ||
        counted_functions != 0) {
        fprintf(stderr, "[machine-encode] FAIL: report function-index sets mismatch\n");
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_report_free(&report);
    return ok;
}

static int test_machine_encode_clone_and_report_from_program(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineEncodeProgram cloned_program;
    MachineEncodeReport report;
    MachineEncodeError encode_error;
    MachineEncodeTargetPolicySummary program_policy_summary;
    const MachineEncodeTargetPolicySummary *report_policy_summary = NULL;
    char *actual_text = NULL;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&program_policy_summary, 0, sizeof(program_policy_summary));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_encode_program_init(&cloned_program);
    machine_encode_report_init(&report);

    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_program_free(&cloned_program);
        machine_encode_report_free(&report);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(3);

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_encode_clone_program(&encode_program, &cloned_program, &encode_error) ||
        !machine_encode_build_report_from_program(&cloned_program, &report, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: clone/report setup failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_program_free(&cloned_program);
        machine_encode_report_free(&report);
        return 0;
    }

    if (!machine_encode_program_get_target_policy_summary(&cloned_program, &program_policy_summary) ||
        !machine_encode_report_get_target_policy_summary_artifact(&report, &report_policy_summary) ||
        !machine_encode_verify_current_riscv32_preview_compatibility(&cloned_program, &encode_error) ||
        !machine_encode_report_verify_current_riscv32_preview_compatibility(&report, &encode_error) ||
        !machine_encode_verify_current_riscv32_preview_bytes_compatibility(&cloned_program, &encode_error) ||
        !machine_encode_report_verify_current_riscv32_preview_bytes_compatibility(&report, &encode_error) ||
        program_policy_summary.emit_policy.select_policy.current_riscv32_preview_logical_register_cap != 8u ||
        !program_policy_summary.emit_policy.preserves_spill_operands_for_later_materialization ||
        !program_policy_summary.emit_policy.preserves_global_slot_ops_for_later_address_formation ||
        !program_policy_summary.emit_policy.preserves_fallthrough_terminator_shapes ||
        !program_policy_summary.preserves_block_unit_offsets ||
        !program_policy_summary.preserves_fallthrough_terminator_shapes ||
        !report_policy_summary ||
        report_policy_summary->emit_policy.select_policy.current_riscv32_preview_logical_register_cap != 8u ||
        !report_policy_summary->preserves_block_unit_offsets ||
        !report_policy_summary->preserves_fallthrough_terminator_shapes ||
        !machine_encode_build_report_from_program_dump(&cloned_program, &actual_text, &encode_error) || !actual_text ||
        strstr(actual_text, "machine_encode-report call_funcs=0 fallthrough_funcs=0 branch_funcs=0 total_block_summaries=1\n") != actual_text ||
        strstr(actual_text, "target_policy preview_reg_cap=8 preserves_spills=1 preserves_global_slots=1 preserves_fallthrough=1 preserves_offsets=1\n") == NULL ||
        strstr(actual_text, "functions-with-calls:\nfunctions-with-fallthrough:\nfunctions-with-branches:\nfunction-summaries:\n") == NULL ||
        strstr(actual_text, "emit.0 label=F0.L0 layout.0 bb.0 start=0 term=0 end=1\n") == NULL) {
        fprintf(stderr, "[machine-encode] FAIL: clone/report dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }

    free(actual_text);
    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_encode_program_free(&cloned_program);
    machine_encode_report_free(&report);
    return ok;
}

static int test_machine_encode_rejects_riscv32_preview_incompatible_register_bank(void) {
    MachineEncodeProgram program;
    MachineEncodeError error;
    size_t register_index;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    machine_encode_program_init(&program);

    program.register_bank.register_count = 9u;
    program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(9u, sizeof(MachineEmitRegisterDesc));
    program.function_count = 1u;
    program.function_capacity = 1u;
    program.functions = (MachineEncodeFunction *)calloc(1u, sizeof(MachineEncodeFunction));
    if (!program.register_bank.registers || !program.functions) {
        machine_encode_program_free(&program);
        return 0;
    }
    for (register_index = 0u; register_index < 9u; ++register_index) {
        program.register_bank.registers[register_index].register_id = register_index;
        program.register_bank.registers[register_index].name = dup_text("r");
        program.register_bank.registers[register_index].allocatable = 1u;
        if (!program.register_bank.registers[register_index].name) {
            machine_encode_program_free(&program);
            return 0;
        }
    }

    program.functions[0].name = dup_text("main");
    program.functions[0].has_body = 1;
    program.functions[0].block_count = 1u;
    program.functions[0].block_capacity = 1u;
    program.functions[0].blocks = (MachineEncodeBlock *)calloc(1u, sizeof(MachineEncodeBlock));
    if (!program.functions[0].name || !program.functions[0].blocks) {
        machine_encode_program_free(&program);
        return 0;
    }
    program.functions[0].blocks[0].emit_index = 0u;
    program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    program.functions[0].blocks[0].start_offset = 0u;
    program.functions[0].blocks[0].terminator_offset = 1u;
    program.functions[0].blocks[0].end_offset = 2u;
    program.functions[0].blocks[0].span.start_offset = 0u;
    program.functions[0].blocks[0].span.terminator_offset = 1u;
    program.functions[0].blocks[0].span.end_offset = 2u;
    program.functions[0].blocks[0].block.op_count = 1u;
    program.functions[0].blocks[0].block.op_capacity = 1u;
    program.functions[0].blocks[0].block.ops = (MachineEmitOp *)calloc(1u, sizeof(MachineEmitOp));
    if (!program.functions[0].blocks[0].label_name || !program.functions[0].blocks[0].block.ops) {
        machine_encode_program_free(&program);
        return 0;
    }
    program.functions[0].blocks[0].block.ops[0].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
    program.functions[0].blocks[0].block.ops[0].has_result = 1;
    program.functions[0].blocks[0].block.ops[0].result = machine_select_operand_register(8u);
    program.functions[0].blocks[0].block.ops[0].as.copy_value = machine_select_operand_immediate(5);
    program.functions[0].blocks[0].block.has_terminator = 1;
    program.functions[0].blocks[0].block.terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    program.functions[0].blocks[0].block.terminator.as.return_value = machine_select_operand_register(8u);

    if (machine_encode_verify_current_riscv32_preview_compatibility(&program, &error) ||
        strstr(error.message, "MACHINE-ENCODE-122") == NULL) {
        fprintf(stderr,
            "[machine-encode] FAIL: oversized riscv32-preview register bank was not rejected: %s\n",
            error.message);
        ok = 0;
    }

    machine_encode_program_free(&program);
    return ok;
}

static int test_machine_encode_rejects_riscv32_preview_bytes_incompatible_branch_range(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineEncodeReport report;
    MachineEncodeError encode_error;
    size_t op_index;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_encode_report_init(&report);

    emit_program.register_bank.register_count = 1u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(1u, sizeof(MachineEmitRegisterDesc));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.functions) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&report);
        return 0;
    }
    emit_program.register_bank.registers[0].register_id = 0u;
    emit_program.register_bank.registers[0].name = dup_text("r0");
    emit_program.register_bank.registers[0].allocatable = 1u;

    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 3u;
    emit_program.functions[0].block_capacity = 3u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(3u, sizeof(MachineEmitBlock));
    if (!emit_program.register_bank.registers[0].name ||
        !emit_program.functions[0].name ||
        !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&report);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0u;
    emit_program.functions[0].blocks[0].original_layout_index = 0u;
    emit_program.functions[0].blocks[0].original_block_id = 0u;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_BRANCH;
    emit_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0u);
    emit_program.functions[0].blocks[0].terminator.as.branch.then_target = 2u;
    emit_program.functions[0].blocks[0].terminator.as.branch.else_target = 1u;

    emit_program.functions[0].blocks[1].emit_index = 1u;
    emit_program.functions[0].blocks[1].original_layout_index = 1u;
    emit_program.functions[0].blocks[1].original_block_id = 1u;
    emit_program.functions[0].blocks[1].label_name = dup_text("F0.L1");
    emit_program.functions[0].blocks[1].op_count = 513u;
    emit_program.functions[0].blocks[1].op_capacity = 513u;
    emit_program.functions[0].blocks[1].ops = (MachineEmitOp *)calloc(513u, sizeof(MachineEmitOp));
    emit_program.functions[0].blocks[1].has_terminator = 1;
    emit_program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    if (!emit_program.functions[0].blocks[0].label_name ||
        !emit_program.functions[0].blocks[1].label_name ||
        !emit_program.functions[0].blocks[1].ops) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&report);
        return 0;
    }
    for (op_index = 0u; op_index < 513u; ++op_index) {
        emit_program.functions[0].blocks[1].ops[op_index].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
        emit_program.functions[0].blocks[1].ops[op_index].has_result = 1;
        emit_program.functions[0].blocks[1].ops[op_index].result = machine_select_operand_register(0u);
        emit_program.functions[0].blocks[1].ops[op_index].as.copy_value = machine_select_operand_immediate(5000);
    }

    emit_program.functions[0].blocks[2].emit_index = 2u;
    emit_program.functions[0].blocks[2].original_layout_index = 2u;
    emit_program.functions[0].blocks[2].original_block_id = 2u;
    emit_program.functions[0].blocks[2].label_name = dup_text("F0.L2");
    emit_program.functions[0].blocks[2].has_terminator = 1;
    emit_program.functions[0].blocks[2].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    if (!emit_program.functions[0].blocks[2].label_name) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&report);
        return 0;
    }

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_encode_build_report_from_program(&encode_program, &report, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: bytes-compat setup failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&report);
        return 0;
    }

    if (machine_encode_verify_current_riscv32_preview_bytes_compatibility(&encode_program, &encode_error) ||
        strstr(encode_error.message, "MACHINE-ENCODE-125") == NULL ||
        strstr(encode_error.message, "MACHINE-BYTES-344") == NULL) {
        fprintf(stderr,
            "[machine-encode] FAIL: preview bytes-compatibility branch-range reject mismatch: %s\n",
            encode_error.message);
        ok = 0;
    }

    memset(&encode_error, 0, sizeof(encode_error));
    if (machine_encode_report_verify_current_riscv32_preview_bytes_compatibility(&report, &encode_error) ||
        strstr(encode_error.message, "MACHINE-ENCODE-125") == NULL ||
        strstr(encode_error.message, "MACHINE-BYTES-344") == NULL) {
        fprintf(stderr,
            "[machine-encode] FAIL: preview report bytes-compatibility branch-range reject mismatch: %s\n",
            encode_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_encode_report_free(&report);
    return ok;
}

static int test_machine_encode_from_machine_emit_report(void) {
    MachineEmitProgram emit_program;
    MachineEmitLowerReport emit_report;
    MachineEmitError emit_error;
    MachineEncodeProgram encode_program;
    MachineEncodeReport encode_report;
    MachineEncodeError encode_error;
    const MachineEncodeFunction *function_view = NULL;
    const MachineEncodeBlockSummary *block_summary = NULL;
    char *actual_text = NULL;
    int ok = 1;

    memset(&emit_error, 0, sizeof(emit_error));
    memset(&encode_error, 0, sizeof(encode_error));
    machine_emit_program_init(&emit_program);
    machine_emit_lower_report_init(&emit_report);
    machine_encode_program_init(&encode_program);
    machine_encode_report_init(&encode_report);

    emit_program.global_count = 1;
    emit_program.global_capacity = 1;
    emit_program.globals = (MachineEmitGlobal *)calloc(1, sizeof(MachineEmitGlobal));
    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.globals || !emit_program.functions) {
        return 0;
    }
    emit_program.globals[0].id = 0;
    emit_program.globals[0].name = dup_text("g0");
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 2;
    emit_program.functions[0].block_capacity = 2;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(2, sizeof(MachineEmitBlock));
    if (!emit_program.globals[0].name || !emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_emit_lower_report_free(&emit_report);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&encode_report);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1;
    emit_program.functions[0].blocks[0].op_capacity = 1;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1, sizeof(MachineEmitOp));
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
    emit_program.functions[0].blocks[0].ops[0].as.store.slot = machine_select_slot_global(0);
    emit_program.functions[0].blocks[0].ops[0].as.store.value = machine_select_operand_immediate(4);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_FALLTHROUGH;
    emit_program.functions[0].blocks[0].terminator.as.fallthrough_target = 1;

    emit_program.functions[0].blocks[1].emit_index = 1;
    emit_program.functions[0].blocks[1].original_layout_index = 1;
    emit_program.functions[0].blocks[1].original_block_id = 1;
    emit_program.functions[0].blocks[1].label_name = dup_text("F0.L1");
    emit_program.functions[0].blocks[1].has_terminator = 1;
    emit_program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(9);

    if (!machine_emit_build_report_from_program(&emit_program, &emit_report, &emit_error)) {
        fprintf(stderr, "[machine-encode] FAIL: emit-report build failed: %s\n", emit_error.message);
        machine_emit_program_free(&emit_program);
        machine_emit_lower_report_free(&emit_report);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&encode_report);
        return 0;
    }
    if (!machine_encode_lower_program_from_machine_emit_report(&emit_report, &encode_program, &encode_error) ||
        !machine_encode_build_report_from_machine_emit_report(&emit_report, &encode_report, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: emit-report bridge setup failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_emit_lower_report_free(&emit_report);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&encode_report);
        return 0;
    }

    ok &= expect_dump(
        &encode_program,
        "machine_encode\n"
        "function main params=0 locals=0 spills=0\n"
        "  F0.L0 @0..2 term@1 ; emit.0 -> layout.0 -> bb.0\n"
        "    fallthrough F0.L1 @2\n"
        "  F0.L1 @2..3 term@2 ; emit.1 -> layout.1 -> bb.1\n"
        "    reti 9\n");

    if (!machine_encode_report_get_function_by_name(&encode_report, "main", NULL, &function_view) || !function_view ||
        !machine_encode_report_find_block_summary_by_function_name_and_offset(
            &encode_report, "main", 2, NULL, &block_summary) ||
        !block_summary || strcmp(block_summary->label_name, "F0.L1") != 0) {
        fprintf(stderr, "[machine-encode] FAIL: emit-report bridge query mismatch\n");
        ok = 0;
    }

    if (!machine_encode_build_report_from_machine_emit_report_dump(&emit_report, &actual_text, &encode_error) ||
        !actual_text ||
        strstr(actual_text, "machine_encode-report call_funcs=0 fallthrough_funcs=1 branch_funcs=0 total_block_summaries=2\n") != actual_text ||
        strstr(actual_text, "target_policy preview_reg_cap=8 preserves_spills=1 preserves_global_slots=1 preserves_fallthrough=1 preserves_offsets=1\n") == NULL ||
        strstr(actual_text, "emit.1 label=F0.L1 layout.1 bb.1 start=2 term=2 end=3\n") == NULL) {
        fprintf(stderr, "[machine-encode] FAIL: emit-report bridge dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }

    free(actual_text);
    machine_emit_program_free(&emit_program);
    machine_emit_lower_report_free(&emit_report);
    machine_encode_program_free(&encode_program);
    machine_encode_report_free(&encode_report);
    return ok;
}

static int test_machine_encode_from_machine_ir_report(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineEncodeProgram encode_program;
    MachineEncodeReport encode_report;
    MachineEncodeError encode_error;
    const MachineEncodeFunction *function_view = NULL;
    const MachineEncodeBlockSummary *block_summary = NULL;
    const MachineEncodeFunctionSummary *function_summary = NULL;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&encode_error, 0, sizeof(encode_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_encode_program_init(&encode_program);
    machine_encode_report_init(&encode_report);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-encode] FAIL: machine-ir report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&encode_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-encode] FAIL: machine-ir report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&encode_report);
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
        fprintf(stderr, "[machine-encode] FAIL: machine-ir report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&encode_report);
        return 0;
    }

    if (!machine_encode_lower_program_from_machine_ir_report(&machine_report, &encode_program, &encode_error) ||
        !machine_encode_build_report_from_machine_ir_report(&machine_report, &encode_report, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: machine-ir report bridge failed: %s\n", encode_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&encode_report);
        return 0;
    }

    ok &= expect_dump(
        &encode_program,
        "machine_encode\n"
        "function main params=1 locals=1 spills=0\n"
        "  F0.L0 @0..2 term@1 ; emit.0 -> layout.0 -> bb.0\n"
        "    cmpbrift.10 taken=F0.L2 @3 fallthrough=F0.L1 @2\n"
        "  F0.L1 @2..3 term@2 ; emit.1 -> layout.1 -> bb.1\n"
        "    reti 1\n"
        "  F0.L2 @3..4 term@3 ; emit.2 -> layout.2 -> bb.2\n"
        "    reti 2\n");

    if (!machine_encode_report_get_function_by_name(&encode_report, "main", NULL, &function_view) || !function_view ||
        !machine_encode_report_get_function_summary_artifact(&encode_report, 0, &function_summary) ||
        !function_summary || function_summary->compare_branch_count != 1 || function_summary->return_count != 2 ||
        !machine_encode_report_find_block_summary_by_label(&encode_report, "F0.L2", NULL, &block_summary) ||
        !block_summary || block_summary->start_offset != 3) {
        fprintf(stderr, "[machine-encode] FAIL: machine-ir report bridge query mismatch\n");
        ok = 0;
    }

    if (!machine_encode_build_report_from_machine_ir_report_dump(&machine_report, &actual_text, &encode_error) ||
        !actual_text ||
        strstr(actual_text, "machine_encode-report call_funcs=0 fallthrough_funcs=1 branch_funcs=1 total_block_summaries=3\n") != actual_text ||
        strstr(actual_text, "emit.2 label=F0.L2 layout.2 bb.2 start=3 term=3 end=4\n") == NULL ||
        strstr(actual_text, "cmpbrift.10 taken=F0.L2 @3 fallthrough=F0.L1 @2\n") == NULL) {
        fprintf(stderr, "[machine-encode] FAIL: machine-ir report bridge dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_encode_program_free(&encode_program);
    machine_encode_report_free(&encode_report);
    return ok;
}

static int test_machine_encode_verifier_rejects_bad_offsets(void) {
    MachineEncodeProgram program;
    MachineEncodeError error;

    memset(&error, 0, sizeof(error));
    machine_encode_program_init(&program);

    program.function_count = 1;
    program.function_capacity = 1;
    program.functions = (MachineEncodeFunction *)calloc(1, sizeof(MachineEncodeFunction));
    if (!program.functions) {
        return 0;
    }
    program.functions[0].name = dup_text("main");
    program.functions[0].has_body = 1;
    program.functions[0].block_count = 1;
    program.functions[0].block_capacity = 1;
    program.functions[0].blocks = (MachineEncodeBlock *)calloc(1, sizeof(MachineEncodeBlock));
    if (!program.functions[0].name || !program.functions[0].blocks) {
        machine_encode_program_free(&program);
        return 0;
    }

    program.functions[0].blocks[0].emit_index = 0;
    program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    program.functions[0].blocks[0].start_offset = 1;
    program.functions[0].blocks[0].terminator_offset = 1;
    program.functions[0].blocks[0].end_offset = 2;
    program.functions[0].blocks[0].span.start_offset = 1;
    program.functions[0].blocks[0].span.terminator_offset = 1;
    program.functions[0].blocks[0].span.end_offset = 2;
    if (!program.functions[0].blocks[0].label_name) {
        machine_encode_program_free(&program);
        return 0;
    }
    program.functions[0].blocks[0].block.has_terminator = 1;
    program.functions[0].blocks[0].block.terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;

    if (machine_encode_verify_program(&program, &error)) {
        fprintf(stderr, "[machine-encode] FAIL: verifier accepted bad offsets\n");
        machine_encode_program_free(&program);
        return 0;
    }

    machine_encode_program_free(&program);
    return 1;
}

static int test_machine_encode_verifier_rejects_bad_targets(void) {
    MachineEncodeProgram program;
    MachineEncodeError error;

    memset(&error, 0, sizeof(error));
    machine_encode_program_init(&program);

    program.function_count = 1;
    program.function_capacity = 1;
    program.functions = (MachineEncodeFunction *)calloc(1, sizeof(MachineEncodeFunction));
    if (!program.functions) {
        return 0;
    }
    program.functions[0].name = dup_text("main");
    program.functions[0].has_body = 1;
    program.functions[0].block_count = 1;
    program.functions[0].block_capacity = 1;
    program.functions[0].blocks = (MachineEncodeBlock *)calloc(1, sizeof(MachineEncodeBlock));
    if (!program.functions[0].name || !program.functions[0].blocks) {
        machine_encode_program_free(&program);
        return 0;
    }

    program.functions[0].blocks[0].emit_index = 0;
    program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    program.functions[0].blocks[0].start_offset = 0;
    program.functions[0].blocks[0].terminator_offset = 0;
    program.functions[0].blocks[0].end_offset = 1;
    program.functions[0].blocks[0].span.start_offset = 0;
    program.functions[0].blocks[0].span.terminator_offset = 0;
    program.functions[0].blocks[0].span.end_offset = 1;
    if (!program.functions[0].blocks[0].label_name) {
        machine_encode_program_free(&program);
        return 0;
    }
    program.functions[0].blocks[0].block.has_terminator = 1;
    program.functions[0].blocks[0].block.terminator.kind = MACHINE_LAYOUT_TERM_JUMP;
    program.functions[0].blocks[0].block.terminator.as.jump_target = 1;

    if (machine_encode_verify_program(&program, &error)) {
        fprintf(stderr, "[machine-encode] FAIL: verifier accepted bad target\n");
        machine_encode_program_free(&program);
        return 0;
    }

    machine_encode_program_free(&program);
    return 1;
}

static int test_machine_encode_verifier_enforces_return_shapes(void) {
    MachineEncodeProgram program;
    MachineEncodeError error;

    memset(&error, 0, sizeof(error));
    machine_encode_program_init(&program);

    program.register_bank.register_count = 1;
    program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(1, sizeof(MachineEmitRegisterDesc));
    program.function_count = 1;
    program.function_capacity = 1;
    program.functions = (MachineEncodeFunction *)calloc(1, sizeof(MachineEncodeFunction));
    if (!program.register_bank.registers || !program.functions) {
        machine_encode_program_free(&program);
        return 0;
    }
    program.functions[0].name = dup_text("main");
    program.functions[0].has_body = 1;
    program.functions[0].spill_slot_count = 1;
    program.functions[0].block_count = 1;
    program.functions[0].block_capacity = 1;
    program.functions[0].blocks = (MachineEncodeBlock *)calloc(1, sizeof(MachineEncodeBlock));
    if (!program.functions[0].name || !program.functions[0].blocks) {
        machine_encode_program_free(&program);
        return 0;
    }

    program.functions[0].blocks[0].emit_index = 0;
    program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    program.functions[0].blocks[0].start_offset = 0;
    program.functions[0].blocks[0].terminator_offset = 0;
    program.functions[0].blocks[0].end_offset = 1;
    program.functions[0].blocks[0].span.start_offset = 0;
    program.functions[0].blocks[0].span.terminator_offset = 0;
    program.functions[0].blocks[0].span.end_offset = 1;
    if (!program.functions[0].blocks[0].label_name) {
        machine_encode_program_free(&program);
        return 0;
    }
    program.functions[0].blocks[0].block.has_terminator = 1;

    program.functions[0].blocks[0].block.terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    program.functions[0].blocks[0].block.terminator.as.return_value = machine_select_operand_none();
    if (!machine_encode_verify_program(&program, &error)) {
        fprintf(stderr, "[machine-encode] FAIL: verifier rejected void return: %s\n", error.message);
        machine_encode_program_free(&program);
        return 0;
    }

    memset(&error, 0, sizeof(error));
    program.functions[0].blocks[0].block.terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    program.functions[0].blocks[0].block.terminator.as.return_value = machine_select_operand_none();
    if (machine_encode_verify_program(&program, &error) || strstr(error.message, "MACHINE-ENCODE-142") == NULL) {
        fprintf(stderr,
            "[machine-encode] FAIL: verifier accepted malformed immediate return: %s\n",
            error.message);
        machine_encode_program_free(&program);
        return 0;
    }

    memset(&error, 0, sizeof(error));
    program.functions[0].blocks[0].block.terminator.kind = MACHINE_LAYOUT_TERM_RETURN_SPILL;
    program.functions[0].blocks[0].block.terminator.as.return_value = machine_select_operand_spill_slot(1);
    if (machine_encode_verify_program(&program, &error) || strstr(error.message, "MACHINE-ENCODE-143") == NULL) {
        fprintf(stderr,
            "[machine-encode] FAIL: verifier accepted out-of-range spill return: %s\n",
            error.message);
        machine_encode_program_free(&program);
        return 0;
    }

    machine_encode_program_free(&program);
    return 1;
}

static int test_machine_encode_rejects_load_store_slot_kind_mismatch(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineEncodeProgram mutated_program;
    MachineEncodeError encode_error;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_encode_program_init(&mutated_program);

    if (!build_emit_encode_smoke_program(&emit_program) ||
        !machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: slot-kind mismatch setup failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&mutated_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }
    if (!machine_encode_clone_program(&encode_program, &mutated_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: slot-kind mismatch clone failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&mutated_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }
    mutated_program.functions[0].blocks[0].block.ops[0].as.load_slot = machine_select_slot_global(0u);
    memset(&encode_error, 0, sizeof(encode_error));
    if (machine_encode_verify_program(&mutated_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: verifier accepted load-local/global-slot mismatch\n");
        ok = 0;
    }

    machine_encode_program_free(&mutated_program);
    machine_encode_program_init(&mutated_program);
    if (!machine_encode_clone_program(&encode_program, &mutated_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: store-slot mismatch clone failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&mutated_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }
    mutated_program.functions[0].blocks[0].block.ops[2].as.store.slot = machine_select_slot_local(0u);
    memset(&encode_error, 0, sizeof(encode_error));
    if (machine_encode_verify_program(&mutated_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: verifier accepted store-global/local-slot mismatch\n");
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&mutated_program);
    machine_encode_program_free(&encode_program);
    return ok;
}

static int test_machine_encode_rejects_call_result_family_mismatch(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineEncodeProgram mutated_program;
    MachineEncodeError encode_error;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_encode_program_init(&mutated_program);

    if (!build_emit_encode_call_program(&emit_program) ||
        !machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: call-result mismatch setup failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&mutated_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }
    if (!machine_encode_clone_program(&encode_program, &mutated_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: call-result mismatch clone failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&mutated_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }
    mutated_program.functions[0].blocks[0].block.ops[0].kind = MACHINE_SELECT_OP_CALL_IMM;
    memset(&encode_error, 0, sizeof(encode_error));
    if (machine_encode_verify_program(&mutated_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: verifier accepted register-result call family mismatch\n");
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&mutated_program);
    machine_encode_program_free(&encode_program);
    return ok;
}

static int test_machine_encode_rejects_compare_branch_imm_shape_mismatch(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineEncodeProgram mutated_program;
    MachineEncodeError encode_error;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_encode_program_init(&mutated_program);

    if (!build_emit_encode_cmpbri_program(&emit_program) ||
        !machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: cmpbri mismatch setup failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&mutated_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }
    if (!machine_encode_clone_program(&encode_program, &mutated_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: cmpbri mismatch clone failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&mutated_program);
        machine_encode_program_free(&encode_program);
        return 0;
    }
    mutated_program.functions[0].blocks[0].block.terminator.as.compare_branch_fallthrough.rhs =
        machine_select_operand_register(0u);
    memset(&encode_error, 0, sizeof(encode_error));
    if (machine_encode_verify_program(&mutated_program, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: verifier accepted compare-branch-imm rhs non-immediate mismatch\n");
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&mutated_program);
    machine_encode_program_free(&encode_program);
    return ok;
}

static int test_machine_encode_report_query_dump_rejects_missing_function_table(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineEncodeReport report;
    MachineEncodeError encode_error;
    const MachineEncodeFunction *function_view = NULL;
    char *actual_text = NULL;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_encode_report_init(&report);

    if (!build_emit_encode_smoke_program(&emit_program) ||
        !machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_encode_build_report_from_program(&encode_program, &report, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: malformed report setup failed: %s\n", encode_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_encode_report_free(&report);
        return 0;
    }

    free(report.program.functions);
    report.program.functions = NULL;
    memset(&encode_error, 0, sizeof(encode_error));
    if (machine_encode_report_get_function_by_name(&report, "main", NULL, &function_view) ||
        machine_encode_dump_report(&report, &actual_text, &encode_error)) {
        fprintf(stderr, "[machine-encode] FAIL: malformed report query/dump should fail on missing function table\n");
        ok = 0;
    }

    free(actual_text);
    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_encode_report_free(&report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_encode_lowers_offsets_from_machine_emit();
    ok &= test_machine_encode_bridge_from_machine_ir();
    ok &= test_machine_encode_structured_terminator_targets();
    ok &= test_machine_encode_report_surface();
    ok &= test_machine_encode_clone_and_report_from_program();
    ok &= test_machine_encode_from_machine_emit_report();
    ok &= test_machine_encode_from_machine_ir_report();
    ok &= test_machine_encode_verifier_rejects_bad_offsets();
    ok &= test_machine_encode_verifier_rejects_bad_targets();
    ok &= test_machine_encode_verifier_enforces_return_shapes();
    ok &= test_machine_encode_rejects_load_store_slot_kind_mismatch();
    ok &= test_machine_encode_rejects_call_result_family_mismatch();
    ok &= test_machine_encode_rejects_compare_branch_imm_shape_mismatch();
    ok &= test_machine_encode_report_query_dump_rejects_missing_function_table();
    ok &= test_machine_encode_rejects_riscv32_preview_incompatible_register_bank();
    ok &= test_machine_encode_rejects_riscv32_preview_bytes_incompatible_branch_range();

    if (!ok) {
        return 1;
    }

    printf("[machine-encode] PASS\n");
    return 0;
}
