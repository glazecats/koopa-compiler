#include "machine/object.h"

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

static int expect_dump(const MachineObjectFile *object_file, const char *expected_text) {
    char *actual_text = NULL;
    MachineObjectError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_object_dump_file(object_file, &actual_text, &error)) {
        fprintf(stderr, "[machine-object] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-object] FAIL: dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
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
            "[machine-object] FAIL: %s byte count mismatch: expected %zu got %zu\n",
            context,
            expected_count,
            actual_count);
        return 0;
    }
    for (byte_index = 0; byte_index < expected_count; ++byte_index) {
        if (actual_bytes[byte_index] != expected_bytes[byte_index]) {
            fprintf(stderr,
                "[machine-object] FAIL: %s byte[%zu] mismatch: expected %02X got %02X\n",
                context,
                byte_index,
                expected_bytes[byte_index],
                actual_bytes[byte_index]);
            return 0;
        }
    }
    return 1;
}

static int test_machine_object_builds_from_machine_ir_report(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineObjectFile object_file;
    MachineObjectError object_error;
    const MachineObjectSection *section = NULL;
    const MachineObjectSymbol *symbol = NULL;
    const MachineObjectFixup *fixup = NULL;
    const MachineObjectSymbol *section_symbols = NULL;
    const MachineObjectFixup *section_fixups = NULL;
    unsigned char *section_bytes = NULL;
    size_t total_byte_count = 0;
    size_t section_count = 0;
    size_t symbol_count = 0;
    size_t fixup_count = 0;
    size_t section_byte_count = 0;
    static const unsigned char expected_bytes[] = {0x1Cu, 0x8Au, 0xEAu, 0x00u, 0x21u, 0x81u, 0x11u, 0x81u, 0x12u};
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&object_error, 0, sizeof(object_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_object_file_init(&object_file);

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
        fprintf(stderr, "[machine-object] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_object_file_free(&object_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-object] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_object_file_free(&object_file);
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
        fprintf(stderr, "[machine-object] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_object_file_free(&object_file);
        return 0;
    }

    if (!machine_object_build_from_machine_ir_report(&machine_report, &object_file, &object_error) ||
        !machine_object_file_get_summary(&object_file, &total_byte_count, &section_count, &symbol_count, &fixup_count) ||
        total_byte_count != 9 || section_count != 1 || symbol_count != 4 || fixup_count != 2 ||
        !machine_object_file_find_section_by_name(&object_file, ".text", NULL, &section) || !section ||
        section->byte_count != 9 || section->symbol_count != 4 || section->fixup_count != 2 ||
        !machine_object_file_copy_section_bytes(&object_file, 0, &section_bytes, &section_byte_count, &object_error) ||
        !section_bytes || !expect_byte_image(section_bytes, section_byte_count, expected_bytes, 9, "section-bytes") ||
        !machine_object_file_get_section_symbols(&object_file, 0, &symbol_count, &section_symbols) ||
        symbol_count != 4 || !section_symbols ||
        !machine_object_file_get_section_fixups(&object_file, 0, &fixup_count, &section_fixups) ||
        fixup_count != 2 || !section_fixups ||
        !machine_object_file_find_symbol_by_name(&object_file, "F0.L2", NULL, &symbol) || !symbol ||
        !symbol->is_defined || !symbol->has_emit_index || symbol->emit_index != 2 ||
        !machine_object_file_get_fixup(&object_file, 0, &fixup) || !fixup ||
        fixup->kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        fixup->has_target_symbol_index == 0 || strcmp(fixup->target_name, "F0.L2") != 0) {
        fprintf(stderr, "[machine-object] FAIL: object query mismatch: %s\n", object_error.message);
        ok = 0;
    }

    ok &= expect_dump(
        &object_file,
        "machine_object total_bytes=9 sections=1 symbols=4 fixups=2\n"
        "sections:\n"
        "  sec.0 .text span=0..9 bytes=9 symbols=4 fixups=2\n"
        "symbols:\n"
        "  sym.0 function main defined=1 sec=yes offset=0 bytes=9 incoming_fixups=0\n"
        "  sym.1 block F0.L0 defined=1 sec=yes offset=0 bytes=5 incoming_fixups=0\n"
        "  sym.2 block F0.L1 defined=1 sec=yes offset=5 bytes=2 incoming_fixups=1\n"
        "  sym.3 block F0.L2 defined=1 sec=yes offset=7 bytes=2 incoming_fixups=1\n"
        "fixups:\n"
        "  fixup.0 ctrl-primary sec=0 patch@4 bytes=1 target=F0.L2 sym=yes\n"
        "  fixup.1 ctrl-secondary sec=0 patch@4 bytes=1 target=F0.L1 sym=yes\n");

    free(section_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_object_file_free(&object_file);
    return ok;
}

static int test_machine_object_builds_from_machine_bytes_program_with_external_symbol(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    MachineObjectFile object_file;
    MachineObjectError object_error;
    const MachineObjectSymbol *symbol = NULL;
    const MachineObjectFixup *fixup = NULL;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);

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
        machine_object_file_free(&object_file);
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
        machine_object_file_free(&object_file);
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
        machine_object_file_free(&object_file);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].as.call.args[0] = machine_select_operand_immediate(5);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_bytes_lower_program_from_machine_encode(&encode_program, &bytes_program, &bytes_error) ||
        !machine_object_build_from_machine_bytes_program(&bytes_program, &object_file, &object_error) ||
        !machine_object_file_find_symbol_by_name(&object_file, "sink", NULL, &symbol) ||
        !symbol || symbol->kind != MACHINE_BYTES_SYMBOL_EXTERNAL || symbol->is_defined ||
        symbol->incoming_fixup_count != 1 ||
        !machine_object_file_get_fixup(&object_file, 0, &fixup) || !fixup ||
        fixup->kind != MACHINE_BYTES_FIXUP_CALL_TARGET || !fixup->has_target_symbol_index ||
        strcmp(fixup->target_name, "sink") != 0) {
        fprintf(stderr, "[machine-object] FAIL: external symbol object mismatch: %s\n", object_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_object_builds_from_machine_ir_report();
    ok &= test_machine_object_builds_from_machine_bytes_program_with_external_symbol();

    if (!ok) {
        return 1;
    }

    printf("[machine-object] PASS\n");
    return 0;
}
