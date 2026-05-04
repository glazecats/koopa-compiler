#include "machine/emit.h"

#include "machine/ir.h"
#include "machine/layout.h"

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

static int expect_dump(const MachineEmitProgram *program, const char *expected_text) {
    char *actual_text = NULL;
    MachineEmitError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_emit_dump_program(program, &actual_text, &error)) {
        fprintf(stderr, "[machine-emit] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-emit] FAIL: dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int test_machine_emit_lowers_labels_from_machine_layout(void) {
    MachineLayoutProgram layout_program;
    MachineEmitProgram emit_program;
    MachineEmitError emit_error;
    MachineEmitFunctionSummary summary;
    int ok = 1;

    memset(&emit_error, 0, sizeof(emit_error));
    machine_layout_program_init(&layout_program);
    machine_emit_program_init(&emit_program);

    layout_program.global_count = 1;
    layout_program.global_capacity = 1;
    layout_program.globals = (MachineLayoutGlobal *)calloc(1, sizeof(MachineLayoutGlobal));
    layout_program.function_count = 1;
    layout_program.function_capacity = 1;
    layout_program.functions = (MachineLayoutFunction *)calloc(1, sizeof(MachineLayoutFunction));
    if (!layout_program.globals || !layout_program.functions) {
        machine_layout_program_free(&layout_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }

    layout_program.globals[0].id = 0;
    layout_program.globals[0].name = dup_text("g");

    layout_program.functions[0].name = dup_text("main");
    layout_program.functions[0].has_body = 1;
    layout_program.functions[0].block_count = 2;
    layout_program.functions[0].block_capacity = 2;
    layout_program.functions[0].blocks = (MachineLayoutBlock *)calloc(2, sizeof(MachineLayoutBlock));
    if (!layout_program.globals[0].name ||
        !layout_program.functions[0].name ||
        !layout_program.functions[0].blocks) {
        machine_layout_program_free(&layout_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }

    layout_program.functions[0].blocks[0].layout_index = 0;
    layout_program.functions[0].blocks[0].original_block_id = 0;
    layout_program.functions[0].blocks[0].op_count = 1;
    layout_program.functions[0].blocks[0].op_capacity = 1;
    layout_program.functions[0].blocks[0].ops = (MachineLayoutOp *)calloc(1, sizeof(MachineLayoutOp));
    if (!layout_program.functions[0].blocks[0].ops) {
        machine_layout_program_free(&layout_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }
    layout_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
    layout_program.functions[0].blocks[0].ops[0].as.store.slot = machine_select_slot_global(0);
    layout_program.functions[0].blocks[0].ops[0].as.store.value = machine_select_operand_immediate(9);
    layout_program.functions[0].blocks[0].has_terminator = 1;
    layout_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_FALLTHROUGH;
    layout_program.functions[0].blocks[0].terminator.as.fallthrough_target = 1;

    layout_program.functions[0].blocks[1].layout_index = 1;
    layout_program.functions[0].blocks[1].original_block_id = 1;
    layout_program.functions[0].blocks[1].has_terminator = 1;
    layout_program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    layout_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(7);

    if (!machine_emit_lower_program_from_machine_layout(&layout_program, &emit_program, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: layout lowering failed: %s\n", emit_error.message);
        machine_layout_program_free(&layout_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }

    ok = expect_dump(
        &emit_program,
        "machine_emit\n"
        "function main params=0 locals=0 spills=0\n"
        "  F0.L0: ; emit.0 -> layout.0 -> bb.0\n"
        "    store_globali global.0, 9\n"
        "    fallthrough F0.L1\n"
        "  F0.L1: ; emit.1 -> layout.1 -> bb.1\n"
        "    reti 7\n");

    if (!machine_emit_function_compute_summary(&emit_program.functions[0], &summary) ||
        summary.block_count != 2 ||
        summary.op_count != 1 ||
        summary.fallthrough_count != 1 ||
        summary.return_imm_count != 1) {
        fprintf(stderr, "[machine-emit] FAIL: layout summary mismatch\n");
        ok = 0;
    }

    machine_layout_program_free(&layout_program);
    machine_emit_program_free(&emit_program);
    return ok;
}

static int test_machine_emit_preserves_void_return_shape(void) {
    MachineLayoutProgram layout_program;
    MachineEmitProgram emit_program;
    MachineEmitError emit_error;
    int ok = 1;

    memset(&emit_error, 0, sizeof(emit_error));
    machine_layout_program_init(&layout_program);
    machine_emit_program_init(&emit_program);

    layout_program.function_count = 1;
    layout_program.function_capacity = 1;
    layout_program.functions = (MachineLayoutFunction *)calloc(1, sizeof(MachineLayoutFunction));
    if (!layout_program.functions) {
        return 0;
    }

    layout_program.functions[0].name = dup_text("main");
    layout_program.functions[0].has_body = 1;
    layout_program.functions[0].block_count = 1;
    layout_program.functions[0].block_capacity = 1;
    layout_program.functions[0].blocks = (MachineLayoutBlock *)calloc(1, sizeof(MachineLayoutBlock));
    if (!layout_program.functions[0].name || !layout_program.functions[0].blocks) {
        machine_layout_program_free(&layout_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }

    layout_program.functions[0].blocks[0].layout_index = 0;
    layout_program.functions[0].blocks[0].original_block_id = 0;
    layout_program.functions[0].blocks[0].has_terminator = 1;
    layout_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    layout_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_none();

    if (!machine_emit_lower_program_from_machine_layout(&layout_program, &emit_program, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: void-return lower failed: %s\n", emit_error.message);
        machine_layout_program_free(&layout_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }

    ok = expect_dump(
        &emit_program,
        "machine_emit\n"
        "function main params=0 locals=0 spills=0\n"
        "  F0.L0: ; emit.0 -> layout.0 -> bb.0\n"
        "    ret\n");

    machine_layout_program_free(&layout_program);
    machine_emit_program_free(&emit_program);
    return ok;
}

static int test_machine_emit_summary_surface(void) {
    MachineLayoutProgram layout_program;
    MachineEmitProgram emit_program;
    MachineEmitError emit_error;
    const MachineEmitFunction *function = NULL;
    const MachineEmitFunction *function_by_name = NULL;
    const MachineEmitBlock *block = NULL;
    const MachineEmitBlock *label_block = NULL;
    const MachineEmitBlock *program_label_block = NULL;
    const MachineEmitFunction *program_label_function = NULL;
    MachineEmitBlockSummary block_summary;
    const char *name = NULL;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t function_index = 0;
    size_t program_label_function_index = 0;
    size_t block_count = 0;
    size_t spill_slot_count = 0;
    size_t block_emit_index = 0;
    int has_body = 0;
    int ok = 1;

    memset(&emit_error, 0, sizeof(emit_error));
    machine_layout_program_init(&layout_program);
    machine_emit_program_init(&emit_program);

    layout_program.register_bank.register_count = 1;
    layout_program.register_bank.registers = (MachineLayoutRegisterDesc *)calloc(1, sizeof(MachineLayoutRegisterDesc));
    layout_program.global_count = 1;
    layout_program.global_capacity = 1;
    layout_program.globals = (MachineLayoutGlobal *)calloc(1, sizeof(MachineLayoutGlobal));
    layout_program.function_count = 1;
    layout_program.function_capacity = 1;
    layout_program.functions = (MachineLayoutFunction *)calloc(1, sizeof(MachineLayoutFunction));
    if (!layout_program.register_bank.registers || !layout_program.globals || !layout_program.functions) {
        machine_layout_program_free(&layout_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }
    layout_program.register_bank.registers[0].register_id = 0;
    layout_program.register_bank.registers[0].name = dup_text("r0");
    layout_program.register_bank.registers[0].allocatable = 1u;
    layout_program.globals[0].id = 0;
    layout_program.globals[0].name = dup_text("g");
    layout_program.functions[0].name = dup_text("main");
    layout_program.functions[0].has_body = 1;
    layout_program.functions[0].parameter_count = 1;
    layout_program.functions[0].local_count = 1;
    layout_program.functions[0].spill_slot_count = 0;
    layout_program.functions[0].block_count = 1;
    layout_program.functions[0].block_capacity = 1;
    layout_program.functions[0].blocks = (MachineLayoutBlock *)calloc(1, sizeof(MachineLayoutBlock));
    if (!layout_program.register_bank.registers[0].name ||
        !layout_program.globals[0].name ||
        !layout_program.functions[0].name ||
        !layout_program.functions[0].blocks) {
        machine_layout_program_free(&layout_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }
    layout_program.functions[0].blocks[0].layout_index = 0;
    layout_program.functions[0].blocks[0].original_block_id = 0;
    layout_program.functions[0].blocks[0].has_terminator = 1;
    layout_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    layout_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(5);

    if (!machine_emit_lower_program_from_machine_layout(&layout_program, &emit_program, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: summary setup failed: %s\n", emit_error.message);
        machine_layout_program_free(&layout_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }

    if (!machine_emit_program_get_summary(&emit_program, &register_count, &global_count, &function_count) ||
        register_count != 1 || global_count != 1 || function_count != 1) {
        fprintf(stderr, "[machine-emit] FAIL: program summary mismatch\n");
        ok = 0;
    }
    if (!machine_emit_program_get_function(&emit_program, 0, &function) ||
        !machine_emit_function_get_summary(
            function, &name, &has_body, NULL, NULL, &block_count, &spill_slot_count) ||
        !name || strcmp(name, "main") != 0 || !has_body || block_count != 1 || spill_slot_count != 0) {
        fprintf(stderr, "[machine-emit] FAIL: function summary mismatch\n");
        ok = 0;
    }
    if (!machine_emit_program_get_function_by_name(&emit_program, "main", &function_index, &function_by_name) ||
        function_index != 0 || function_by_name != function) {
        fprintf(stderr, "[machine-emit] FAIL: function lookup by name mismatch\n");
        ok = 0;
    }
    if (!machine_emit_function_get_block(function, 0, &block) || !block) {
        fprintf(stderr, "[machine-emit] FAIL: block lookup mismatch\n");
        ok = 0;
    }
    if (!machine_emit_function_find_block_by_label(function, "F0.L0", &block_emit_index, &label_block) ||
        block_emit_index != 0 || label_block != block) {
        fprintf(stderr, "[machine-emit] FAIL: block lookup by label mismatch\n");
        ok = 0;
    }
    if (!machine_emit_program_find_block_by_label(
            &emit_program, "F0.L0", &program_label_function_index, &program_label_function, &program_label_block) ||
        program_label_function_index != 0 || program_label_function != function || program_label_block != block) {
        fprintf(stderr, "[machine-emit] FAIL: program block lookup by label mismatch\n");
        ok = 0;
    }
    if (!machine_emit_block_get_summary(block, &block_summary) ||
        block_summary.emit_index != 0 ||
        block_summary.original_layout_index != 0 ||
        block_summary.original_block_id != 0 ||
        !block_summary.label_name ||
        strcmp(block_summary.label_name, "F0.L0") != 0 ||
        block_summary.op_count != 0 ||
        !block_summary.has_terminator ||
        block_summary.terminator_kind != MACHINE_LAYOUT_TERM_RETURN_IMM) {
        fprintf(stderr, "[machine-emit] FAIL: block summary mismatch\n");
        ok = 0;
    }

    machine_layout_program_free(&layout_program);
    machine_emit_program_free(&emit_program);
    return ok;
}

static int test_machine_emit_bridge_surfaces_labels_from_machine_ir(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineEmitProgram emit_program;
    MachineEmitError emit_error;
    MachineEmitFunctionSummary summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&emit_error, 0, sizeof(emit_error));
    machine_ir_program_init(&machine_program);
    machine_emit_program_init(&emit_program);

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
        fprintf(stderr, "[machine-emit] FAIL: bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-emit] FAIL: bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_emit_program_free(&emit_program);
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
        fprintf(stderr, "[machine-emit] FAIL: bridge setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }

    if (!machine_emit_lower_program_from_machine_ir(&machine_program, &emit_program, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: bridge lowering failed: %s\n", emit_error.message);
        machine_ir_program_free(&machine_program);
        machine_emit_program_free(&emit_program);
        return 0;
    }

    ok = expect_dump(
        &emit_program,
        "machine_emit\n"
        "function main params=1 locals=1 spills=0\n"
        "  F0.L0: ; emit.0 -> layout.0 -> bb.0\n"
        "    reg.0 = load_local local.0\n"
        "    cmpbrift.t.10 reg.0, 0, taken=F0.L2, fallthrough=F0.L1\n"
        "  F0.L1: ; emit.1 -> layout.1 -> bb.1\n"
        "    reti 1\n"
        "  F0.L2: ; emit.2 -> layout.2 -> bb.2\n"
        "    reti 2\n");

    if (!machine_emit_function_compute_summary(&emit_program.functions[0], &summary) ||
        summary.block_count != 3 ||
        summary.op_count != 1 ||
        summary.compare_branch_imm_fallthrough_count != 1 ||
        summary.return_imm_count != 2) {
        fprintf(stderr, "[machine-emit] FAIL: bridge summary mismatch\n");
        ok = 0;
    }

    machine_ir_program_free(&machine_program);
    machine_emit_program_free(&emit_program);
    return ok;
}

static int test_machine_emit_lower_dump_from_machine_ir(void) {
    MachineIrProgram machine_program;
    MachineIrFunction *function = NULL;
    MachineIrError machine_error;
    MachineEmitError emit_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&emit_error, 0, sizeof(emit_error));
    machine_ir_program_init(&machine_program);

    if (!machine_ir_program_append_function(&machine_program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(1), &machine_error)) {
        fprintf(stderr, "[machine-emit] FAIL: lower-dump setup failed: %s\n", machine_error.message);
        machine_ir_program_free(&machine_program);
        return 0;
    }

    if (!machine_emit_lower_dump_from_machine_ir(&machine_program, &actual_text, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: lower-dump from machine-ir failed: %s\n", emit_error.message);
        machine_ir_program_free(&machine_program);
        return 0;
    }
    if (!actual_text ||
        strcmp(actual_text,
            "machine_emit\n"
            "function main params=0 locals=0 spills=0\n"
            "  F0.L0: ; emit.0 -> layout.0 -> bb.0\n"
            "    reti 1\n") != 0) {
        fprintf(stderr, "[machine-emit] FAIL: lower-dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    machine_ir_program_free(&machine_program);
    return ok;
}

static int test_machine_emit_report_surface_from_machine_layout(void) {
    MachineLayoutProgram layout_program;
    MachineEmitLowerReport report;
    MachineEmitError emit_error;
    MachineEmitTargetPolicySummary program_policy_summary;
    const MachineEmitTargetPolicySummary *report_policy_summary = NULL;
    const MachineEmitProgram *program_view = NULL;
    const MachineEmitFunction *function_view = NULL;
    const MachineEmitFunctionShapeSummary *function_shape = NULL;
    const MachineEmitBlockShapeSummary *block_shape = NULL;
    const MachineEmitBlockShapeSummary *label_block_shape = NULL;
    const size_t *function_indices = NULL;
    size_t register_count = 0;
    size_t global_count = 0;
    size_t function_count = 0;
    size_t total_block_summary_count = 0;
    size_t counted_functions = 0;
    size_t label_function_index = 0;
    int ok = 1;

    memset(&emit_error, 0, sizeof(emit_error));
    memset(&program_policy_summary, 0, sizeof(program_policy_summary));
    machine_layout_program_init(&layout_program);
    machine_emit_lower_report_init(&report);

    layout_program.function_count = 1;
    layout_program.function_capacity = 1;
    layout_program.functions = (MachineLayoutFunction *)calloc(1, sizeof(MachineLayoutFunction));
    if (!layout_program.functions) {
        machine_emit_lower_report_free(&report);
        return 0;
    }

    layout_program.functions[0].name = dup_text("main");
    layout_program.functions[0].has_body = 1;
    layout_program.functions[0].block_count = 2;
    layout_program.functions[0].block_capacity = 2;
    layout_program.functions[0].blocks = (MachineLayoutBlock *)calloc(2, sizeof(MachineLayoutBlock));
    if (!layout_program.functions[0].name || !layout_program.functions[0].blocks) {
        machine_layout_program_free(&layout_program);
        machine_emit_lower_report_free(&report);
        return 0;
    }

    layout_program.functions[0].blocks[0].layout_index = 0;
    layout_program.functions[0].blocks[0].original_block_id = 0;
    layout_program.functions[0].blocks[0].op_count = 1;
    layout_program.functions[0].blocks[0].op_capacity = 1;
    layout_program.functions[0].blocks[0].ops = (MachineLayoutOp *)calloc(1, sizeof(MachineLayoutOp));
    if (!layout_program.functions[0].blocks[0].ops) {
        machine_layout_program_free(&layout_program);
        machine_emit_lower_report_free(&report);
        return 0;
    }
    layout_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_VOID;
    layout_program.functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("foo");
    if (!layout_program.functions[0].blocks[0].ops[0].as.call.callee_name) {
        machine_layout_program_free(&layout_program);
        machine_emit_lower_report_free(&report);
        return 0;
    }
    layout_program.functions[0].blocks[0].has_terminator = 1;
    layout_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_FALLTHROUGH;
    layout_program.functions[0].blocks[0].terminator.as.fallthrough_target = 1;

    layout_program.functions[0].blocks[1].layout_index = 1;
    layout_program.functions[0].blocks[1].original_block_id = 1;
    layout_program.functions[0].blocks[1].has_terminator = 1;
    layout_program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    layout_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(7);

    if (!machine_emit_build_report_from_machine_layout_program(&layout_program, &report, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: layout report build failed: %s\n", emit_error.message);
        machine_layout_program_free(&layout_program);
        machine_emit_lower_report_free(&report);
        return 0;
    }

    if (!machine_emit_lower_report_get_summary(
            &report, &register_count, &global_count, &function_count, &total_block_summary_count) ||
        register_count != 0 || global_count != 0 || function_count != 1 || total_block_summary_count != 2) {
        fprintf(stderr, "[machine-emit] FAIL: report summary mismatch\n");
        ok = 0;
    }
    if (!machine_emit_lower_report_get_program(&report, &program_view) || !program_view ||
        !machine_emit_program_get_target_policy_summary(&report.program, &program_policy_summary) ||
        !machine_emit_lower_report_get_target_policy_summary_artifact(&report, &report_policy_summary) ||
        !machine_emit_verify_current_riscv32_preview_compatibility(&report.program, &emit_error) ||
        !machine_emit_lower_report_verify_current_riscv32_preview_compatibility(&report, &emit_error) ||
        program_policy_summary.select_policy.current_riscv32_preview_logical_register_cap != 8u ||
        !program_policy_summary.preserves_spill_operands_for_later_materialization ||
        !program_policy_summary.preserves_global_slot_ops_for_later_address_formation ||
        !program_policy_summary.preserves_fallthrough_terminator_shapes ||
        !report_policy_summary ||
        report_policy_summary->select_policy.current_riscv32_preview_logical_register_cap != 8u ||
        !report_policy_summary->preserves_spill_operands_for_later_materialization ||
        !report_policy_summary->preserves_global_slot_ops_for_later_address_formation ||
        !report_policy_summary->preserves_fallthrough_terminator_shapes ||
        !machine_emit_lower_report_get_function_by_name(&report, "main", NULL, &function_view) || !function_view) {
        fprintf(stderr, "[machine-emit] FAIL: report program/function lookup mismatch\n");
        ok = 0;
    }
    if (!machine_emit_lower_report_get_function_shape_summary(&report, 0, &function_shape) || !function_shape ||
        function_shape->block_count != 2 || function_shape->op_count != 1 || function_shape->call_count != 1 ||
        function_shape->blocks_with_calls_count != 1 || function_shape->fallthrough_count != 1 ||
        function_shape->return_imm_count != 1) {
        fprintf(stderr, "[machine-emit] FAIL: function shape summary mismatch\n");
        ok = 0;
    }
    if (!machine_emit_lower_report_get_block_shape_summary(&report, 0, 0, &block_shape) || !block_shape ||
        block_shape->emit_index != 0 || block_shape->original_layout_index != 0 ||
        block_shape->original_block_id != 0 || !block_shape->label_name ||
        strcmp(block_shape->label_name, "F0.L0") != 0 || block_shape->op_count != 1 || block_shape->call_count != 1 ||
        !block_shape->has_terminator || block_shape->terminator_kind != MACHINE_LAYOUT_TERM_FALLTHROUGH) {
        fprintf(stderr, "[machine-emit] FAIL: block shape summary mismatch\n");
        ok = 0;
    }
    if (!machine_emit_lower_report_find_block_shape_by_label(&report, "F0.L0", &label_function_index, &label_block_shape) ||
        label_function_index != 0 || label_block_shape != block_shape) {
        fprintf(stderr, "[machine-emit] FAIL: report block shape by label mismatch\n");
        ok = 0;
    }
    if (!machine_emit_lower_report_get_functions_with_calls(&report, &counted_functions, &function_indices) ||
        counted_functions != 1 || !function_indices || function_indices[0] != 0) {
        fprintf(stderr, "[machine-emit] FAIL: functions-with-calls mismatch\n");
        ok = 0;
    }
    if (!machine_emit_lower_report_get_functions_with_fallthrough(&report, &counted_functions, &function_indices) ||
        counted_functions != 1 || !function_indices || function_indices[0] != 0) {
        fprintf(stderr, "[machine-emit] FAIL: functions-with-fallthrough mismatch\n");
        ok = 0;
    }
    if (!machine_emit_lower_report_get_functions_with_branches(&report, &counted_functions, &function_indices) ||
        counted_functions != 0) {
        fprintf(stderr, "[machine-emit] FAIL: functions-with-branches mismatch\n");
        ok = 0;
    }

    machine_layout_program_free(&layout_program);
    machine_emit_lower_report_free(&report);
    return ok;
}

static int test_machine_emit_rejects_riscv32_preview_incompatible_register_bank(void) {
    MachineEmitProgram program;
    MachineEmitError error;
    size_t register_index;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    machine_emit_program_init(&program);

    program.register_bank.register_count = 9u;
    program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(9u, sizeof(MachineEmitRegisterDesc));
    program.function_count = 1u;
    program.function_capacity = 1u;
    program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!program.register_bank.registers || !program.functions) {
        machine_emit_program_free(&program);
        return 0;
    }
    for (register_index = 0u; register_index < 9u; ++register_index) {
        program.register_bank.registers[register_index].register_id = register_index;
        program.register_bank.registers[register_index].name = dup_text("r");
        program.register_bank.registers[register_index].allocatable = 1u;
        if (!program.register_bank.registers[register_index].name) {
            machine_emit_program_free(&program);
            return 0;
        }
    }

    program.functions[0].name = dup_text("main");
    program.functions[0].has_body = 1;
    program.functions[0].block_count = 1u;
    program.functions[0].block_capacity = 1u;
    program.functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!program.functions[0].name || !program.functions[0].blocks) {
        machine_emit_program_free(&program);
        return 0;
    }
    program.functions[0].blocks[0].emit_index = 0u;
    program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    program.functions[0].blocks[0].op_count = 1u;
    program.functions[0].blocks[0].op_capacity = 1u;
    program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1u, sizeof(MachineEmitOp));
    if (!program.functions[0].blocks[0].label_name || !program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&program);
        return 0;
    }
    program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
    program.functions[0].blocks[0].ops[0].has_result = 1;
    program.functions[0].blocks[0].ops[0].result = machine_select_operand_register(8u);
    program.functions[0].blocks[0].ops[0].as.copy_value = machine_select_operand_immediate(3);
    program.functions[0].blocks[0].has_terminator = 1;
    program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_register(8u);

    if (machine_emit_verify_current_riscv32_preview_compatibility(&program, &error) ||
        strstr(error.message, "MACHINE-EMIT-137") == NULL) {
        fprintf(stderr,
            "[machine-emit] FAIL: oversized riscv32-preview register bank was not rejected: %s\n",
            error.message);
        ok = 0;
    }

    machine_emit_program_free(&program);
    return ok;
}

static int test_machine_emit_rejects_riscv32_preview_bytes_incompatible_branch_range(void) {
    MachineEmitProgram program;
    MachineEmitError error;
    size_t op_index;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    machine_emit_program_init(&program);

    program.register_bank.register_count = 1u;
    program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(1u, sizeof(MachineEmitRegisterDesc));
    program.function_count = 1u;
    program.function_capacity = 1u;
    program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!program.register_bank.registers || !program.functions) {
        machine_emit_program_free(&program);
        return 0;
    }
    program.register_bank.registers[0].register_id = 0u;
    program.register_bank.registers[0].name = dup_text("r0");
    program.register_bank.registers[0].allocatable = 1u;

    program.functions[0].name = dup_text("main");
    program.functions[0].has_body = 1;
    program.functions[0].block_count = 3u;
    program.functions[0].block_capacity = 3u;
    program.functions[0].blocks = (MachineEmitBlock *)calloc(3u, sizeof(MachineEmitBlock));
    if (!program.register_bank.registers[0].name ||
        !program.functions[0].name ||
        !program.functions[0].blocks) {
        machine_emit_program_free(&program);
        return 0;
    }

    program.functions[0].blocks[0].emit_index = 0u;
    program.functions[0].blocks[0].original_layout_index = 0u;
    program.functions[0].blocks[0].original_block_id = 0u;
    program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    program.functions[0].blocks[0].has_terminator = 1;
    program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_BRANCH;
    program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0u);
    program.functions[0].blocks[0].terminator.as.branch.then_target = 2u;
    program.functions[0].blocks[0].terminator.as.branch.else_target = 1u;

    program.functions[0].blocks[1].emit_index = 1u;
    program.functions[0].blocks[1].original_layout_index = 1u;
    program.functions[0].blocks[1].original_block_id = 1u;
    program.functions[0].blocks[1].label_name = dup_text("F0.L1");
    program.functions[0].blocks[1].op_count = 513u;
    program.functions[0].blocks[1].op_capacity = 513u;
    program.functions[0].blocks[1].ops = (MachineEmitOp *)calloc(513u, sizeof(MachineEmitOp));
    program.functions[0].blocks[1].has_terminator = 1;
    program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(0u);
    if (!program.functions[0].blocks[0].label_name ||
        !program.functions[0].blocks[1].label_name ||
        !program.functions[0].blocks[1].ops) {
        machine_emit_program_free(&program);
        return 0;
    }
    for (op_index = 0u; op_index < 513u; ++op_index) {
        program.functions[0].blocks[1].ops[op_index].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
        program.functions[0].blocks[1].ops[op_index].has_result = 1;
        program.functions[0].blocks[1].ops[op_index].result = machine_select_operand_register(0u);
        program.functions[0].blocks[1].ops[op_index].as.copy_value = machine_select_operand_immediate(5000);
    }

    program.functions[0].blocks[2].emit_index = 2u;
    program.functions[0].blocks[2].original_layout_index = 2u;
    program.functions[0].blocks[2].original_block_id = 2u;
    program.functions[0].blocks[2].label_name = dup_text("F0.L2");
    program.functions[0].blocks[2].has_terminator = 1;
    program.functions[0].blocks[2].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    program.functions[0].blocks[2].terminator.as.return_value = machine_select_operand_immediate(0u);
    if (!program.functions[0].blocks[2].label_name) {
        machine_emit_program_free(&program);
        return 0;
    }

    if (machine_emit_verify_current_riscv32_preview_bytes_compatibility(&program, &error) ||
        strstr(error.message, "MACHINE-EMIT-140") == NULL ||
        strstr(error.message, "MACHINE-ENCODE-125") == NULL ||
        strstr(error.message, "MACHINE-BYTES-344") == NULL) {
        fprintf(stderr,
            "[machine-emit] FAIL: preview bytes-compatibility branch-range reject mismatch: %s\n",
            error.message);
        ok = 0;
    }

    machine_emit_program_free(&program);
    return ok;
}

static int test_machine_emit_clone_and_report_from_program(void) {
    MachineLayoutProgram layout_program;
    MachineEmitProgram emit_program;
    MachineEmitProgram cloned_program;
    MachineEmitLowerReport report;
    MachineEmitError emit_error;
    const MachineEmitFunctionShapeSummary *function_shape = NULL;
    const MachineEmitBlockShapeSummary *block_shape = NULL;
    char *actual_text = NULL;
    int ok = 1;

    memset(&emit_error, 0, sizeof(emit_error));
    machine_layout_program_init(&layout_program);
    machine_emit_program_init(&emit_program);
    machine_emit_program_init(&cloned_program);
    machine_emit_lower_report_init(&report);

    layout_program.function_count = 1;
    layout_program.function_capacity = 1;
    layout_program.functions = (MachineLayoutFunction *)calloc(1, sizeof(MachineLayoutFunction));
    if (!layout_program.functions) {
        machine_emit_program_free(&cloned_program);
        machine_emit_lower_report_free(&report);
        return 0;
    }

    layout_program.functions[0].name = dup_text("main");
    layout_program.functions[0].has_body = 1;
    layout_program.functions[0].block_count = 2;
    layout_program.functions[0].block_capacity = 2;
    layout_program.functions[0].blocks = (MachineLayoutBlock *)calloc(2, sizeof(MachineLayoutBlock));
    if (!layout_program.functions[0].name || !layout_program.functions[0].blocks) {
        machine_layout_program_free(&layout_program);
        machine_emit_program_free(&cloned_program);
        machine_emit_lower_report_free(&report);
        return 0;
    }

    layout_program.functions[0].blocks[0].layout_index = 0;
    layout_program.functions[0].blocks[0].original_block_id = 0;
    layout_program.functions[0].blocks[0].has_terminator = 1;
    layout_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_FALLTHROUGH;
    layout_program.functions[0].blocks[0].terminator.as.fallthrough_target = 1;

    layout_program.functions[0].blocks[1].layout_index = 1;
    layout_program.functions[0].blocks[1].original_block_id = 1;
    layout_program.functions[0].blocks[1].has_terminator = 1;
    layout_program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    layout_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(3);

    if (!machine_emit_lower_program_from_machine_layout(&layout_program, &emit_program, &emit_error) ||
        !machine_emit_clone_program(&emit_program, &cloned_program, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: clone setup failed: %s\n", emit_error.message);
        machine_layout_program_free(&layout_program);
        machine_emit_program_free(&emit_program);
        machine_emit_program_free(&cloned_program);
        machine_emit_lower_report_free(&report);
        return 0;
    }

    ok &= expect_dump(
        &cloned_program,
        "machine_emit\n"
        "function main params=0 locals=0 spills=0\n"
        "  F0.L0: ; emit.0 -> layout.0 -> bb.0\n"
        "    fallthrough F0.L1\n"
        "  F0.L1: ; emit.1 -> layout.1 -> bb.1\n"
        "    reti 3\n");

    if (!machine_emit_build_report_from_program(&cloned_program, &report, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: build report from program failed: %s\n", emit_error.message);
        ok = 0;
    }
    if (!machine_emit_lower_report_get_function_shape_summary(&report, 0, &function_shape) || !function_shape ||
        function_shape->fallthrough_count != 1 || function_shape->return_imm_count != 1) {
        fprintf(stderr, "[machine-emit] FAIL: report-from-program function shape mismatch\n");
        ok = 0;
    }
    if (!machine_emit_lower_report_get_block_shape_summary(&report, 0, 1, &block_shape) || !block_shape ||
        !block_shape->label_name || strcmp(block_shape->label_name, "F0.L1") != 0 ||
        block_shape->terminator_kind != MACHINE_LAYOUT_TERM_RETURN_IMM) {
        fprintf(stderr, "[machine-emit] FAIL: report-from-program block shape mismatch\n");
        ok = 0;
    }
    if (!machine_emit_build_report_from_program_dump(&cloned_program, &actual_text, &emit_error) || !actual_text ||
        strstr(actual_text, "machine_emit-report call_funcs=0 fallthrough_funcs=1 branch_funcs=0 total_block_summaries=2\n") != actual_text ||
        strstr(actual_text, "target_policy preview_reg_cap=8 preserves_spills=1 preserves_global_slots=1 preserves_fallthrough=1\n") == NULL ||
        strstr(actual_text, "emit.1 label=F0.L1 layout.1 bb.1 ops=0 calls=0 has_term=1 term=1\n") == NULL) {
        fprintf(stderr, "[machine-emit] FAIL: report-from-program dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }

    free(actual_text);
    machine_layout_program_free(&layout_program);
    machine_emit_program_free(&emit_program);
    machine_emit_program_free(&cloned_program);
    machine_emit_lower_report_free(&report);
    return ok;
}

static int test_machine_emit_report_dump_from_machine_layout(void) {
    MachineLayoutProgram layout_program;
    MachineEmitError emit_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&emit_error, 0, sizeof(emit_error));
    machine_layout_program_init(&layout_program);

    layout_program.function_count = 1;
    layout_program.function_capacity = 1;
    layout_program.functions = (MachineLayoutFunction *)calloc(1, sizeof(MachineLayoutFunction));
    if (!layout_program.functions) {
        return 0;
    }
    layout_program.functions[0].name = dup_text("main");
    layout_program.functions[0].has_body = 1;
    layout_program.functions[0].block_count = 2;
    layout_program.functions[0].block_capacity = 2;
    layout_program.functions[0].blocks = (MachineLayoutBlock *)calloc(2, sizeof(MachineLayoutBlock));
    if (!layout_program.functions[0].name || !layout_program.functions[0].blocks) {
        machine_layout_program_free(&layout_program);
        return 0;
    }

    layout_program.functions[0].blocks[0].layout_index = 0;
    layout_program.functions[0].blocks[0].original_block_id = 0;
    layout_program.functions[0].blocks[0].has_terminator = 1;
    layout_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_FALLTHROUGH;
    layout_program.functions[0].blocks[0].terminator.as.fallthrough_target = 1;

    layout_program.functions[0].blocks[1].layout_index = 1;
    layout_program.functions[0].blocks[1].original_block_id = 1;
    layout_program.functions[0].blocks[1].has_terminator = 1;
    layout_program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    layout_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(7);

    if (!machine_emit_build_report_from_machine_layout_program_dump(&layout_program, &actual_text, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: layout report dump failed: %s\n", emit_error.message);
        machine_layout_program_free(&layout_program);
        return 0;
    }

    if (!actual_text ||
        strcmp(actual_text,
            "machine_emit-report call_funcs=0 fallthrough_funcs=1 branch_funcs=0 total_block_summaries=2\n"
            "target_policy preview_reg_cap=8 preserves_spills=1 preserves_global_slots=1 preserves_fallthrough=1\n"
            "functions-with-calls:\n"
            "functions-with-fallthrough: 0\n"
            "functions-with-branches:\n"
            "function-shapes:\n"
            "  fn.0 main blocks=2 ops=0 calls=0 blocks_with_calls=0 jumps=0 fallthrough=1 branch=0 branch_ft=0 cmpbr=0 cmpbri=0 cmpbrft=0 cmpbrift=0 ret=0 reti=1 retspill=0\n"
            "    emit.0 label=F0.L0 layout.0 bb.0 ops=0 calls=0 has_term=1 term=3\n"
            "    emit.1 label=F0.L1 layout.1 bb.1 ops=0 calls=0 has_term=1 term=1\n"
            "\n"
            "machine_emit\n"
            "function main params=0 locals=0 spills=0\n"
            "  F0.L0: ; emit.0 -> layout.0 -> bb.0\n"
            "    fallthrough F0.L1\n"
            "  F0.L1: ; emit.1 -> layout.1 -> bb.1\n"
            "    reti 7\n") != 0) {
        fprintf(stderr, "[machine-emit] FAIL: layout report dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }

    free(actual_text);
    machine_layout_program_free(&layout_program);
    return ok;
}

static int test_machine_emit_report_bridge_from_machine_ir_report(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineEmitProgram emit_program;
    MachineEmitLowerReport emit_report;
    MachineEmitError emit_error;
    const MachineEmitFunctionShapeSummary *function_shape = NULL;
    const MachineEmitBlockShapeSummary *block_shape = NULL;
    const MachineEmitFunction *function_view = NULL;
    const size_t *function_indices = NULL;
    size_t counted_functions = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&emit_error, 0, sizeof(emit_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_emit_program_init(&emit_program);
    machine_emit_lower_report_init(&emit_report);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        machine_ir_allocate_rewrite_report_free(&machine_report);
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
        fprintf(stderr, "[machine-emit] FAIL: machine-ir report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_emit_program_free(&emit_program);
        machine_emit_lower_report_free(&emit_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-emit] FAIL: machine-ir report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_emit_program_free(&emit_program);
        machine_emit_lower_report_free(&emit_report);
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
        fprintf(stderr, "[machine-emit] FAIL: machine-ir report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_emit_program_free(&emit_program);
        machine_emit_lower_report_free(&emit_report);
        return 0;
    }

    if (!machine_emit_build_program_from_machine_ir_report(&machine_report, &emit_program, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: build program from machine-ir report failed: %s\n", emit_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_emit_program_free(&emit_program);
        machine_emit_lower_report_free(&emit_report);
        return 0;
    }
    if (!machine_emit_build_report_from_machine_ir_report(&machine_report, &emit_report, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: build report from machine-ir report failed: %s\n", emit_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_emit_program_free(&emit_program);
        machine_emit_lower_report_free(&emit_report);
        return 0;
    }

    if (!machine_emit_lower_report_get_function_by_name(&emit_report, "main", NULL, &function_view) || !function_view ||
        !machine_emit_lower_report_get_function_shape_summary(&emit_report, 0, &function_shape) || !function_shape ||
        function_shape->compare_branch_imm_fallthrough_count != 1 || function_shape->return_imm_count != 2) {
        fprintf(stderr, "[machine-emit] FAIL: report bridge function shape mismatch\n");
        ok = 0;
    }
    if (!machine_emit_lower_report_get_block_shape_summary(&emit_report, 0, 0, &block_shape) || !block_shape ||
        block_shape->emit_index != 0 || block_shape->call_count != 0 ||
        block_shape->terminator_kind != MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH) {
        fprintf(stderr, "[machine-emit] FAIL: report bridge block shape mismatch\n");
        ok = 0;
    }
    if (!machine_emit_lower_report_get_functions_with_branches(&emit_report, &counted_functions, &function_indices) ||
        counted_functions != 1 || !function_indices || function_indices[0] != 0) {
        fprintf(stderr, "[machine-emit] FAIL: report bridge functions-with-branches mismatch\n");
        ok = 0;
    }

    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_emit_program_free(&emit_program);
    machine_emit_lower_report_free(&emit_report);
    return ok;
}

static int test_machine_emit_report_dump_from_machine_ir_report(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineEmitError emit_error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&emit_error, 0, sizeof(emit_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);

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
        fprintf(stderr, "[machine-emit] FAIL: machine-ir report dump setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-emit] FAIL: machine-ir report dump setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
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
        fprintf(stderr, "[machine-emit] FAIL: machine-ir report dump setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        return 0;
    }

    if (!machine_emit_build_report_from_machine_ir_report_dump(&machine_report, &actual_text, &emit_error)) {
        fprintf(stderr, "[machine-emit] FAIL: machine-ir report dump failed: %s\n", emit_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        return 0;
    }

    if (!actual_text ||
        strstr(actual_text, "machine_emit-report call_funcs=0 fallthrough_funcs=1 branch_funcs=1 total_block_summaries=3\n") != actual_text ||
        strstr(actual_text, "functions-with-fallthrough: 0\n") == NULL ||
        strstr(actual_text, "functions-with-branches: 0\n") == NULL ||
        strstr(actual_text, "emit.0 label=F0.L0 layout.0 bb.0 ops=1 calls=0 has_term=1 term=10\n") == NULL ||
        strstr(actual_text, "cmpbrift.t.10 reg.0, 0, taken=F0.L2, fallthrough=F0.L1\n") == NULL) {
        fprintf(stderr, "[machine-emit] FAIL: machine-ir report dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }

    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_emit_verifier_rejects_duplicate_labels(void) {
    MachineEmitProgram program;
    MachineEmitError error;

    memset(&error, 0, sizeof(error));
    machine_emit_program_init(&program);

    program.function_count = 1;
    program.function_capacity = 1;
    program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!program.functions) {
        return 0;
    }

    program.functions[0].name = dup_text("main");
    program.functions[0].has_body = 1;
    program.functions[0].block_count = 2;
    program.functions[0].block_capacity = 2;
    program.functions[0].blocks = (MachineEmitBlock *)calloc(2, sizeof(MachineEmitBlock));
    if (!program.functions[0].name || !program.functions[0].blocks) {
        machine_emit_program_free(&program);
        return 0;
    }

    program.functions[0].blocks[0].emit_index = 0;
    program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    program.functions[0].blocks[0].has_terminator = 1;
    program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_FALLTHROUGH;
    program.functions[0].blocks[0].terminator.as.fallthrough_target = 1;

    program.functions[0].blocks[1].emit_index = 1;
    program.functions[0].blocks[1].label_name = dup_text("F0.L0");
    program.functions[0].blocks[1].has_terminator = 1;
    program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(0);

    if (machine_emit_verify_program(&program, &error)) {
        fprintf(stderr, "[machine-emit] FAIL: verifier accepted duplicate labels\n");
        machine_emit_program_free(&program);
        return 0;
    }
    if (strstr(error.message, "unique") == NULL) {
        fprintf(stderr, "[machine-emit] FAIL: unexpected verifier error: %s\n", error.message);
        machine_emit_program_free(&program);
        return 0;
    }

    machine_emit_program_free(&program);
    return 1;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_emit_summary_surface();
    ok &= test_machine_emit_lowers_labels_from_machine_layout();
    ok &= test_machine_emit_preserves_void_return_shape();
    ok &= test_machine_emit_bridge_surfaces_labels_from_machine_ir();
    ok &= test_machine_emit_lower_dump_from_machine_ir();
    ok &= test_machine_emit_report_surface_from_machine_layout();
    ok &= test_machine_emit_clone_and_report_from_program();
    ok &= test_machine_emit_report_dump_from_machine_layout();
    ok &= test_machine_emit_report_bridge_from_machine_ir_report();
    ok &= test_machine_emit_report_dump_from_machine_ir_report();
    ok &= test_machine_emit_verifier_rejects_duplicate_labels();
    ok &= test_machine_emit_rejects_riscv32_preview_incompatible_register_bank();
    ok &= test_machine_emit_rejects_riscv32_preview_bytes_incompatible_branch_range();

    if (!ok) {
        return 1;
    }

    printf("[machine-emit] PASS\n");
    return 0;
}
