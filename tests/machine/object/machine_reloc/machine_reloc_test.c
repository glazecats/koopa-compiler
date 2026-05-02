#include "machine/reloc.h"

#include "machine/emit.h"
#include "machine/encode.h"
#include "machine/bytes.h"
#include "machine/ir.h"
#include "machine/object.h"

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

static int expect_dump(const MachineRelocFile *reloc_file, const char *expected_text) {
    char *actual_text = NULL;
    MachineRelocError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_reloc_dump_file(reloc_file, &actual_text, &error)) {
        fprintf(stderr, "[machine-reloc] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-reloc] FAIL: dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int test_machine_reloc_builds_from_machine_ir_report(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineRelocFile reloc_file;
    MachineRelocError reloc_error;
    const MachineObjectFile *object_file = NULL;
    const MachineRelocSection *section = NULL;
    const MachineRelocation *relocation = NULL;
    const MachineRelocation *section_relocations = NULL;
    size_t total_byte_count = 0;
    size_t object_section_count = 0;
    size_t symbol_count = 0;
    size_t relocation_section_count = 0;
    size_t relocation_count = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_reloc_file_init(&reloc_file);

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
        fprintf(stderr, "[machine-reloc] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_reloc_file_free(&reloc_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-reloc] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_reloc_file_free(&reloc_file);
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
        fprintf(stderr, "[machine-reloc] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_reloc_file_free(&reloc_file);
        return 0;
    }

    if (!machine_reloc_build_from_machine_ir_report(&machine_report, &reloc_file, &reloc_error) ||
        !machine_reloc_file_get_summary(
            &reloc_file,
            &total_byte_count,
            &object_section_count,
            &symbol_count,
            &relocation_section_count,
            &relocation_count) ||
        total_byte_count != 9 || object_section_count != 1 || symbol_count != 4 ||
        relocation_section_count != 1 || relocation_count != 2 ||
        !machine_reloc_file_get_object_file(&reloc_file, &object_file) || !object_file ||
        object_file->fixup_count != 2 ||
        !machine_reloc_file_find_section_by_name(&reloc_file, ".text", NULL, &section) || !section ||
        section->relocation_count != 2 ||
        !machine_reloc_file_get_section_relocations(&reloc_file, 0, &relocation_count, &section_relocations) ||
        relocation_count != 2 || !section_relocations ||
        !machine_reloc_file_get_relocation(&reloc_file, 0, &relocation) || !relocation ||
        relocation->kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        !relocation->has_target_symbol_index || relocation->target_symbol_index != 3 ||
        !relocation->has_target_section_index || relocation->target_section_index != 0 ||
        !relocation->has_target_byte_offset || relocation->target_byte_offset != 7) {
        fprintf(stderr, "[machine-reloc] FAIL: reloc query mismatch: %s\n", reloc_error.message);
        ok = 0;
    }

    ok &= expect_dump(
        &reloc_file,
        "machine_reloc total_bytes=9 object_sections=1 symbols=4 relocation_sections=1 relocations=2\n"
        "reloc-sections:\n"
        "  relsec.0 .text span=0..9 bytes=9 relocations=2\n"
        "relocations:\n"
        "  reloc.0 ctrl-primary sec=0 patch@4 bytes=1 target=F0.L2 sym=yes tgt_sec=yes tgt_off=yes addend=0\n"
        "  reloc.1 ctrl-secondary sec=0 patch@4 bytes=1 target=F0.L1 sym=yes tgt_sec=yes tgt_off=yes addend=0\n");

    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

static int test_machine_reloc_builds_from_machine_object_with_external_call(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    const MachineRelocation *relocation = NULL;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);

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
        machine_reloc_file_free(&reloc_file);
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
        machine_reloc_file_free(&reloc_file);
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
        machine_reloc_file_free(&reloc_file);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].as.call.args[0] = machine_select_operand_immediate(5);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_bytes_lower_program_from_machine_encode(&encode_program, &bytes_program, &bytes_error) ||
        !machine_object_build_from_machine_bytes_program(&bytes_program, &object_file, &object_error) ||
        !machine_reloc_build_from_machine_object_file(&object_file, &reloc_file, &reloc_error) ||
        !machine_reloc_file_get_relocation(&reloc_file, 0, &relocation) || !relocation ||
        relocation->kind != MACHINE_BYTES_FIXUP_CALL_TARGET ||
        !relocation->has_target_symbol_index ||
        relocation->has_target_section_index ||
        relocation->has_target_byte_offset ||
        strcmp(relocation->target_name, "sink") != 0) {
        fprintf(stderr, "[machine-reloc] FAIL: external reloc mismatch: %s\n", reloc_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_reloc_builds_from_machine_ir_report();
    ok &= test_machine_reloc_builds_from_machine_object_with_external_call();

    if (!ok) {
        return 1;
    }

    printf("[machine-reloc] PASS\n");
    return 0;
}
