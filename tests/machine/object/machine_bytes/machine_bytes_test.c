#include "machine/bytes.h"

#include "machine/emit.h"
#include "machine/ir.h"

#include <stdint.h>
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

static uint32_t read_u32_le(const unsigned char *bytes) {
    return ((uint32_t)bytes[0]) |
        ((uint32_t)bytes[1] << 8u) |
        ((uint32_t)bytes[2] << 16u) |
        ((uint32_t)bytes[3] << 24u);
}

static int decode_riscv_j_imm(uint32_t word) {
    int imm = 0;

    imm |= (int)(((word >> 21u) & 0x3FFu) << 1u);
    imm |= (int)(((word >> 20u) & 0x1u) << 11u);
    imm |= (int)(((word >> 12u) & 0xFFu) << 12u);
    imm |= (int)(((word >> 31u) & 0x1u) << 20u);
    if ((imm & (1 << 20)) != 0) {
        imm |= ~((1 << 21) - 1);
    }
    return imm;
}

static int decode_riscv_b_imm(uint32_t word) {
    int imm = 0;

    imm |= (int)(((word >> 8u) & 0xFu) << 1u);
    imm |= (int)(((word >> 25u) & 0x3Fu) << 5u);
    imm |= (int)(((word >> 7u) & 0x1u) << 11u);
    imm |= (int)(((word >> 31u) & 0x1u) << 12u);
    if ((imm & (1 << 12)) != 0) {
        imm |= ~((1 << 13) - 1);
    }
    return imm;
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
    static const unsigned char expected_program_bytes[] = {0x23u, 0x81u, 0x83u, 0x01u, 0x81u, 0x17u};
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.global_count = 1u;
    emit_program.global_capacity = 1u;
    emit_program.globals = (MachineEmitGlobal *)calloc(1u, sizeof(MachineEmitGlobal));
    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.globals || !emit_program.functions) {
        return 0;
    }
    emit_program.globals[0].id = 0u;
    emit_program.globals[0].name = dup_text("g");
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 2;
    emit_program.functions[0].block_capacity = 2;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(2, sizeof(MachineEmitBlock));
    if (!emit_program.globals[0].name ||
        !emit_program.functions[0].name || !emit_program.functions[0].blocks) {
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
        "  F0.L0 @0..4 term@2 bytes=23 81 83 01 ; emit.0 -> layout.0 -> bb.0\n"
        "    fallthrough F0.L1 @4\n"
        "  F0.L1 @4..6 term@4 bytes=81 17 ; emit.1 -> layout.1 -> bb.1\n"
        "    reti 7\n");

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

