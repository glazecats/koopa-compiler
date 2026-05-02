#include "machine/bytes.h"

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

static int expect_dump(const MachineBytesProgram *program, const char *expected_text) {
    char *actual_text = NULL;
    MachineBytesError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_bytes_dump_program(program, &actual_text, &error)) {
        fprintf(stderr, "[machine-bytes] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-bytes] FAIL: dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int expect_byte_image(
    const unsigned char *actual_bytes,
    size_t actual_count,
    const unsigned char *expected_bytes,
    size_t expected_count,
    const char *context) {
    size_t byte_index;

    if (actual_count != expected_count) {
        fprintf(stderr,
            "[machine-bytes] FAIL: %s byte count mismatch: expected %zu got %zu\n",
            context,
            expected_count,
            actual_count);
        return 0;
    }
    for (byte_index = 0; byte_index < expected_count; ++byte_index) {
        if (actual_bytes[byte_index] != expected_bytes[byte_index]) {
            fprintf(stderr,
                "[machine-bytes] FAIL: %s byte[%zu] mismatch: expected %02X got %02X\n",
                context,
                byte_index,
                expected_bytes[byte_index],
                actual_bytes[byte_index]);
            return 0;
        }
    }
    return 1;
}

static int test_machine_bytes_lowers_from_machine_encode(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    MachineBytesFunctionSummary summary;
    const MachineBytesFunction *function_view = NULL;
    const MachineBytesBlock *block_view = NULL;
    const unsigned char *block_bytes = NULL;
    unsigned char *function_bytes = NULL;
    unsigned char *program_bytes = NULL;
    size_t block_byte_count = 0;
    size_t function_byte_count = 0;
    size_t program_byte_count = 0;
    size_t function_index = 0;
    size_t emit_index = 0;
    static const unsigned char expected_program_bytes[] = {0x21u, 0x81u, 0x83u, 0x01u, 0x81u, 0x17u};
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 2;
    emit_program.functions[0].block_capacity = 2;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(2, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1;
    emit_program.functions[0].blocks[0].op_capacity = 1;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
    emit_program.functions[0].blocks[0].ops[0].as.store.slot = machine_select_slot_global(0);
    emit_program.functions[0].blocks[0].ops[0].as.store.value = machine_select_operand_immediate(1);
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

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_bytes_lower_program_from_machine_encode(&encode_program, &bytes_program, &bytes_error)) {
        fprintf(stderr, "[machine-bytes] FAIL: lower failed: %s / %s\n", encode_error.message, bytes_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    ok &= expect_dump(
        &bytes_program,
        "machine_bytes\n"
        "function main params=0 locals=0 spills=0 bytes=6\n"
        "  F0.L0 @0..4 term@2 bytes=21 81 83 01 ; emit.0 -> layout.0 -> bb.0\n"
        "    fallthrough F0.L1 @4\n"
        "  F0.L1 @4..6 term@4 bytes=81 17 ; emit.1 -> layout.1 -> bb.1\n"
        "    return\n");

    if (!machine_bytes_function_compute_summary(&bytes_program.functions[0], &summary) ||
        summary.block_count != 2 || summary.byte_count != 6 || summary.op_byte_count != 2 ||
        summary.terminator_byte_count != 4 || summary.jump_count != 1 || summary.fallthrough_count != 1 ||
        summary.return_count != 1) {
        fprintf(stderr, "[machine-bytes] FAIL: summary mismatch\n");
        ok = 0;
    }
    if (!machine_bytes_program_get_function_by_name(&bytes_program, "main", &function_index, &function_view) ||
        function_index != 0 || !function_view ||
        !machine_bytes_function_get_block(function_view, 0, &block_view) || !block_view ||
        !machine_bytes_block_get_bytes(block_view, &block_bytes, &block_byte_count) ||
        !expect_byte_image(block_bytes, block_byte_count, expected_program_bytes, 4, "block-view") ||
        !machine_bytes_function_copy_bytes(function_view, &function_bytes, &function_byte_count, &bytes_error) ||
        !function_bytes ||
        !expect_byte_image(function_bytes, function_byte_count, expected_program_bytes, 6, "function-view") ||
        !machine_bytes_program_copy_bytes(&bytes_program, &program_bytes, &program_byte_count, &bytes_error) ||
        !program_bytes ||
        !expect_byte_image(program_bytes, program_byte_count, expected_program_bytes, 6, "program-view") ||
        !machine_bytes_program_find_block_by_program_byte_offset(
            &bytes_program, 5, &function_index, &emit_index, &function_view, &block_view) ||
        function_index != 0 || emit_index != 1 || !block_view || strcmp(block_view->label_name, "F0.L1") != 0) {
        fprintf(stderr, "[machine-bytes] FAIL: direct byte/query helper mismatch\n");
        ok = 0;
    }

    free(function_bytes);
    free(program_bytes);
    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_report_and_bridges(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineBytesProgram bytes_program;
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    const MachineBytesFunctionSummary *function_summary = NULL;
    const MachineBytesBlockSummary *block_summary = NULL;
    const MachineBytesProgram *program_view = NULL;
    const MachineBytesReferenceSummary *reference_summary = NULL;
    const MachineBytesReferenceSummary *function_references = NULL;
    const MachineBytesFixupSummary *fixup_summary = NULL;
    const MachineBytesFixupSummary *function_fixups = NULL;
    const MachineBytesSymbolSummary *symbol_summary = NULL;
    const MachineBytesSectionSummary *section_summary = NULL;
    const MachineBytesSymbolSummary *section_symbols = NULL;
    const MachineBytesFixupSummary *section_fixups = NULL;
    const size_t *function_indices = NULL;
    unsigned char *report_function_bytes = NULL;
    unsigned char *report_block_bytes = NULL;
    unsigned char *report_bytes = NULL;
    size_t function_count = 0;
    size_t reference_count = 0;
    size_t fixup_count = 0;
    size_t symbol_count = 0;
    size_t section_count = 0;
    size_t total_program_byte_count = 0;
    size_t function_start = 0;
    size_t function_end = 0;
    size_t located_function_index = 0;
    size_t report_function_byte_count = 0;
    size_t report_block_byte_count = 0;
    size_t report_byte_count = 0;
    char *actual_text = NULL;
    static const unsigned char expected_report_bytes[] = {0x1Cu, 0x8Au, 0xEAu, 0x00u, 0x21u, 0x81u, 0x11u, 0x81u, 0x12u};
    static const unsigned char expected_report_block_bytes[] = {0x81u, 0x11u};
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_bytes_program_init(&bytes_program);
    machine_bytes_report_init(&bytes_report);

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
        fprintf(stderr, "[machine-bytes] FAIL: machine-ir report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-bytes] FAIL: machine-ir report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
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
        fprintf(stderr, "[machine-bytes] FAIL: machine-ir report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    if (!machine_bytes_lower_program_from_machine_ir_report(&machine_report, &bytes_program, &bytes_error) ||
        !machine_bytes_build_report_from_machine_ir_report(&machine_report, &bytes_report, &bytes_error)) {
        fprintf(stderr, "[machine-bytes] FAIL: report bridge failed: %s\n", bytes_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    if (!machine_bytes_report_get_function_summary_artifact(&bytes_report, 0, &function_summary) || !function_summary ||
        function_summary->byte_count != 9 || function_summary->compare_branch_count != 1 || function_summary->return_count != 2 ||
        !machine_bytes_report_get_program(&bytes_report, &program_view) || !program_view ||
        !machine_bytes_report_get_total_program_byte_count(&bytes_report, &total_program_byte_count) ||
        total_program_byte_count != 9 ||
        !machine_bytes_report_get_reference_summary_count(&bytes_report, &reference_count) ||
        reference_count != 2 ||
        !machine_bytes_report_get_function_reference_summaries(&bytes_report, 0, &reference_count, &function_references) ||
        reference_count != 2 || !function_references ||
        !machine_bytes_report_get_fixup_summary_count(&bytes_report, &fixup_count) ||
        fixup_count != 2 ||
        !machine_bytes_report_get_function_fixup_summaries(&bytes_report, 0, &fixup_count, &function_fixups) ||
        fixup_count != 2 || !function_fixups ||
        !machine_bytes_report_get_symbol_summary_count(&bytes_report, &symbol_count) ||
        symbol_count != 4 ||
        !machine_bytes_report_get_section_summary_count(&bytes_report, &section_count) ||
        section_count != 1 ||
        !machine_bytes_report_find_section_summary_by_name(&bytes_report, ".text", NULL, &section_summary) ||
        !section_summary || section_summary->byte_count != 9 || section_summary->function_count != 1 ||
        section_summary->block_count != 3 || section_summary->symbol_count != 4 || section_summary->fixup_count != 2 ||
        !machine_bytes_report_get_section_symbol_summaries(&bytes_report, 0, &symbol_count, &section_symbols) ||
        symbol_count != 4 || !section_symbols ||
        !machine_bytes_report_get_section_fixup_summaries(&bytes_report, 0, &fixup_count, &section_fixups) ||
        fixup_count != 2 || !section_fixups ||
        !machine_bytes_report_find_symbol_summary_by_name(&bytes_report, "main", NULL, &symbol_summary) ||
        !symbol_summary || symbol_summary->kind != MACHINE_BYTES_SYMBOL_FUNCTION ||
        !symbol_summary->is_defined || !symbol_summary->has_byte_offset ||
        symbol_summary->byte_offset != 0 || symbol_summary->byte_count != 9 ||
        !machine_bytes_report_find_symbol_summary_by_name(&bytes_report, "F0.L2", NULL, &symbol_summary) ||
        !symbol_summary || symbol_summary->kind != MACHINE_BYTES_SYMBOL_BLOCK ||
        !symbol_summary->has_emit_index || symbol_summary->emit_index != 2 ||
        !symbol_summary->has_byte_offset || symbol_summary->byte_offset != 7 ||
        !machine_bytes_report_get_reference_summary(&bytes_report, 0, &reference_summary) ||
        !reference_summary ||
        reference_summary->kind != MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY ||
        reference_summary->patch_byte_offset != 4 ||
        reference_summary->patch_byte_count != 1 ||
        !reference_summary->target_name || strcmp(reference_summary->target_name, "F0.L2") != 0 ||
        !reference_summary->has_target_byte_offset || reference_summary->target_byte_offset != 7 ||
        !machine_bytes_report_get_reference_summary(&bytes_report, 1, &reference_summary) ||
        !reference_summary ||
        reference_summary->kind != MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY ||
        !reference_summary->target_name || strcmp(reference_summary->target_name, "F0.L1") != 0 ||
        !reference_summary->has_target_emit_index || reference_summary->target_emit_index != 1 ||
        !machine_bytes_report_get_fixup_summary(&bytes_report, 0, &fixup_summary) ||
        !fixup_summary ||
        fixup_summary->kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        fixup_summary->target_kind != MACHINE_BYTES_FIXUP_TARGET_BLOCK ||
        fixup_summary->patch_byte_offset != 4 ||
        fixup_summary->patch_byte_count != 1 ||
        !fixup_summary->has_target_symbol_index ||
        !fixup_summary->target_name || strcmp(fixup_summary->target_name, "F0.L2") != 0 ||
        !machine_bytes_report_get_fixup_summary(&bytes_report, 1, &fixup_summary) ||
        !fixup_summary ||
        fixup_summary->kind != MACHINE_BYTES_FIXUP_CONTROL_SECONDARY ||
        fixup_summary->target_kind != MACHINE_BYTES_FIXUP_TARGET_BLOCK ||
        !fixup_summary->target_name || strcmp(fixup_summary->target_name, "F0.L1") != 0 ||
        !machine_bytes_report_get_function_byte_span(&bytes_report, 0, &function_start, &function_end) ||
        function_start != 0 || function_end != 9 ||
        !machine_bytes_report_copy_function_bytes(
            &bytes_report, 0, &report_function_bytes, &report_function_byte_count, &bytes_error) ||
        !report_function_bytes ||
        !expect_byte_image(report_function_bytes, report_function_byte_count, expected_report_bytes, 9, "report-function-view") ||
        !machine_bytes_report_copy_block_bytes(
            &bytes_report, 0, 1, &report_block_bytes, &report_block_byte_count, &bytes_error) ||
        !report_block_bytes ||
        !expect_byte_image(
            report_block_bytes, report_block_byte_count, expected_report_block_bytes, 2, "report-block-view") ||
        !machine_bytes_report_find_block_summary_by_label(&bytes_report, "F0.L2", NULL, &block_summary) ||
        !block_summary || block_summary->start_byte_offset != 7 ||
        !machine_bytes_report_copy_bytes(&bytes_report, &report_bytes, &report_byte_count, &bytes_error) ||
        !report_bytes ||
        !expect_byte_image(report_bytes, report_byte_count, expected_report_bytes, 9, "report-view") ||
        !machine_bytes_report_find_block_summary_by_program_byte_offset(
            &bytes_report, 7, &located_function_index, &block_summary) ||
        located_function_index != 0 || !block_summary || strcmp(block_summary->label_name, "F0.L2") != 0) {
        fprintf(stderr, "[machine-bytes] FAIL: report query mismatch\n");
        ok = 0;
    }
    if (!machine_bytes_report_get_functions_with_fallthrough(&bytes_report, &function_count, &function_indices) ||
        function_count != 1 || !function_indices || function_indices[0] != 0 ||
        !machine_bytes_report_get_functions_with_branches(&bytes_report, &function_count, &function_indices) ||
        function_count != 1 || !function_indices || function_indices[0] != 0) {
        fprintf(stderr, "[machine-bytes] FAIL: function-index set mismatch\n");
        ok = 0;
    }
    if (!machine_bytes_build_report_from_machine_ir_report_dump(&machine_report, &actual_text, &bytes_error) ||
        !actual_text ||
        strstr(actual_text, "machine_bytes-report total_bytes=9 call_funcs=0 fallthrough_funcs=1 branch_funcs=1 total_block_summaries=3\n") != actual_text ||
        strstr(actual_text, "sec.0 .text span=0..9 bytes=9 funcs=1 blocks=3 symbols=4 fixups=2\n") == NULL ||
        strstr(actual_text, "sym.0 function main defined=1 fn=yes emit=no offset=yes bytes=9 incoming_fixups=0\n") == NULL ||
        strstr(actual_text, "fixup.0 ctrl-primary src=F0.L0 patch@4 bytes=1 target=F0.L2 sym=yes\n") == NULL ||
        strstr(actual_text, "fn.0 main span=0..9 blocks=3 bytes=9 op_bytes=1 calls=0 blocks_with_calls=0 term_bytes=8") == NULL ||
        strstr(actual_text, "emit.2 label=F0.L2 layout.2 bb.2 start=7 term=7 end=9 bytes=2\n") == NULL ||
        strstr(actual_text, "cmpbrift.10 taken=F0.L2 @7 fallthrough=F0.L1 @5\n") == NULL) {
        fprintf(stderr, "[machine-bytes] FAIL: report dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }

    free(actual_text);
    free(report_function_bytes);
    free(report_block_bytes);
    free(report_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_bytes_program_free(&bytes_program);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

static int test_machine_bytes_riscv32_preview_profile_changes_text_bytes(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineBytesReport generic_report;
    MachineBytesReport preview_report;
    MachineBytesError bytes_error;
    unsigned char *generic_bytes = NULL;
    unsigned char *preview_bytes = NULL;
    size_t generic_byte_count = 0u;
    size_t preview_byte_count = 0u;
    size_t fixup_count = 0u;
    const MachineBytesFixupSummary *fixup = NULL;
    const MachineBytesFunctionSummary *function_summary = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_bytes_report_init(&generic_report);
    machine_bytes_report_init(&preview_report);

    machine_report.program.register_bank.register_count = 1u;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0u;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_bytes_report_free(&generic_report);
        machine_bytes_report_free(&preview_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_bytes_report_free(&generic_report);
        machine_bytes_report_free(&preview_report);
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
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_bytes_report_free(&generic_report);
        machine_bytes_report_free(&preview_report);
        return 0;
    }

    if (!machine_bytes_build_report_from_machine_ir_report(&machine_report, &generic_report, &bytes_error) ||
        !machine_bytes_build_report_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &preview_report,
            &bytes_error) ||
        !machine_bytes_report_copy_bytes(&generic_report, &generic_bytes, &generic_byte_count, &bytes_error) ||
        !machine_bytes_report_copy_bytes(&preview_report, &preview_bytes, &preview_byte_count, &bytes_error) ||
        generic_report.program.target_profile != MACHINE_BYTES_TARGET_PROFILE_GENERIC ||
        preview_report.program.target_profile != MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW ||
        generic_byte_count != 9u ||
        preview_byte_count != 28u ||
        preview_byte_count % 4u != 0u ||
        !generic_bytes || !preview_bytes ||
        memcmp(generic_bytes, preview_bytes, generic_byte_count) == 0 ||
        !machine_bytes_report_get_fixup_summary_count(&preview_report, &fixup_count) ||
        fixup_count != 2u ||
        !machine_bytes_report_get_fixup_summary(&preview_report, 0u, &fixup) || !fixup ||
        fixup->kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        fixup->patch_byte_offset != 4u ||
        fixup->patch_byte_count != 4u ||
        fixup->owner_byte_count != 8u ||
        !machine_bytes_report_get_fixup_summary(&preview_report, 1u, &fixup) || !fixup ||
        fixup->kind != MACHINE_BYTES_FIXUP_CONTROL_SECONDARY ||
        fixup->patch_byte_offset != 8u ||
        fixup->patch_byte_count != 4u ||
        !machine_bytes_report_get_function_summary_artifact(&preview_report, 0u, &function_summary) ||
        !function_summary ||
        function_summary->byte_count != 28u ||
        function_summary->op_byte_count != 4u ||
        function_summary->terminator_byte_count != 24u) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview profile mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(generic_bytes);
    free(preview_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_bytes_report_free(&generic_report);
    machine_bytes_report_free(&preview_report);
    return ok;
}

static int test_machine_bytes_program_level_offsets_and_byte_image(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    MachineBytesReport bytes_report;
    const MachineBytesFunction *function_view = NULL;
    const MachineBytesBlock *block_view = NULL;
    unsigned char *function_bytes = NULL;
    unsigned char *block_bytes = NULL;
    unsigned char *program_bytes = NULL;
    unsigned char *report_function_bytes = NULL;
    unsigned char *report_block_bytes = NULL;
    size_t total_program_byte_count = 0;
    size_t function_start = 0;
    size_t function_end = 0;
    size_t function_byte_count = 0;
    size_t block_byte_count = 0;
    size_t program_byte_count = 0;
    size_t report_function_byte_count = 0;
    size_t report_block_byte_count = 0;
    size_t function_index = 0;
    size_t emit_index = 0;
    static const unsigned char expected_program_bytes[] = {
        0x21u, 0x82u, 0x83u, 0x01u, 0x81u, 0x11u,
        0x21u, 0x85u, 0x81u, 0x13u};
    static const unsigned char expected_helper_bytes[] = {0x21u, 0x85u, 0x81u, 0x13u};
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);
    machine_bytes_report_init(&bytes_report);

    emit_program.global_count = 1;
    emit_program.global_capacity = 1;
    emit_program.globals = (MachineEmitGlobal *)calloc(1, sizeof(MachineEmitGlobal));
    emit_program.function_count = 2;
    emit_program.function_capacity = 2;
    emit_program.functions = (MachineEmitFunction *)calloc(2, sizeof(MachineEmitFunction));
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

    emit_program.functions[1].name = dup_text("helper");
    emit_program.functions[1].has_body = 1;
    emit_program.functions[1].block_count = 1;
    emit_program.functions[1].block_capacity = 1;
    emit_program.functions[1].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks ||
        !emit_program.functions[1].name || !emit_program.functions[1].blocks) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
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
    emit_program.functions[0].blocks[0].ops[0].as.store.value = machine_select_operand_immediate(2);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_FALLTHROUGH;
    emit_program.functions[0].blocks[0].terminator.as.fallthrough_target = 1;

    emit_program.functions[0].blocks[1].emit_index = 1;
    emit_program.functions[0].blocks[1].original_layout_index = 1;
    emit_program.functions[0].blocks[1].original_block_id = 1;
    emit_program.functions[0].blocks[1].label_name = dup_text("F0.L1");
    emit_program.functions[0].blocks[1].has_terminator = 1;
    emit_program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(1);

    emit_program.functions[1].blocks[0].emit_index = 0;
    emit_program.functions[1].blocks[0].original_layout_index = 0;
    emit_program.functions[1].blocks[0].original_block_id = 0;
    emit_program.functions[1].blocks[0].label_name = dup_text("F1.L0");
    emit_program.functions[1].blocks[0].op_count = 1;
    emit_program.functions[1].blocks[0].op_capacity = 1;
    emit_program.functions[1].blocks[0].ops = (MachineEmitOp *)calloc(1, sizeof(MachineEmitOp));
    emit_program.functions[1].blocks[0].ops[0].kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
    emit_program.functions[1].blocks[0].ops[0].as.store.slot = machine_select_slot_global(0);
    emit_program.functions[1].blocks[0].ops[0].as.store.value = machine_select_operand_immediate(5);
    emit_program.functions[1].blocks[0].has_terminator = 1;
    emit_program.functions[1].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[1].blocks[0].terminator.as.return_value = machine_select_operand_immediate(3);

    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops ||
        !emit_program.functions[0].blocks[1].label_name ||
        !emit_program.functions[1].blocks[0].label_name || !emit_program.functions[1].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_bytes_lower_program_from_machine_encode(&encode_program, &bytes_program, &bytes_error) ||
        !machine_bytes_build_report_from_program(&bytes_program, &bytes_report, &bytes_error) ||
        !machine_bytes_program_get_total_byte_count(&bytes_program, &total_program_byte_count) ||
        total_program_byte_count != 10 ||
        !machine_bytes_program_get_function_byte_span(&bytes_program, 0, &function_start, &function_end) ||
        function_start != 0 || function_end != 6 ||
        !machine_bytes_program_get_function_byte_span(&bytes_program, 1, &function_start, &function_end) ||
        function_start != 6 || function_end != 10 ||
        !machine_bytes_program_copy_bytes(&bytes_program, &program_bytes, &program_byte_count, &bytes_error) ||
        !program_bytes ||
        !expect_byte_image(program_bytes, program_byte_count, expected_program_bytes, 10, "multi-function-program") ||
        !machine_bytes_program_copy_function_bytes(
            &bytes_program, 1, &function_bytes, &function_byte_count, &bytes_error) ||
        !function_bytes ||
        !expect_byte_image(function_bytes, function_byte_count, expected_helper_bytes, 4, "program-helper-function") ||
        !machine_bytes_program_copy_block_bytes(
            &bytes_program, 1, 0, &block_bytes, &block_byte_count, &bytes_error) ||
        !block_bytes ||
        !expect_byte_image(block_bytes, block_byte_count, expected_helper_bytes, 4, "program-helper-block") ||
        !machine_bytes_report_copy_function_bytes(
            &bytes_report, 1, &report_function_bytes, &report_function_byte_count, &bytes_error) ||
        !report_function_bytes ||
        !expect_byte_image(
            report_function_bytes, report_function_byte_count, expected_helper_bytes, 4, "report-helper-function") ||
        !machine_bytes_report_copy_block_bytes(
            &bytes_report, 1, 0, &report_block_bytes, &report_block_byte_count, &bytes_error) ||
        !report_block_bytes ||
        !expect_byte_image(
            report_block_bytes, report_block_byte_count, expected_helper_bytes, 4, "report-helper-block") ||
        !machine_bytes_program_find_block_by_program_byte_offset(
            &bytes_program, 0, &function_index, &emit_index, &function_view, &block_view) ||
        function_index != 0 || emit_index != 0 || !function_view || strcmp(function_view->name, "main") != 0 ||
        !block_view || strcmp(block_view->label_name, "F0.L0") != 0 ||
        !machine_bytes_program_find_block_by_program_byte_offset(
            &bytes_program, 6, &function_index, &emit_index, &function_view, &block_view) ||
        function_index != 1 || emit_index != 0 || !function_view || strcmp(function_view->name, "helper") != 0 ||
        !block_view || strcmp(block_view->label_name, "F1.L0") != 0 ||
        machine_bytes_program_find_block_by_program_byte_offset(
            &bytes_program, 10, &function_index, &emit_index, &function_view, &block_view)) {
        fprintf(stderr, "[machine-bytes] FAIL: program-level offset/byte image mismatch\n");
        ok = 0;
    }

    free(function_bytes);
    free(block_bytes);
    free(program_bytes);
    free(report_function_bytes);
    free(report_block_bytes);
    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

static int test_machine_bytes_from_machine_emit_artifacts(void) {
    MachineEmitProgram emit_program;
    MachineEmitLowerReport emit_report;
    MachineEmitError emit_error;
    MachineBytesProgram bytes_program;
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    const MachineBytesBlockSummary *block_summary = NULL;
    char *actual_text = NULL;
    int ok = 1;

    memset(&emit_error, 0, sizeof(emit_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_emit_lower_report_init(&emit_report);
    machine_bytes_program_init(&bytes_program);
    machine_bytes_report_init(&bytes_report);

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
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1;
    emit_program.functions[0].blocks[0].op_capacity = 1;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_emit_lower_report_free(&emit_report);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }
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
    emit_program.functions[0].blocks[1].terminator.as.return_value = machine_select_operand_immediate(3);

    if (!machine_emit_build_report_from_program(&emit_program, &emit_report, &emit_error)) {
        fprintf(stderr, "[machine-bytes] FAIL: emit report setup failed: %s\n", emit_error.message);
        machine_emit_program_free(&emit_program);
        machine_emit_lower_report_free(&emit_report);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    if (!machine_bytes_lower_program_from_machine_emit(&emit_program, &bytes_program, &bytes_error) ||
        !machine_bytes_build_report_from_machine_emit_report(&emit_report, &bytes_report, &bytes_error)) {
        fprintf(stderr, "[machine-bytes] FAIL: emit artifact bridge failed: %s\n", bytes_error.message);
        machine_emit_program_free(&emit_program);
        machine_emit_lower_report_free(&emit_report);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    ok &= expect_dump(
        &bytes_program,
        "machine_bytes\n"
        "function main params=0 locals=0 spills=0 bytes=6\n"
        "  F0.L0 @0..4 term@2 bytes=21 84 83 01 ; emit.0 -> layout.0 -> bb.0\n"
        "    fallthrough F0.L1 @4\n"
        "  F0.L1 @4..6 term@4 bytes=81 13 ; emit.1 -> layout.1 -> bb.1\n"
        "    return\n");

    if (!machine_bytes_report_find_block_summary_by_function_name_and_offset(&bytes_report, "main", 4, NULL, &block_summary) ||
        !block_summary || strcmp(block_summary->label_name, "F0.L1") != 0) {
        fprintf(stderr, "[machine-bytes] FAIL: emit artifact report query mismatch\n");
        ok = 0;
    }
    if (!machine_bytes_build_report_from_machine_emit_report_dump(&emit_report, &actual_text, &bytes_error) ||
        !actual_text ||
        strstr(actual_text, "machine_bytes-report total_bytes=6 call_funcs=0 fallthrough_funcs=1 branch_funcs=0 total_block_summaries=2\n") != actual_text ||
        strstr(actual_text, "fn.0 main span=0..6 blocks=2 bytes=6 op_bytes=2 calls=0 blocks_with_calls=0 term_bytes=4") == NULL ||
        strstr(actual_text, "emit.1 label=F0.L1 layout.1 bb.1 start=4 term=4 end=6 bytes=2\n") == NULL) {
        fprintf(stderr, "[machine-bytes] FAIL: emit artifact report dump mismatch\nactual:\n%s\n",
            actual_text ? actual_text : "<null>");
        ok = 0;
    }

    free(actual_text);
    machine_emit_program_free(&emit_program);
    machine_emit_lower_report_free(&emit_report);
    machine_bytes_program_free(&bytes_program);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

static int test_machine_bytes_encodes_call_payloads(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineBytesReport bytes_report;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    const MachineBytesReferenceSummary *reference_summary = NULL;
    const MachineBytesReferenceSummary *function_references = NULL;
    const MachineBytesFixupSummary *fixup_summary = NULL;
    const MachineBytesFixupSummary *function_fixups = NULL;
    const MachineBytesSymbolSummary *symbol_summary = NULL;
    const MachineBytesSectionSummary *section_summary = NULL;
    const MachineBytesSymbolSummary *section_symbols = NULL;
    const MachineBytesFixupSummary *section_fixups = NULL;
    size_t reference_count = 0;
    size_t fixup_count = 0;
    size_t symbol_count = 0;
    size_t section_count = 0;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);
    machine_bytes_report_init(&bytes_report);

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
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1;
    emit_program.functions[0].blocks[0].op_capacity = 1;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_VOID_IMM;
    emit_program.functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("sink");
    emit_program.functions[0].blocks[0].ops[0].as.call.arg_count = 1;
    emit_program.functions[0].blocks[0].ops[0].as.call.args = (MachineEmitOperand *)calloc(1, sizeof(MachineEmitOperand));
    if (!emit_program.functions[0].blocks[0].ops[0].as.call.callee_name ||
        !emit_program.functions[0].blocks[0].ops[0].as.call.args) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].as.call.args[0] = machine_select_operand_immediate(5);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_bytes_lower_program_from_machine_encode(&encode_program, &bytes_program, &bytes_error) ||
        !machine_bytes_build_report_from_program(&bytes_program, &bytes_report, &bytes_error)) {
        fprintf(stderr, "[machine-bytes] FAIL: call payload lower failed: %s / %s\n",
            encode_error.message,
            bytes_error.message);
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    ok &= expect_dump(
        &bytes_program,
        "machine_bytes\n"
        "function main params=0 locals=0 spills=0 bytes=5\n"
        "  F0.L0 @0..5 term@3 bytes=1B 01 15 81 10 ; emit.0 -> layout.0 -> bb.0\n"
        "    return\n");

    if (!machine_bytes_report_get_reference_summary_count(&bytes_report, &reference_count) ||
        reference_count != 1 ||
        !machine_bytes_report_get_function_reference_summaries(&bytes_report, 0, &reference_count, &function_references) ||
        reference_count != 1 || !function_references ||
        !machine_bytes_report_get_fixup_summary_count(&bytes_report, &fixup_count) ||
        fixup_count != 1 ||
        !machine_bytes_report_get_function_fixup_summaries(&bytes_report, 0, &fixup_count, &function_fixups) ||
        fixup_count != 1 || !function_fixups ||
        !machine_bytes_report_get_symbol_summary_count(&bytes_report, &symbol_count) ||
        symbol_count != 3 ||
        !machine_bytes_report_get_section_summary_count(&bytes_report, &section_count) ||
        section_count != 1 ||
        !machine_bytes_report_get_section_summary(&bytes_report, 0, &section_summary) ||
        !section_summary || strcmp(section_summary->name, ".text") != 0 ||
        section_summary->byte_count != 5 || section_summary->symbol_count != 2 || section_summary->fixup_count != 1 ||
        !machine_bytes_report_get_section_symbol_summaries(&bytes_report, 0, &symbol_count, &section_symbols) ||
        symbol_count != 2 || !section_symbols ||
        !machine_bytes_report_get_section_fixup_summaries(&bytes_report, 0, &fixup_count, &section_fixups) ||
        fixup_count != 1 || !section_fixups ||
        !machine_bytes_report_find_symbol_summary_by_name(&bytes_report, "sink", NULL, &symbol_summary) ||
        !symbol_summary || symbol_summary->kind != MACHINE_BYTES_SYMBOL_EXTERNAL ||
        symbol_summary->is_defined || symbol_summary->incoming_fixup_count != 1 ||
        function_references[0].kind != MACHINE_BYTES_REFERENCE_CALL ||
        !function_references[0].target_name || strcmp(function_references[0].target_name, "sink") != 0 ||
        function_references[0].owner_byte_offset != 0 ||
        function_references[0].owner_byte_count != 3 ||
        function_references[0].patch_byte_offset != 0 ||
        function_references[0].patch_byte_count != 0 ||
        function_references[0].op_kind != MACHINE_SELECT_OP_CALL_VOID_IMM ||
        !machine_bytes_report_get_reference_summary(&bytes_report, 0, &reference_summary) ||
        !reference_summary ||
        reference_summary->kind != MACHINE_BYTES_REFERENCE_CALL ||
        !machine_bytes_report_get_fixup_summary(&bytes_report, 0, &fixup_summary) ||
        !fixup_summary ||
        fixup_summary->kind != MACHINE_BYTES_FIXUP_CALL_TARGET ||
        fixup_summary->target_kind != MACHINE_BYTES_FIXUP_TARGET_SYMBOL ||
        !fixup_summary->has_target_symbol_index ||
        !fixup_summary->target_name || strcmp(fixup_summary->target_name, "sink") != 0 ||
        fixup_summary->patch_byte_count != 0 ||
        fixup_summary->owner_byte_count != 3) {
        fprintf(stderr, "[machine-bytes] FAIL: call reference summary mismatch\n");
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_bytes_lowers_from_machine_encode();
    ok &= test_machine_bytes_report_and_bridges();
    ok &= test_machine_bytes_riscv32_preview_profile_changes_text_bytes();
    ok &= test_machine_bytes_from_machine_emit_artifacts();
    ok &= test_machine_bytes_encodes_call_payloads();
    ok &= test_machine_bytes_program_level_offsets_and_byte_image();

    if (!ok) {
        return 1;
    }

    printf("[machine-bytes] PASS\n");
    return 0;
}