static int test_machine_bytes_riscv32_preview_emits_void_return_word(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    MachineBytesTerminatorSummary terminator_summary;
    const MachineBytesFunction *function_view = NULL;
    size_t function_index = 0u;
    unsigned char *program_bytes = NULL;
    size_t program_byte_count = 0u;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_bytes_report_init(&bytes_report);

    emit_program.register_bank.register_count = 8u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(8u, sizeof(MachineEmitRegisterDesc));
    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.functions) {
        return 0;
    }
    for (size_t reg_index = 0u; reg_index < 8u; ++reg_index) {
        char name_buf[8];

        snprintf(name_buf, sizeof(name_buf), "r%zu", reg_index);
        emit_program.register_bank.registers[reg_index].register_id = reg_index;
        emit_program.register_bank.registers[reg_index].name = dup_text(name_buf);
        emit_program.register_bank.registers[reg_index].allocatable = 1u;
        if (!emit_program.register_bank.registers[reg_index].name) {
            machine_emit_program_free(&emit_program);
            machine_bytes_program_free(&bytes_program);
            return 0;
        }
    }

    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_none();
    if (!emit_program.functions[0].blocks[0].label_name) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_build_report_from_program(&bytes_program, &bytes_report, &bytes_error) ||
        !machine_bytes_program_get_function_by_name(&bytes_program, "main", &function_index, &function_view) ||
        function_index != 0u ||
        !function_view ||
        !machine_bytes_function_get_block_terminator_summary(function_view, 0u, &terminator_summary) ||
        terminator_summary.kind != MACHINE_LAYOUT_TERM_RETURN ||
        terminator_summary.has_return_value ||
        terminator_summary.return_value.kind != MACHINE_SELECT_OPERAND_NONE ||
        !machine_bytes_report_get_block_terminator_summary(&bytes_report, 0u, 0u, &terminator_summary) ||
        terminator_summary.kind != MACHINE_LAYOUT_TERM_RETURN ||
        terminator_summary.has_return_value ||
        terminator_summary.return_value.kind != MACHINE_SELECT_OPERAND_NONE ||
        !machine_bytes_program_copy_bytes(&bytes_program, &program_bytes, &program_byte_count, &bytes_error) ||
        program_byte_count != 4u ||
        read_u32_le(program_bytes) != 0x00008067u) {
        fprintf(stderr, "[machine-bytes] FAIL: preview void-return mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(program_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_bytes_report_free(&bytes_report);
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
    static const unsigned char expected_report_bytes[] = {0x1Eu, 0x8Au, 0xEAu, 0x00u, 0x21u, 0x81u, 0x11u, 0x81u, 0x12u};
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

static int test_machine_bytes_report_query_helpers_reject_malformed_summary_tables(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    MachineBytesReferenceSummary *saved_reference_summaries = NULL;
    MachineBytesFixupSummary *saved_fixup_summaries = NULL;
    MachineBytesBlockSummary *saved_block_summaries = NULL;
    size_t *saved_function_byte_offsets = NULL;
    const MachineBytesReferenceSummary *function_references = NULL;
    const MachineBytesFixupSummary *function_fixups = NULL;
    const MachineBytesBlockSummary *block_summary = NULL;
    size_t count = 0u;
    size_t function_index = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
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
        fprintf(stderr, "[machine-bytes] FAIL: malformed-report setup failed: %s\n", machine_error.message);
        ok = 0;
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-bytes] FAIL: malformed-report setup failed: %s\n", machine_error.message);
        ok = 0;
        goto cleanup;
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
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_bytes_build_report_from_machine_ir_report(&machine_report, &bytes_report, &bytes_error)) {
        fprintf(stderr, "[machine-bytes] FAIL: malformed-report bridge failed: %s\n", bytes_error.message);
        ok = 0;
        goto cleanup;
    }

    saved_reference_summaries = bytes_report.reference_summaries;
    bytes_report.reference_summaries = NULL;
    if (machine_bytes_report_get_function_reference_summaries(
            &bytes_report, 0u, &count, &function_references) ||
        machine_bytes_report_get_reference_summary(&bytes_report, 0u, &function_references)) {
        fprintf(stderr, "[machine-bytes] FAIL: malformed reference-summary query unexpectedly succeeded\n");
        ok = 0;
    }
    bytes_report.reference_summaries = saved_reference_summaries;

    saved_fixup_summaries = bytes_report.fixup_summaries;
    bytes_report.fixup_summaries = NULL;
    if (machine_bytes_report_get_function_fixup_summaries(&bytes_report, 0u, &count, &function_fixups) ||
        machine_bytes_report_get_fixup_summary(&bytes_report, 0u, &function_fixups)) {
        fprintf(stderr, "[machine-bytes] FAIL: malformed fixup-summary query unexpectedly succeeded\n");
        ok = 0;
    }
    bytes_report.fixup_summaries = saved_fixup_summaries;

    saved_function_byte_offsets = bytes_report.function_byte_offsets;
    saved_block_summaries = bytes_report.block_summaries;
    bytes_report.function_byte_offsets = NULL;
    bytes_report.block_summaries = NULL;
    if (machine_bytes_report_get_function_byte_span(&bytes_report, 0u, NULL, NULL) ||
        machine_bytes_report_find_block_summary_by_program_byte_offset(
            &bytes_report, 0u, &function_index, &block_summary) ||
        machine_bytes_report_get_block_summary(&bytes_report, 0u, 0u, &block_summary)) {
        fprintf(stderr, "[machine-bytes] FAIL: malformed block-summary query unexpectedly succeeded\n");
        ok = 0;
    }
    bytes_report.function_byte_offsets = saved_function_byte_offsets;
    bytes_report.block_summaries = saved_block_summaries;

    if (bytes_report.section_summaries) {
        size_t saved_symbol_start = bytes_report.section_summaries[0].symbol_start_index;
        size_t saved_symbol_count = bytes_report.section_summaries[0].symbol_count;
        size_t saved_fixup_start = bytes_report.section_summaries[0].fixup_start_index;
        size_t saved_fixup_count = bytes_report.section_summaries[0].fixup_count;

        bytes_report.section_summaries[0].symbol_start_index = bytes_report.total_symbol_summary_count;
        bytes_report.section_summaries[0].symbol_count = 1u;
        if (machine_bytes_report_get_section_symbol_summaries(&bytes_report, 0u, &count, NULL)) {
            fprintf(stderr, "[machine-bytes] FAIL: malformed section symbol slice unexpectedly succeeded\n");
            ok = 0;
        }
        bytes_report.section_summaries[0].symbol_start_index = saved_symbol_start;
        bytes_report.section_summaries[0].symbol_count = saved_symbol_count;

        bytes_report.section_summaries[0].fixup_start_index = bytes_report.total_fixup_summary_count;
        bytes_report.section_summaries[0].fixup_count = 1u;
        if (machine_bytes_report_get_section_fixup_summaries(&bytes_report, 0u, &count, NULL)) {
            fprintf(stderr, "[machine-bytes] FAIL: malformed section fixup slice unexpectedly succeeded\n");
            ok = 0;
        }
        bytes_report.section_summaries[0].fixup_start_index = saved_fixup_start;
        bytes_report.section_summaries[0].fixup_count = saved_fixup_count;
    }

cleanup:
    bytes_report.reference_summaries =
        saved_reference_summaries ? saved_reference_summaries : bytes_report.reference_summaries;
    bytes_report.fixup_summaries = saved_fixup_summaries ? saved_fixup_summaries : bytes_report.fixup_summaries;
    bytes_report.function_byte_offsets =
        saved_function_byte_offsets ? saved_function_byte_offsets : bytes_report.function_byte_offsets;
    bytes_report.block_summaries = saved_block_summaries ? saved_block_summaries : bytes_report.block_summaries;
    machine_bytes_report_free(&bytes_report);
    machine_ir_allocate_rewrite_report_free(&machine_report);
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
    MachineBytesTargetPolicySummary generic_policy_summary;
    MachineBytesTargetPolicySummary preview_program_policy_summary;
    const MachineBytesTargetPolicySummary *preview_report_policy_summary = NULL;
    const MachineBytesFixupSummary *fixup = NULL;
    const MachineBytesFunctionSummary *function_summary = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&generic_policy_summary, 0, sizeof(generic_policy_summary));
    memset(&preview_program_policy_summary, 0, sizeof(preview_program_policy_summary));
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
        preview_byte_count != 24u ||
        preview_byte_count % 4u != 0u ||
        !generic_bytes || !preview_bytes ||
        memcmp(generic_bytes, preview_bytes, generic_byte_count) == 0 ||
        !machine_bytes_get_target_policy_summary(MACHINE_BYTES_TARGET_PROFILE_GENERIC, &generic_policy_summary) ||
        generic_policy_summary.target_profile != MACHINE_BYTES_TARGET_PROFILE_GENERIC ||
        generic_policy_summary.max_logical_machine_register_count != 0u ||
        generic_policy_summary.preserves_known_internal_pc_relative_targets ||
        generic_policy_summary.preserves_direct_fallthrough_honesty ||
        generic_policy_summary.uses_paired_global_data_addressing ||
        generic_policy_summary.supports_rv32m_alu_ops ||
        !machine_bytes_program_get_target_policy_summary(&preview_report.program, &preview_program_policy_summary) ||
        preview_program_policy_summary.target_profile != MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW ||
        preview_program_policy_summary.max_logical_machine_register_count != 8u ||
        !preview_program_policy_summary.preserves_known_internal_pc_relative_targets ||
        !preview_program_policy_summary.preserves_direct_fallthrough_honesty ||
        !preview_program_policy_summary.uses_paired_global_data_addressing ||
        !preview_program_policy_summary.supports_rv32m_alu_ops ||
        !machine_bytes_report_get_target_policy_summary_artifact(
            &preview_report, &preview_report_policy_summary) ||
        !machine_bytes_verify_current_riscv32_preview_compatibility(&preview_report.program, &bytes_error) ||
        !machine_bytes_report_verify_current_riscv32_preview_compatibility(&preview_report, &bytes_error) ||
        !preview_report_policy_summary ||
        preview_report_policy_summary->target_profile != MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW ||
        preview_report_policy_summary->max_logical_machine_register_count != 8u ||
        !preview_report_policy_summary->preserves_known_internal_pc_relative_targets ||
        !preview_report_policy_summary->preserves_direct_fallthrough_honesty ||
        !preview_report_policy_summary->uses_paired_global_data_addressing ||
        !preview_report_policy_summary->supports_rv32m_alu_ops ||
        !machine_bytes_report_get_fixup_summary_count(&preview_report, &fixup_count) ||
        fixup_count != 1u ||
        !machine_bytes_report_get_fixup_summary(&preview_report, 0u, &fixup) || !fixup ||
        fixup->kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        fixup->patch_byte_offset != 4u ||
        fixup->patch_byte_count != 4u ||
        fixup->owner_byte_count != 4u ||
        decode_riscv_b_imm(read_u32_le(preview_bytes + 4u)) != 12 ||
        !machine_bytes_report_get_function_summary_artifact(&preview_report, 0u, &function_summary) ||
        !function_summary ||
        function_summary->byte_count != 24u ||
        function_summary->op_byte_count != 4u ||
        function_summary->terminator_byte_count != 20u) {
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

static int test_machine_bytes_riscv32_preview_patches_internal_call_targets(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    unsigned char *preview_bytes = NULL;
    size_t preview_byte_count = 0u;
    size_t fixup_count = 0u;
    const MachineBytesFixupSummary *fixup = NULL;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_bytes_report_init(&bytes_report);

    emit_program.function_count = 2;
    emit_program.function_capacity = 2;
    emit_program.functions = (MachineEmitFunction *)calloc(2, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    emit_program.functions[1].name = dup_text("sink");
    emit_program.functions[1].has_body = 1;
    emit_program.functions[1].block_count = 1;
    emit_program.functions[1].block_capacity = 1;
    emit_program.functions[1].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks ||
        !emit_program.functions[1].name || !emit_program.functions[1].blocks) {
        machine_emit_program_free(&emit_program);
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
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_VOID;
    emit_program.functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("sink");
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;

    emit_program.functions[1].blocks[0].emit_index = 0;
    emit_program.functions[1].blocks[0].original_layout_index = 0;
    emit_program.functions[1].blocks[0].original_block_id = 0;
    emit_program.functions[1].blocks[0].label_name = dup_text("F1.L0");
    emit_program.functions[1].blocks[0].has_terminator = 1;
    emit_program.functions[1].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;

    if (!emit_program.functions[0].blocks[0].ops[0].as.call.callee_name ||
        !machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_build_report_from_program(&bytes_program, &bytes_report, &bytes_error) ||
        !machine_bytes_report_copy_bytes(&bytes_report, &preview_bytes, &preview_byte_count, &bytes_error) ||
        preview_byte_count != 16u ||
        (read_u32_le(preview_bytes + 0u) & 0x7Fu) != 0x17u ||
        (read_u32_le(preview_bytes + 4u) & 0x7Fu) != 0x67u ||
        !machine_bytes_report_get_fixup_summary_count(&bytes_report, &fixup_count) ||
        fixup_count != 1u ||
        !machine_bytes_report_get_fixup_summary(&bytes_report, 0u, &fixup) || !fixup ||
        fixup->kind != MACHINE_BYTES_FIXUP_CALL_TARGET ||
        !fixup->has_target_function_index ||
        fixup->target_function_index != 1u ||
        !fixup->has_target_byte_offset ||
        fixup->target_byte_offset != 12u ||
        fixup->patch_byte_offset != 0u ||
        fixup->patch_byte_count != 4u) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview internal call mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(preview_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

static int test_machine_bytes_riscv32_preview_uses_real_fallthrough_control(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    unsigned char *preview_bytes = NULL;
    size_t preview_byte_count = 0u;
    size_t fixup_count = 0u;
    const MachineBytesFixupSummary *fixup = NULL;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_bytes_report_init(&bytes_report);

    emit_program.register_bank.register_count = 1u;
    emit_program.register_bank.registers =
        (MachineEmitRegisterDesc *)calloc(1u, sizeof(MachineEmitRegisterDesc));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.functions) {
        return 0;
    }
    emit_program.register_bank.registers[0].register_id = 0u;
    emit_program.register_bank.registers[0].name = dup_text("r0");
    emit_program.register_bank.registers[0].allocatable = 1u;

    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 4u;
    emit_program.functions[0].block_capacity = 4u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(4u, sizeof(MachineEmitBlock));
    if (!emit_program.register_bank.registers[0].name ||
        !emit_program.functions[0].name ||
        !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0u;
    emit_program.functions[0].blocks[0].original_layout_index = 0u;
    emit_program.functions[0].blocks[0].original_block_id = 0u;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_FALLTHROUGH;
    emit_program.functions[0].blocks[0].terminator.as.fallthrough_target = 1u;

    emit_program.functions[0].blocks[1].emit_index = 1u;
    emit_program.functions[0].blocks[1].original_layout_index = 1u;
    emit_program.functions[0].blocks[1].original_block_id = 1u;
    emit_program.functions[0].blocks[1].label_name = dup_text("F0.L1");
    emit_program.functions[0].blocks[1].has_terminator = 1;
    emit_program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH;
    emit_program.functions[0].blocks[1].terminator.as.branch_fallthrough.condition = machine_select_operand_register(0u);
    emit_program.functions[0].blocks[1].terminator.as.branch_fallthrough.taken_target = 3u;
    emit_program.functions[0].blocks[1].terminator.as.branch_fallthrough.fallthrough_target = 2u;
    emit_program.functions[0].blocks[1].terminator.as.branch_fallthrough.branch_on_true = 0u;

    emit_program.functions[0].blocks[2].emit_index = 2u;
    emit_program.functions[0].blocks[2].original_layout_index = 2u;
    emit_program.functions[0].blocks[2].original_block_id = 2u;
    emit_program.functions[0].blocks[2].label_name = dup_text("F0.L2");
    emit_program.functions[0].blocks[2].has_terminator = 1;
    emit_program.functions[0].blocks[2].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;

    emit_program.functions[0].blocks[3].emit_index = 3u;
    emit_program.functions[0].blocks[3].original_layout_index = 3u;
    emit_program.functions[0].blocks[3].original_block_id = 3u;
    emit_program.functions[0].blocks[3].label_name = dup_text("F0.L3");
    emit_program.functions[0].blocks[3].has_terminator = 1;
    emit_program.functions[0].blocks[3].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;

    if (!emit_program.functions[0].blocks[0].label_name ||
        !emit_program.functions[0].blocks[1].label_name ||
        !emit_program.functions[0].blocks[2].label_name ||
        !emit_program.functions[0].blocks[3].label_name ||
        !machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_build_report_from_program(&bytes_program, &bytes_report, &bytes_error) ||
        !machine_bytes_report_copy_bytes(&bytes_report, &preview_bytes, &preview_byte_count, &bytes_error) ||
        preview_byte_count != 12u ||
        (read_u32_le(preview_bytes + 0u) & 0x7Fu) != 0x63u ||
        ((read_u32_le(preview_bytes + 0u) >> 12u) & 0x7u) != 0x0u ||
        decode_riscv_b_imm(read_u32_le(preview_bytes + 0u)) != 8 ||
        read_u32_le(preview_bytes + 4u) != 0x00008067u ||
        read_u32_le(preview_bytes + 8u) != 0x00008067u ||
        !machine_bytes_report_get_fixup_summary_count(&bytes_report, &fixup_count) ||
        fixup_count != 1u ||
        !machine_bytes_report_get_fixup_summary(&bytes_report, 0u, &fixup) || !fixup ||
        fixup->kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        fixup->patch_byte_offset != 0u ||
        fixup->patch_byte_count != 4u ||
        fixup->owner_byte_count != 4u) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview real fallthrough control mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(preview_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

static int test_machine_bytes_riscv32_preview_expands_large_immediates(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    unsigned char *preview_bytes = NULL;
    size_t preview_byte_count = 0u;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.register_bank.register_count = 1u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(1u, sizeof(MachineEmitRegisterDesc));
    emit_program.global_count = 1;
    emit_program.global_capacity = 1;
    emit_program.globals = (MachineEmitGlobal *)calloc(1, sizeof(MachineEmitGlobal));
    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.globals || !emit_program.functions) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }
    emit_program.register_bank.registers[0].register_id = 0u;
    emit_program.register_bank.registers[0].name = dup_text("r0");
    emit_program.register_bank.registers[0].allocatable = 1u;
    emit_program.globals[0].id = 0u;
    emit_program.globals[0].name = dup_text("g");
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.register_bank.registers[0].name ||
        !emit_program.globals[0].name || !emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 2;
    emit_program.functions[0].blocks[0].op_capacity = 2;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(2, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
    emit_program.functions[0].blocks[0].ops[0].has_result = 1;
    emit_program.functions[0].blocks[0].ops[0].result = machine_select_operand_register(0);
    emit_program.functions[0].blocks[0].ops[0].as.copy_value = machine_select_operand_immediate(4097);
    emit_program.functions[0].blocks[0].ops[1].kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
    emit_program.functions[0].blocks[0].ops[1].as.store.slot = machine_select_slot_global(0);
    emit_program.functions[0].blocks[0].ops[1].as.store.value = machine_select_operand_immediate(5000);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(6000);

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_program_copy_bytes(&bytes_program, &preview_bytes, &preview_byte_count, &bytes_error) ||
        preview_byte_count != 36u ||
        read_u32_le(preview_bytes + 0u) != 0x00001537u ||
        read_u32_le(preview_bytes + 4u) != 0x00150513u ||
        (read_u32_le(preview_bytes + 8u) & 0x7Fu) != 0x37u ||
        (read_u32_le(preview_bytes + 12u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 16u) & 0x7Fu) != 0x37u ||
        (read_u32_le(preview_bytes + 20u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 24u) & 0x7Fu) != 0x37u ||
        (read_u32_le(preview_bytes + 28u) & 0x7Fu) != 0x13u ||
        read_u32_le(preview_bytes + 32u) != 0x00008067u) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview large immediate expansion mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(preview_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_riscv32_preview_expands_nonzero_compare_branch_immediates(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    unsigned char *preview_bytes = NULL;
    size_t preview_byte_count = 0u;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.register_bank.register_count = 8u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(8u, sizeof(MachineEmitRegisterDesc));
    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.functions) {
        return 0;
    }
    for (size_t reg_index = 0u; reg_index < 8u; ++reg_index) {
        char name_buf[8];

        snprintf(name_buf, sizeof(name_buf), "r%zu", reg_index);
        emit_program.register_bank.registers[reg_index].register_id = reg_index;
        emit_program.register_bank.registers[reg_index].name = dup_text(name_buf);
        emit_program.register_bank.registers[reg_index].allocatable = 1u;
        if (!emit_program.register_bank.registers[reg_index].name) {
            machine_emit_program_free(&emit_program);
            machine_bytes_program_free(&bytes_program);
            return 0;
        }
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 3;
    emit_program.functions[0].block_capacity = 3;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(3, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
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
    emit_program.functions[0].blocks[1].emit_index = 1;
    emit_program.functions[0].blocks[1].original_layout_index = 1;
    emit_program.functions[0].blocks[1].original_block_id = 1;
    emit_program.functions[0].blocks[1].label_name = dup_text("F0.L1");
    emit_program.functions[0].blocks[2].emit_index = 2;
    emit_program.functions[0].blocks[2].original_layout_index = 2;
    emit_program.functions[0].blocks[2].original_block_id = 2;
    emit_program.functions[0].blocks[2].label_name = dup_text("F0.L2");
    if (!emit_program.functions[0].blocks[0].label_name ||
        !emit_program.functions[0].blocks[0].ops ||
        !emit_program.functions[0].blocks[1].label_name ||
        !emit_program.functions[0].blocks[2].label_name) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
    emit_program.functions[0].blocks[0].ops[0].has_result = 1;
    emit_program.functions[0].blocks[0].ops[0].result = machine_select_operand_register(0);
    emit_program.functions[0].blocks[0].ops[0].as.copy_value = machine_select_operand_immediate(1);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM;
    emit_program.functions[0].blocks[0].terminator.as.compare_branch.op = MACHINE_IR_BINARY_LT;
    emit_program.functions[0].blocks[0].terminator.as.compare_branch.lhs = machine_select_operand_register(0);
    emit_program.functions[0].blocks[0].terminator.as.compare_branch.rhs = machine_select_operand_immediate(5);
    emit_program.functions[0].blocks[0].terminator.as.compare_branch.then_target = 1u;
    emit_program.functions[0].blocks[0].terminator.as.compare_branch.else_target = 2u;
    emit_program.functions[0].blocks[1].has_terminator = 1;
    emit_program.functions[0].blocks[1].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    emit_program.functions[0].blocks[2].has_terminator = 1;
    emit_program.functions[0].blocks[2].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_program_copy_bytes(&bytes_program, &preview_bytes, &preview_byte_count, &bytes_error) ||
        preview_byte_count != 24u ||
        read_u32_le(preview_bytes + 0u) != 0x00100513u ||
        (read_u32_le(preview_bytes + 4u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 8u) & 0x7Fu) != 0x63u ||
        (read_u32_le(preview_bytes + 12u) & 0x7Fu) != 0x6Fu) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview compare-branch immediate expansion mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(preview_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_riscv32_preview_rejects_out_of_range_branch_targets(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    size_t op_index;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.register_bank.register_count = 1u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(1u, sizeof(MachineEmitRegisterDesc));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.functions) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
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
        machine_bytes_program_free(&bytes_program);
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
        machine_bytes_program_free(&bytes_program);
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
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    if (machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        strstr(bytes_error.message, "MACHINE-BYTES-344") == NULL) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview out-of-range branch was not rejected: %s\n",
            bytes_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_rejects_generic_branch_targets_that_do_not_fit_bytecode(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    char label[32];
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.register_bank.register_count = 1u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(1u, sizeof(MachineEmitRegisterDesc));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.functions) {
        return 0;
    }
    emit_program.register_bank.registers[0].register_id = 0u;
    emit_program.register_bank.registers[0].name = dup_text("r0");
    emit_program.register_bank.registers[0].allocatable = 1u;
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 17u;
    emit_program.functions[0].block_capacity = 17u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(17u, sizeof(MachineEmitBlock));
    if (!emit_program.register_bank.registers[0].name ||
        !emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    for (size_t block_index = 0u; block_index < 17u; ++block_index) {
        snprintf(label, sizeof(label), "F0.L%zu", block_index);
        emit_program.functions[0].blocks[block_index].emit_index = block_index;
        emit_program.functions[0].blocks[block_index].original_layout_index = block_index;
        emit_program.functions[0].blocks[block_index].original_block_id = block_index;
        emit_program.functions[0].blocks[block_index].label_name = dup_text(label);
        emit_program.functions[0].blocks[block_index].has_terminator = 1u;
        emit_program.functions[0].blocks[block_index].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
        if (!emit_program.functions[0].blocks[block_index].label_name) {
            machine_emit_program_free(&emit_program);
            machine_bytes_program_free(&bytes_program);
            return 0;
        }
    }
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_BRANCH;
    emit_program.functions[0].blocks[0].terminator.as.branch.condition = machine_select_operand_register(0u);
    emit_program.functions[0].blocks[0].terminator.as.branch.then_target = 16u;
    emit_program.functions[0].blocks[0].terminator.as.branch.else_target = 1u;

    if (machine_bytes_lower_program_from_machine_emit(&emit_program, &bytes_program, &bytes_error) ||
        strstr(bytes_error.message, "MACHINE-BYTES-139") == NULL) {
        fprintf(stderr,
            "[machine-bytes] FAIL: generic branch target width check mismatch: %s\n",
            bytes_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_rejects_generic_immediates_that_do_not_fit_bytecode(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.register_bank.register_count = 1u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(1u, sizeof(MachineEmitRegisterDesc));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.functions) {
        return 0;
    }
    emit_program.register_bank.registers[0].register_id = 0u;
    emit_program.register_bank.registers[0].name = dup_text("r0");
    emit_program.register_bank.registers[0].allocatable = 1u;
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].local_count = 1u;
    emit_program.functions[0].block_count = 1u;
    emit_program.functions[0].block_capacity = 1u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!emit_program.register_bank.registers[0].name ||
        !emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0u;
    emit_program.functions[0].blocks[0].original_layout_index = 0u;
    emit_program.functions[0].blocks[0].original_block_id = 0u;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1u;
    emit_program.functions[0].blocks[0].op_capacity = 1u;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1u, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
    emit_program.functions[0].blocks[0].ops[0].has_result = 1u;
    emit_program.functions[0].blocks[0].ops[0].result = machine_select_operand_register(0u);
    emit_program.functions[0].blocks[0].ops[0].as.copy_value = machine_select_operand_immediate(300u);
    emit_program.functions[0].blocks[0].has_terminator = 1u;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(17u);

    if (machine_bytes_lower_program_from_machine_emit(&emit_program, &bytes_program, &bytes_error) ||
        strstr(bytes_error.message, "MACHINE-BYTES-144") == NULL) {
        fprintf(stderr,
            "[machine-bytes] FAIL: generic immediate width check mismatch: %s\n",
            bytes_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_riscv32_preview_materializes_call_args_and_spill_results(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    unsigned char *preview_bytes = NULL;
    size_t preview_byte_count = 0u;
    size_t fixup_count = 0u;
    const MachineBytesFixupSummary *fixup = NULL;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_bytes_report_init(&bytes_report);

    emit_program.function_count = 2;
    emit_program.function_capacity = 2;
    emit_program.functions = (MachineEmitFunction *)calloc(2, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].spill_slot_count = 1;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    emit_program.functions[1].name = dup_text("sink");
    emit_program.functions[1].has_body = 1;
    emit_program.functions[1].block_count = 1;
    emit_program.functions[1].block_capacity = 1;
    emit_program.functions[1].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks ||
        !emit_program.functions[1].name || !emit_program.functions[1].blocks) {
        machine_emit_program_free(&emit_program);
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
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_IMM_SPILL;
    emit_program.functions[0].blocks[0].ops[0].has_result = 1;
    emit_program.functions[0].blocks[0].ops[0].result = machine_select_operand_spill_slot(0);
    emit_program.functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("sink");
    emit_program.functions[0].blocks[0].ops[0].as.call.arg_count = 1;
    emit_program.functions[0].blocks[0].ops[0].as.call.args = (MachineEmitOperand *)calloc(1, sizeof(MachineEmitOperand));
    if (!emit_program.functions[0].blocks[0].ops[0].as.call.callee_name ||
        !emit_program.functions[0].blocks[0].ops[0].as.call.args) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].as.call.args[0] = machine_select_operand_immediate(5000);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_SPILL;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_spill_slot(0);

    emit_program.functions[1].blocks[0].emit_index = 0;
    emit_program.functions[1].blocks[0].original_layout_index = 0;
    emit_program.functions[1].blocks[0].original_block_id = 0;
    emit_program.functions[1].blocks[0].label_name = dup_text("F1.L0");
    emit_program.functions[1].blocks[0].has_terminator = 1;
    emit_program.functions[1].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[1].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_build_report_from_program(&bytes_program, &bytes_report, &bytes_error) ||
        !machine_bytes_report_copy_bytes(&bytes_report, &preview_bytes, &preview_byte_count, &bytes_error) ||
        preview_byte_count != 36u ||
        (read_u32_le(preview_bytes + 0u) & 0x7Fu) != 0x37u ||
        (read_u32_le(preview_bytes + 4u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 8u) & 0x7Fu) != 0x17u ||
        (read_u32_le(preview_bytes + 12u) & 0x7Fu) != 0x67u ||
        (read_u32_le(preview_bytes + 16u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 20u) & 0x7Fu) != 0x03u ||
        read_u32_le(preview_bytes + 24u) != 0x00008067u ||
        !machine_bytes_report_get_fixup_summary_count(&bytes_report, &fixup_count) ||
        fixup_count != 1u ||
        !machine_bytes_report_get_fixup_summary(&bytes_report, 0u, &fixup) || !fixup ||
        fixup->kind != MACHINE_BYTES_FIXUP_CALL_TARGET ||
        !fixup->has_target_byte_offset ||
        fixup->target_byte_offset != 28u ||
        fixup->patch_byte_offset != 8u ||
        fixup->patch_byte_count != 4u ||
        fixup->owner_byte_count != 20u) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview call arg/result materialization mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(preview_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

static int test_machine_bytes_riscv32_preview_materializes_spill_and_stack_call_args(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    unsigned char *preview_bytes = NULL;
    size_t preview_byte_count = 0u;
    size_t fixup_count = 0u;
    const MachineBytesFixupSummary *fixup = NULL;
    size_t arg_index;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_bytes_report_init(&bytes_report);

    emit_program.function_count = 2;
    emit_program.function_capacity = 2;
    emit_program.functions = (MachineEmitFunction *)calloc(2, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].spill_slot_count = 1;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    emit_program.functions[1].name = dup_text("sink");
    emit_program.functions[1].has_body = 1;
    emit_program.functions[1].block_count = 1;
    emit_program.functions[1].block_capacity = 1;
    emit_program.functions[1].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks ||
        !emit_program.functions[1].name || !emit_program.functions[1].blocks) {
        machine_emit_program_free(&emit_program);
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
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_VOID_IMM;
    emit_program.functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("sink");
    emit_program.functions[0].blocks[0].ops[0].as.call.arg_count = 9u;
    emit_program.functions[0].blocks[0].ops[0].as.call.args = (MachineEmitOperand *)calloc(9, sizeof(MachineEmitOperand));
    if (!emit_program.functions[0].blocks[0].ops[0].as.call.callee_name ||
        !emit_program.functions[0].blocks[0].ops[0].as.call.args) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].as.call.args[0] = machine_select_operand_spill_slot(0);
    for (arg_index = 1u; arg_index < 8u; ++arg_index) {
        emit_program.functions[0].blocks[0].ops[0].as.call.args[arg_index] =
            machine_select_operand_immediate((long long)arg_index);
    }
    emit_program.functions[0].blocks[0].ops[0].as.call.args[8] = machine_select_operand_immediate(99);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);

    emit_program.functions[1].blocks[0].emit_index = 0;
    emit_program.functions[1].blocks[0].original_layout_index = 0;
    emit_program.functions[1].blocks[0].original_block_id = 0;
    emit_program.functions[1].blocks[0].label_name = dup_text("F1.L0");
    emit_program.functions[1].blocks[0].has_terminator = 1;
    emit_program.functions[1].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[1].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_build_report_from_program(&bytes_program, &bytes_report, &bytes_error) ||
        !machine_bytes_report_copy_bytes(&bytes_report, &preview_bytes, &preview_byte_count, &bytes_error) ||
        preview_byte_count != 72u ||
        read_u32_le(preview_bytes + 0u) != 0xff010113u ||
        (read_u32_le(preview_bytes + 4u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 8u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 12u) & 0x7Fu) != 0x03u ||
        ((read_u32_le(preview_bytes + 12u) >> 7u) & 0x1Fu) != 10u ||
        ((read_u32_le(preview_bytes + 12u) >> 20u) & 0xFFFu) != 16u ||
        (read_u32_le(preview_bytes + 40u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 44u) & 0x7Fu) != 0x17u ||
        (read_u32_le(preview_bytes + 48u) & 0x7Fu) != 0x67u ||
        read_u32_le(preview_bytes + 52u) != 0x01010113u ||
        !machine_bytes_report_get_fixup_summary_count(&bytes_report, &fixup_count) ||
        fixup_count != 1u ||
        !machine_bytes_report_get_fixup_summary(&bytes_report, 0u, &fixup) || !fixup ||
        fixup->kind != MACHINE_BYTES_FIXUP_CALL_TARGET ||
        !fixup->has_target_byte_offset ||
        fixup->target_byte_offset != 64u ||
        fixup->patch_byte_offset != 44u ||
        fixup->patch_byte_count != 4u ||
        fixup->owner_byte_count != 56u) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview spill/stack call arg materialization mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(preview_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

static int test_machine_bytes_riscv32_preview_materializes_spill_backed_value_ops(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    unsigned char *preview_bytes = NULL;
    size_t preview_byte_count = 0u;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.register_bank.register_count = 8u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(8u, sizeof(MachineEmitRegisterDesc));
    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.functions) {
        return 0;
    }
    for (size_t reg_index = 0u; reg_index < 8u; ++reg_index) {
        char name_buf[8];

        snprintf(name_buf, sizeof(name_buf), "r%zu", reg_index);
        emit_program.register_bank.registers[reg_index].register_id = reg_index;
        emit_program.register_bank.registers[reg_index].name = dup_text(name_buf);
        emit_program.register_bank.registers[reg_index].allocatable = 1u;
        if (!emit_program.register_bank.registers[reg_index].name) {
            machine_emit_program_free(&emit_program);
            machine_bytes_program_free(&bytes_program);
            return 0;
        }
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].local_count = 2;
    emit_program.functions[0].spill_slot_count = 3;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 4;
    emit_program.functions[0].blocks[0].op_capacity = 4;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(4, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_LOAD_LOCAL;
    emit_program.functions[0].blocks[0].ops[0].has_result = 1;
    emit_program.functions[0].blocks[0].ops[0].result = machine_select_operand_spill_slot(0);
    emit_program.functions[0].blocks[0].ops[0].as.load_slot.kind = MACHINE_SELECT_SLOT_LOCAL;
    emit_program.functions[0].blocks[0].ops[0].as.load_slot.id = 0;

    emit_program.functions[0].blocks[0].ops[1].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
    emit_program.functions[0].blocks[0].ops[1].has_result = 1;
    emit_program.functions[0].blocks[0].ops[1].result = machine_select_operand_spill_slot(1);
    emit_program.functions[0].blocks[0].ops[1].as.copy_value = machine_select_operand_immediate(5000);

    emit_program.functions[0].blocks[0].ops[2].kind = MACHINE_SELECT_OP_ALU;
    emit_program.functions[0].blocks[0].ops[2].has_result = 1;
    emit_program.functions[0].blocks[0].ops[2].result = machine_select_operand_spill_slot(2);
    emit_program.functions[0].blocks[0].ops[2].as.binary.op = MACHINE_IR_BINARY_ADD;
    emit_program.functions[0].blocks[0].ops[2].as.binary.lhs = machine_select_operand_spill_slot(0);
    emit_program.functions[0].blocks[0].ops[2].as.binary.rhs = machine_select_operand_spill_slot(1);

    emit_program.functions[0].blocks[0].ops[3].kind = MACHINE_SELECT_OP_STORE_LOCAL;
    emit_program.functions[0].blocks[0].ops[3].as.store.slot.kind = MACHINE_SELECT_SLOT_LOCAL;
    emit_program.functions[0].blocks[0].ops[3].as.store.slot.id = 1;
    emit_program.functions[0].blocks[0].ops[3].as.store.value = machine_select_operand_spill_slot(2);

    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_SPILL;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_spill_slot(2);

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_program_copy_bytes(&bytes_program, &preview_bytes, &preview_byte_count, &bytes_error) ||
        preview_byte_count != 52u ||
        (read_u32_le(preview_bytes + 0u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 4u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 8u) & 0x7Fu) != 0x37u ||
        (read_u32_le(preview_bytes + 12u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 16u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 20u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 24u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 28u) & 0x7Fu) != 0x33u ||
        (read_u32_le(preview_bytes + 32u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 36u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 40u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 44u) & 0x7Fu) != 0x03u ||
        read_u32_le(preview_bytes + 48u) != 0x00008067u) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview spill-backed value ops mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(preview_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_riscv32_preview_keeps_large_spill_binary_operands_disjoint(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    unsigned char *preview_bytes = NULL;
    size_t preview_byte_count = 0u;
    int ok = 1;
    uint32_t rhs_base_add = 0u;
    uint32_t rhs_load = 0u;
    uint32_t add_word = 0u;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }

    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].local_count = 1025u;
    emit_program.functions[0].spill_slot_count = 3u;
    emit_program.functions[0].block_count = 1u;
    emit_program.functions[0].block_capacity = 1u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0u;
    emit_program.functions[0].blocks[0].original_layout_index = 0u;
    emit_program.functions[0].blocks[0].original_block_id = 0u;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 3u;
    emit_program.functions[0].blocks[0].op_capacity = 3u;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(3u, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
    emit_program.functions[0].blocks[0].ops[0].has_result = 1u;
    emit_program.functions[0].blocks[0].ops[0].result = machine_select_operand_spill_slot(0u);
    emit_program.functions[0].blocks[0].ops[0].as.copy_value = machine_select_operand_immediate(1);

    emit_program.functions[0].blocks[0].ops[1].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
    emit_program.functions[0].blocks[0].ops[1].has_result = 1u;
    emit_program.functions[0].blocks[0].ops[1].result = machine_select_operand_spill_slot(1u);
    emit_program.functions[0].blocks[0].ops[1].as.copy_value = machine_select_operand_immediate(1);

    emit_program.functions[0].blocks[0].ops[2].kind = MACHINE_SELECT_OP_ALU;
    emit_program.functions[0].blocks[0].ops[2].has_result = 1u;
    emit_program.functions[0].blocks[0].ops[2].result = machine_select_operand_spill_slot(2u);
    emit_program.functions[0].blocks[0].ops[2].as.binary.op = MACHINE_IR_BINARY_ADD;
    emit_program.functions[0].blocks[0].ops[2].as.binary.lhs = machine_select_operand_spill_slot(0u);
    emit_program.functions[0].blocks[0].ops[2].as.binary.rhs = machine_select_operand_spill_slot(1u);

    emit_program.functions[0].blocks[0].has_terminator = 1u;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_SPILL;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_spill_slot(2u);

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_program_copy_bytes(&bytes_program, &preview_bytes, &preview_byte_count, &bytes_error) ||
        preview_byte_count != 112u) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview large-spill binary setup failed: %s\n", bytes_error.message);
        ok = 0;
    } else {
        rhs_base_add = read_u32_le(preview_bytes + 64u);
        rhs_load = read_u32_le(preview_bytes + 68u);
        add_word = read_u32_le(preview_bytes + 72u);

        if ((rhs_base_add & 0x7Fu) != 0x33u ||
            ((rhs_base_add >> 7u) & 0x1Fu) != 29u ||
            ((rhs_base_add >> 15u) & 0x1Fu) != 2u ||
            ((rhs_base_add >> 20u) & 0x1Fu) != 29u ||
            (rhs_load & 0x7Fu) != 0x03u ||
            ((rhs_load >> 7u) & 0x1Fu) != 30u ||
            ((rhs_load >> 15u) & 0x1Fu) != 29u ||
            add_word != 0x01ef8eb3u) {
            fprintf(stderr,
                "[machine-bytes] FAIL: riscv preview large-spill binary prep reused the lhs scratch unexpectedly\n");
            ok = 0;
        }
    }

    free(preview_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_riscv32_preview_materializes_spill_backed_cmp_results(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    unsigned char *preview_bytes = NULL;
    size_t preview_byte_count = 0u;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].local_count = 2;
    emit_program.functions[0].spill_slot_count = 4;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 4;
    emit_program.functions[0].blocks[0].op_capacity = 4;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(4, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_LOAD_LOCAL;
    emit_program.functions[0].blocks[0].ops[0].has_result = 1;
    emit_program.functions[0].blocks[0].ops[0].result = machine_select_operand_spill_slot(0);
    emit_program.functions[0].blocks[0].ops[0].as.load_slot.kind = MACHINE_SELECT_SLOT_LOCAL;
    emit_program.functions[0].blocks[0].ops[0].as.load_slot.id = 0;

    emit_program.functions[0].blocks[0].ops[1].kind = MACHINE_SELECT_OP_LOAD_LOCAL;
    emit_program.functions[0].blocks[0].ops[1].has_result = 1;
    emit_program.functions[0].blocks[0].ops[1].result = machine_select_operand_spill_slot(1);
    emit_program.functions[0].blocks[0].ops[1].as.load_slot.kind = MACHINE_SELECT_SLOT_LOCAL;
    emit_program.functions[0].blocks[0].ops[1].as.load_slot.id = 1;

    emit_program.functions[0].blocks[0].ops[2].kind = MACHINE_SELECT_OP_CMP;
    emit_program.functions[0].blocks[0].ops[2].has_result = 1;
    emit_program.functions[0].blocks[0].ops[2].result = machine_select_operand_spill_slot(2);
    emit_program.functions[0].blocks[0].ops[2].as.binary.op = MACHINE_IR_BINARY_EQ;
    emit_program.functions[0].blocks[0].ops[2].as.binary.lhs = machine_select_operand_spill_slot(0);
    emit_program.functions[0].blocks[0].ops[2].as.binary.rhs = machine_select_operand_spill_slot(1);

    emit_program.functions[0].blocks[0].ops[3].kind = MACHINE_SELECT_OP_CMP_IMM;
    emit_program.functions[0].blocks[0].ops[3].has_result = 1;
    emit_program.functions[0].blocks[0].ops[3].result = machine_select_operand_spill_slot(3);
    emit_program.functions[0].blocks[0].ops[3].as.binary.op = MACHINE_IR_BINARY_LT;
    emit_program.functions[0].blocks[0].ops[3].as.binary.lhs = machine_select_operand_spill_slot(2);
    emit_program.functions[0].blocks[0].ops[3].as.binary.rhs = machine_select_operand_immediate(5);

    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_SPILL;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_spill_slot(3);

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_program_copy_bytes(&bytes_program, &preview_bytes, &preview_byte_count, &bytes_error) ||
        preview_byte_count != 56u ||
        (read_u32_le(preview_bytes + 0u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 4u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 8u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 12u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 16u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 20u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 24u) & 0x7Fu) != 0x33u ||
        (read_u32_le(preview_bytes + 28u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 32u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 36u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 40u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 44u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 48u) & 0x7Fu) != 0x03u ||
        read_u32_le(preview_bytes + 52u) != 0x00008067u) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview spill-backed cmp result mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(preview_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_riscv32_preview_materializes_large_slot_offsets(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    unsigned char *preview_bytes = NULL;
    size_t preview_byte_count = 0u;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.register_bank.register_count = 1u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(1u, sizeof(MachineEmitRegisterDesc));
    emit_program.global_count = 1025u;
    emit_program.global_capacity = 1025u;
    emit_program.globals = (MachineEmitGlobal *)calloc(1025u, sizeof(MachineEmitGlobal));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.globals || !emit_program.functions) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }
    emit_program.register_bank.registers[0].register_id = 0u;
    emit_program.register_bank.registers[0].name = dup_text("r0");
    emit_program.register_bank.registers[0].allocatable = 1u;

    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].local_count = 2049u;
    emit_program.functions[0].spill_slot_count = 1u;
    emit_program.functions[0].block_count = 1u;
    emit_program.functions[0].block_capacity = 1u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!emit_program.register_bank.registers[0].name ||
        !emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0u;
    emit_program.functions[0].blocks[0].original_layout_index = 0u;
    emit_program.functions[0].blocks[0].original_block_id = 0u;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 4u;
    emit_program.functions[0].blocks[0].op_capacity = 4u;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(4u, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_LOAD_LOCAL;
    emit_program.functions[0].blocks[0].ops[0].has_result = 1;
    emit_program.functions[0].blocks[0].ops[0].result = machine_select_operand_spill_slot(0u);
    emit_program.functions[0].blocks[0].ops[0].as.load_slot.kind = MACHINE_SELECT_SLOT_LOCAL;
    emit_program.functions[0].blocks[0].ops[0].as.load_slot.id = 2048u;

    emit_program.functions[0].blocks[0].ops[1].kind = MACHINE_SELECT_OP_LOAD_GLOBAL;
    emit_program.functions[0].blocks[0].ops[1].has_result = 1;
    emit_program.functions[0].blocks[0].ops[1].result = machine_select_operand_register(0u);
    emit_program.functions[0].blocks[0].ops[1].as.load_slot.kind = MACHINE_SELECT_SLOT_GLOBAL;
    emit_program.functions[0].blocks[0].ops[1].as.load_slot.id = 1024u;

    emit_program.functions[0].blocks[0].ops[2].kind = MACHINE_SELECT_OP_STORE_LOCAL;
    emit_program.functions[0].blocks[0].ops[2].as.store.slot.kind = MACHINE_SELECT_SLOT_LOCAL;
    emit_program.functions[0].blocks[0].ops[2].as.store.slot.id = 2048u;
    emit_program.functions[0].blocks[0].ops[2].as.store.value = machine_select_operand_spill_slot(0u);

    emit_program.functions[0].blocks[0].ops[3].kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
    emit_program.functions[0].blocks[0].ops[3].as.store.slot.kind = MACHINE_SELECT_SLOT_GLOBAL;
    emit_program.functions[0].blocks[0].ops[3].as.store.slot.id = 1024u;
    emit_program.functions[0].blocks[0].ops[3].as.store.value = machine_select_operand_immediate(7);

    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_program_copy_bytes(&bytes_program, &preview_bytes, &preview_byte_count, &bytes_error) ||
        preview_byte_count != 92u ||
        (read_u32_le(preview_bytes + 0u) & 0x7Fu) != 0x37u ||
        (read_u32_le(preview_bytes + 4u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 8u) & 0x7Fu) != 0x33u ||
        (read_u32_le(preview_bytes + 12u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 16u) & 0x7Fu) != 0x37u ||
        (read_u32_le(preview_bytes + 20u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 24u) & 0x7Fu) != 0x33u ||
        (read_u32_le(preview_bytes + 28u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 32u) & 0x7Fu) != 0x37u ||
        (read_u32_le(preview_bytes + 36u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 40u) & 0x7Fu) != 0x37u ||
        (read_u32_le(preview_bytes + 44u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 48u) & 0x7Fu) != 0x33u ||
        (read_u32_le(preview_bytes + 52u) & 0x7Fu) != 0x03u ||
        (read_u32_le(preview_bytes + 56u) & 0x7Fu) != 0x37u ||
        (read_u32_le(preview_bytes + 60u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 64u) & 0x7Fu) != 0x33u ||
        (read_u32_le(preview_bytes + 68u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 72u) & 0x7Fu) != 0x13u ||
        (read_u32_le(preview_bytes + 76u) & 0x7Fu) != 0x37u ||
        (read_u32_le(preview_bytes + 80u) & 0x7Fu) != 0x23u ||
        (read_u32_le(preview_bytes + 84u) & 0x7Fu) != 0x13u ||
        read_u32_le(preview_bytes + 88u) != 0x00008067u) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview large-slot offset mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(preview_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_riscv32_preview_emits_rv32m_alu_words(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    unsigned char *preview_bytes = NULL;
    size_t preview_byte_count = 0u;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.register_bank.register_count = 8u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(8u, sizeof(MachineEmitRegisterDesc));
    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.functions) {
        return 0;
    }
    for (size_t reg_index = 0u; reg_index < 8u; ++reg_index) {
        char name_buf[8];

        snprintf(name_buf, sizeof(name_buf), "r%zu", reg_index);
        emit_program.register_bank.registers[reg_index].register_id = reg_index;
        emit_program.register_bank.registers[reg_index].name = dup_text(name_buf);
        emit_program.register_bank.registers[reg_index].allocatable = 1u;
        if (!emit_program.register_bank.registers[reg_index].name) {
            machine_emit_program_free(&emit_program);
            machine_bytes_program_free(&bytes_program);
            return 0;
        }
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 3;
    emit_program.functions[0].blocks[0].op_capacity = 3;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(3, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_ALU;
    emit_program.functions[0].blocks[0].ops[0].has_result = 1;
    emit_program.functions[0].blocks[0].ops[0].result = machine_select_operand_register(0);
    emit_program.functions[0].blocks[0].ops[0].as.binary.op = MACHINE_IR_BINARY_MUL;
    emit_program.functions[0].blocks[0].ops[0].as.binary.lhs = machine_select_operand_register(1);
    emit_program.functions[0].blocks[0].ops[0].as.binary.rhs = machine_select_operand_register(2);

    emit_program.functions[0].blocks[0].ops[1].kind = MACHINE_SELECT_OP_ALU;
    emit_program.functions[0].blocks[0].ops[1].has_result = 1;
    emit_program.functions[0].blocks[0].ops[1].result = machine_select_operand_register(3);
    emit_program.functions[0].blocks[0].ops[1].as.binary.op = MACHINE_IR_BINARY_DIV;
    emit_program.functions[0].blocks[0].ops[1].as.binary.lhs = machine_select_operand_register(4);
    emit_program.functions[0].blocks[0].ops[1].as.binary.rhs = machine_select_operand_register(5);

    emit_program.functions[0].blocks[0].ops[2].kind = MACHINE_SELECT_OP_ALU;
    emit_program.functions[0].blocks[0].ops[2].has_result = 1;
    emit_program.functions[0].blocks[0].ops[2].result = machine_select_operand_register(6);
    emit_program.functions[0].blocks[0].ops[2].as.binary.op = MACHINE_IR_BINARY_MOD;
    emit_program.functions[0].blocks[0].ops[2].as.binary.lhs = machine_select_operand_register(7);
    emit_program.functions[0].blocks[0].ops[2].as.binary.rhs = machine_select_operand_register(0);

    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_program_copy_bytes(&bytes_program, &preview_bytes, &preview_byte_count, &bytes_error) ||
        preview_byte_count != 16u ||
        read_u32_le(preview_bytes + 0u) != 0x02c58533u ||
        read_u32_le(preview_bytes + 4u) != 0x02f746b3u ||
        read_u32_le(preview_bytes + 8u) != 0x02a8e833u ||
        read_u32_le(preview_bytes + 12u) != 0x00008067u) {
        fprintf(stderr, "[machine-bytes] FAIL: riscv preview rv32m alu words mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(preview_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_riscv32_preview_rejects_more_than_eight_logical_registers(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.register_bank.register_count = 9u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(9u, sizeof(MachineEmitRegisterDesc));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.functions) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    for (size_t register_index = 0u; register_index < 9u; ++register_index) {
        emit_program.register_bank.registers[register_index].register_id = register_index;
        emit_program.register_bank.registers[register_index].name = dup_text("r");
        emit_program.register_bank.registers[register_index].allocatable = 1u;
        if (!emit_program.register_bank.registers[register_index].name) {
            machine_emit_program_free(&emit_program);
            machine_bytes_program_free(&bytes_program);
            return 0;
        }
    }

    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].local_count = 1u;
    emit_program.functions[0].block_count = 1u;
    emit_program.functions[0].block_capacity = 1u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0u;
    emit_program.functions[0].blocks[0].original_layout_index = 0u;
    emit_program.functions[0].blocks[0].original_block_id = 0u;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1u;
    emit_program.functions[0].blocks[0].op_capacity = 1u;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1u, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
    emit_program.functions[0].blocks[0].ops[0].has_result = 1u;
    emit_program.functions[0].blocks[0].ops[0].result = machine_select_operand_register(8u);
    emit_program.functions[0].blocks[0].ops[0].as.copy_value = machine_select_operand_immediate(7);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_register(8u);

    if (machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        strstr(bytes_error.message, "MACHINE-BYTES-124") == NULL) {
        fprintf(stderr,
            "[machine-bytes] FAIL: riscv preview oversized register bank was not rejected: %s\n",
            bytes_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    return ok;
}

static int test_machine_bytes_report_handles_zero_reference_function(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrError machine_error;
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    const MachineBytesFunctionSummary *function_summary = NULL;
    size_t reference_count = 1u;
    size_t fixup_count = 1u;
    size_t symbol_count = 0u;
    char *actual_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_bytes_report_init(&bytes_report);

    machine_report.program.register_bank.register_count = 1u;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0u;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_report.program.register_bank.registers[0].name ||
        !machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-bytes] FAIL: zero-reference setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    if (!machine_bytes_build_report_from_machine_ir_report(&machine_report, &bytes_report, &bytes_error) ||
        !machine_bytes_report_get_function_summary_artifact(&bytes_report, 0u, &function_summary) ||
        !function_summary ||
        function_summary->byte_count != 2u ||
        function_summary->return_count != 1u ||
        !machine_bytes_report_get_reference_summary_count(&bytes_report, &reference_count) ||
        reference_count != 0u ||
        !machine_bytes_report_get_fixup_summary_count(&bytes_report, &fixup_count) ||
        fixup_count != 0u ||
        !machine_bytes_report_get_symbol_summary_count(&bytes_report, &symbol_count) ||
        symbol_count != 2u ||
        !machine_bytes_build_report_from_machine_ir_report_dump(&machine_report, &actual_text, &bytes_error) ||
        !actual_text ||
        strstr(actual_text, "machine_bytes-report total_bytes=2 call_funcs=0 fallthrough_funcs=0 branch_funcs=0 total_block_summaries=1\n") !=
            actual_text ||
        strstr(actual_text, "sec.0 .text span=0..2 bytes=2 funcs=1 blocks=1 symbols=2 fixups=0\n") == NULL) {
        fprintf(stderr, "[machine-bytes] FAIL: zero-reference report mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(actual_text);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_bytes_report_free(&bytes_report);
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
        0x23u, 0x82u, 0x83u, 0x01u, 0x81u, 0x11u,
        0x23u, 0x85u, 0x81u, 0x13u};
    static const unsigned char expected_helper_bytes[] = {0x23u, 0x85u, 0x81u, 0x13u};
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
        "  F0.L0 @0..4 term@2 bytes=23 84 83 01 ; emit.0 -> layout.0 -> bb.0\n"
        "    fallthrough F0.L1 @4\n"
        "  F0.L1 @4..6 term@4 bytes=81 13 ; emit.1 -> layout.1 -> bb.1\n"
        "    reti 3\n");

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

static int test_machine_bytes_store_imm_zero_uses_zero_register(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    const MachineBytesFunction *function_view = NULL;
    const MachineBytesBlock *block_view = NULL;
    unsigned char *block_bytes = NULL;
    size_t block_byte_count = 0;
    size_t function_index = 0;
    size_t emit_index = 0;
    uint32_t first_word = 0u;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);

    emit_program.register_bank.register_count = 1u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(1u, sizeof(MachineEmitRegisterDesc));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.functions) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.register_bank.registers[0].register_id = 0u;
    emit_program.register_bank.registers[0].name = dup_text("r0");
    emit_program.register_bank.registers[0].allocatable = 1u;
    if (!emit_program.register_bank.registers[0].name) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].local_count = 1u;
    emit_program.functions[0].block_count = 1u;
    emit_program.functions[0].block_capacity = 1u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0u;
    emit_program.functions[0].blocks[0].original_layout_index = 0u;
    emit_program.functions[0].blocks[0].original_block_id = 0u;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1u;
    emit_program.functions[0].blocks[0].op_capacity = 1u;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1u, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_STORE_LOCAL_IMM;
    emit_program.functions[0].blocks[0].ops[0].as.store.slot = machine_select_slot_local(0u);
    emit_program.functions[0].blocks[0].ops[0].as.store.value = machine_select_operand_immediate(0);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_none();

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_program_copy_block_bytes(&bytes_program, 0u, 0u, &block_bytes, &block_byte_count, &bytes_error) ||
        !block_bytes ||
        block_byte_count != 8u ||
        !machine_bytes_program_find_block_by_program_byte_offset(
            &bytes_program, 0u, &function_index, &emit_index, &function_view, &block_view) ||
        function_index != 0u ||
        emit_index != 0u ||
        !function_view ||
        strcmp(function_view->name, "main") != 0 ||
        !block_view ||
        strcmp(block_view->label_name, "F0.L0") != 0) {
        fprintf(stderr, "[machine-bytes] FAIL: store-imm-zero optimization mismatch\n");
        ok = 0;
    } else {
        first_word = read_u32_le(block_bytes);
        if (first_word != 0x00012023u) {
            fprintf(stderr, "[machine-bytes] FAIL: store-imm-zero did not use zero register\n");
            ok = 0;
        }
    }

    free(block_bytes);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
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
        "    reti 0\n");

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

static int test_machine_bytes_report_surfaces_global_object_sections(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineBytesReport bytes_report;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    const MachineBytesSectionSummary *section_summary = NULL;
    const MachineBytesSymbolSummary *symbol_summary = NULL;
    const MachineBytesSymbolSummary *section_symbols = NULL;
    char *actual_text = NULL;
    size_t symbol_count = 0u;
    size_t section_count = 0u;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);
    machine_bytes_report_init(&bytes_report);

    emit_program.global_count = 2u;
    emit_program.global_capacity = 2u;
    emit_program.globals = (MachineEmitGlobal *)calloc(2u, sizeof(MachineEmitGlobal));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.globals || !emit_program.functions) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    emit_program.globals[0].id = 0u;
    emit_program.globals[0].name = dup_text("g");
    emit_program.globals[1].id = 1u;
    emit_program.globals[1].name = dup_text("h");
    emit_program.globals[1].has_initializer = 1;
    emit_program.globals[1].initializer_value = 7;

    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1u;
    emit_program.functions[0].block_capacity = 1u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!emit_program.globals[0].name || !emit_program.globals[1].name ||
        !emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0u;
    emit_program.functions[0].blocks[0].original_layout_index = 0u;
    emit_program.functions[0].blocks[0].original_block_id = 0u;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);
    if (!emit_program.functions[0].blocks[0].label_name) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_bytes_lower_program_from_machine_encode(&encode_program, &bytes_program, &bytes_error) ||
        !machine_bytes_build_report_from_program(&bytes_program, &bytes_report, &bytes_error) ||
        !machine_bytes_report_get_section_summary_count(&bytes_report, &section_count) ||
        section_count != 3u ||
        !machine_bytes_report_get_section_summary(&bytes_report, 1u, &section_summary) ||
        !section_summary || strcmp(section_summary->name, ".sbss") != 0 ||
        section_summary->byte_count != 4u || section_summary->symbol_count != 1u ||
        !machine_bytes_report_get_section_symbol_summaries(&bytes_report, 1u, &symbol_count, &section_symbols) ||
        symbol_count != 1u || !section_symbols ||
        section_symbols[0].kind != MACHINE_BYTES_SYMBOL_GLOBAL_OBJECT ||
        !section_symbols[0].name || strcmp(section_symbols[0].name, "g") != 0 ||
        !section_symbols[0].has_section_index || section_symbols[0].section_index != 1u ||
        !section_symbols[0].has_byte_offset || section_symbols[0].byte_offset != 0u ||
        !machine_bytes_report_get_section_summary(&bytes_report, 2u, &section_summary) ||
        !section_summary || strcmp(section_summary->name, ".sdata") != 0 ||
        section_summary->byte_count != 4u || section_summary->symbol_count != 1u ||
        !machine_bytes_report_find_symbol_summary_by_name(&bytes_report, "h", NULL, &symbol_summary) ||
        !symbol_summary || symbol_summary->kind != MACHINE_BYTES_SYMBOL_GLOBAL_OBJECT ||
        !symbol_summary->has_section_index || symbol_summary->section_index != 2u ||
        !symbol_summary->has_byte_offset || symbol_summary->byte_offset != 0u ||
        !machine_bytes_build_report_from_program_dump(&bytes_program, &actual_text, &bytes_error) ||
        !actual_text ||
        strstr(actual_text, "sec.1 .sbss") == NULL ||
        strstr(actual_text, "sec.2 .sdata") == NULL ||
        strstr(actual_text, "global g defined=1") == NULL ||
        strstr(actual_text, "global h defined=1") == NULL) {
        fprintf(stderr, "[machine-bytes] FAIL: global object section report mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    free(actual_text);
    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

static int test_machine_bytes_report_allows_empty_section_subqueries(void) {
    MachineBytesReport bytes_report;
    const MachineBytesSymbolSummary *section_symbols = NULL;
    const MachineBytesFixupSummary *section_fixups = NULL;
    size_t symbol_count = 1u;
    size_t fixup_count = 1u;

    machine_bytes_report_init(&bytes_report);
    bytes_report.section_summaries = (MachineBytesSectionSummary *)calloc(1u, sizeof(MachineBytesSectionSummary));
    if (!bytes_report.section_summaries) {
        return 0;
    }
    bytes_report.total_section_summary_count = 1u;
    bytes_report.section_summaries[0].name = ".empty";

    if (!machine_bytes_report_get_section_symbol_summaries(&bytes_report, 0u, &symbol_count, &section_symbols) ||
        symbol_count != 0u || section_symbols != NULL ||
        !machine_bytes_report_get_section_fixup_summaries(&bytes_report, 0u, &fixup_count, &section_fixups) ||
        fixup_count != 0u || section_fixups != NULL) {
        fprintf(stderr, "[machine-bytes] FAIL: empty section subquery contract mismatch\n");
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    machine_bytes_report_free(&bytes_report);
    return 1;
}

static int test_machine_bytes_report_surfaces_global_data_references(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    const MachineBytesReferenceSummary *reference_summary = NULL;
    const MachineBytesFixupSummary *fixup_summary = NULL;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_bytes_report_init(&bytes_report);

    emit_program.register_bank.register_count = 1u;
    emit_program.register_bank.registers = (MachineEmitRegisterDesc *)calloc(1u, sizeof(MachineEmitRegisterDesc));
    emit_program.global_count = 2u;
    emit_program.global_capacity = 2u;
    emit_program.globals = (MachineEmitGlobal *)calloc(2u, sizeof(MachineEmitGlobal));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.register_bank.registers || !emit_program.globals || !emit_program.functions) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }
    emit_program.register_bank.registers[0].register_id = 0u;
    emit_program.register_bank.registers[0].name = dup_text("r0");
    emit_program.register_bank.registers[0].allocatable = 1u;

    emit_program.globals[0].id = 0u;
    emit_program.globals[0].name = dup_text("g");
    emit_program.globals[1].id = 1u;
    emit_program.globals[1].name = dup_text("h");
    emit_program.globals[1].has_initializer = 1;
    emit_program.globals[1].initializer_value = 7;

    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1u;
    emit_program.functions[0].block_capacity = 1u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!emit_program.register_bank.registers[0].name ||
        !emit_program.globals[0].name || !emit_program.globals[1].name ||
        !emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0u;
    emit_program.functions[0].blocks[0].original_layout_index = 0u;
    emit_program.functions[0].blocks[0].original_block_id = 0u;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 2u;
    emit_program.functions[0].blocks[0].op_capacity = 2u;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(2u, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_LOAD_GLOBAL;
    emit_program.functions[0].blocks[0].ops[0].has_result = 1;
    emit_program.functions[0].blocks[0].ops[0].result = machine_select_operand_register(0u);
    emit_program.functions[0].blocks[0].ops[0].as.load_slot.kind = MACHINE_SELECT_SLOT_GLOBAL;
    emit_program.functions[0].blocks[0].ops[0].as.load_slot.id = 0u;

    emit_program.functions[0].blocks[0].ops[1].kind = MACHINE_SELECT_OP_STORE_GLOBAL_IMM;
    emit_program.functions[0].blocks[0].ops[1].as.store.slot.kind = MACHINE_SELECT_SLOT_GLOBAL;
    emit_program.functions[0].blocks[0].ops[1].as.store.slot.id = 1u;
    emit_program.functions[0].blocks[0].ops[1].as.store.value = machine_select_operand_immediate(9);

    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_bytes_build_report_from_program(&bytes_program, &bytes_report, &bytes_error) ||
        !machine_bytes_report_get_reference_summary_count(&bytes_report, &(size_t){0}) ||
        !machine_bytes_report_get_reference_summary(&bytes_report, 0u, &reference_summary) ||
        !reference_summary ||
        reference_summary->kind != MACHINE_BYTES_REFERENCE_DATA_ADDR ||
        !reference_summary->target_name || strcmp(reference_summary->target_name, "g") != 0 ||
        reference_summary->patch_byte_offset != 0u ||
        reference_summary->patch_byte_count != 4u ||
        !reference_summary->has_target_byte_offset || reference_summary->target_byte_offset != 0u ||
        !machine_bytes_report_get_reference_summary(&bytes_report, 1u, &reference_summary) ||
        !reference_summary ||
        reference_summary->kind != MACHINE_BYTES_REFERENCE_DATA_LOAD ||
        !reference_summary->target_name || strcmp(reference_summary->target_name, "g") != 0 ||
        reference_summary->patch_byte_offset != 4u ||
        reference_summary->patch_byte_count != 4u ||
        !reference_summary->has_target_byte_offset || reference_summary->target_byte_offset != 0u ||
        !machine_bytes_report_get_fixup_summary(&bytes_report, 0u, &fixup_summary) ||
        !fixup_summary ||
        fixup_summary->kind != MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET ||
        !machine_bytes_report_get_fixup_summary(&bytes_report, 1u, &fixup_summary) ||
        !fixup_summary ||
        fixup_summary->kind != MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET ||
        !machine_bytes_report_get_reference_summary(&bytes_report, 2u, &reference_summary) ||
        !reference_summary ||
        reference_summary->kind != MACHINE_BYTES_REFERENCE_DATA_ADDR ||
        !reference_summary->target_name || strcmp(reference_summary->target_name, "h") != 0 ||
        reference_summary->patch_byte_offset != 12u ||
        reference_summary->patch_byte_count != 4u ||
        !machine_bytes_report_get_reference_summary(&bytes_report, 3u, &reference_summary) ||
        !reference_summary ||
        reference_summary->kind != MACHINE_BYTES_REFERENCE_DATA_STORE ||
        !reference_summary->target_name || strcmp(reference_summary->target_name, "h") != 0 ||
        reference_summary->patch_byte_offset != 16u ||
        reference_summary->patch_byte_count != 4u ||
        !machine_bytes_report_get_fixup_summary(&bytes_report, 2u, &fixup_summary) ||
        !fixup_summary ||
        fixup_summary->kind != MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET ||
        !machine_bytes_report_get_fixup_summary(&bytes_report, 3u, &fixup_summary) ||
        !fixup_summary ||
        fixup_summary->kind != MACHINE_BYTES_FIXUP_DATA_STORE_TARGET) {
        fprintf(stderr, "[machine-bytes] FAIL: global data reference mismatch: %s\n", bytes_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_bytes_lowers_from_machine_encode();
    ok &= test_machine_bytes_riscv32_preview_emits_void_return_word();
    ok &= test_machine_bytes_report_and_bridges();
    ok &= test_machine_bytes_report_query_helpers_reject_malformed_summary_tables();
    ok &= test_machine_bytes_riscv32_preview_profile_changes_text_bytes();
    ok &= test_machine_bytes_riscv32_preview_patches_internal_call_targets();
    ok &= test_machine_bytes_riscv32_preview_uses_real_fallthrough_control();
    ok &= test_machine_bytes_riscv32_preview_expands_large_immediates();
    ok &= test_machine_bytes_riscv32_preview_expands_nonzero_compare_branch_immediates();
    ok &= test_machine_bytes_rejects_generic_branch_targets_that_do_not_fit_bytecode();
    ok &= test_machine_bytes_rejects_generic_immediates_that_do_not_fit_bytecode();
    ok &= test_machine_bytes_riscv32_preview_rejects_out_of_range_branch_targets();
    ok &= test_machine_bytes_riscv32_preview_materializes_call_args_and_spill_results();
    ok &= test_machine_bytes_riscv32_preview_materializes_spill_and_stack_call_args();
    ok &= test_machine_bytes_riscv32_preview_materializes_spill_backed_value_ops();
    ok &= test_machine_bytes_riscv32_preview_keeps_large_spill_binary_operands_disjoint();
    ok &= test_machine_bytes_riscv32_preview_materializes_spill_backed_cmp_results();
    ok &= test_machine_bytes_riscv32_preview_materializes_large_slot_offsets();
    ok &= test_machine_bytes_riscv32_preview_emits_rv32m_alu_words();
    ok &= test_machine_bytes_riscv32_preview_rejects_more_than_eight_logical_registers();
    ok &= test_machine_bytes_report_handles_zero_reference_function();
    ok &= test_machine_bytes_from_machine_emit_artifacts();
    ok &= test_machine_bytes_store_imm_zero_uses_zero_register();
    ok &= test_machine_bytes_encodes_call_payloads();
    ok &= test_machine_bytes_report_surfaces_global_object_sections();
    ok &= test_machine_bytes_report_allows_empty_section_subqueries();
    ok &= test_machine_bytes_report_surfaces_global_data_references();
    ok &= test_machine_bytes_program_level_offsets_and_byte_image();

    if (!ok) {
        return 1;
    }

    printf("[machine-bytes] PASS\n");
    return 0;
}
