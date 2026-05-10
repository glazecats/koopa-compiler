#include "machine/container.h"

#include "machine/bytes.h"
#include "machine/emit.h"
#include "machine/encode.h"
#include "machine/ir.h"
#include "machine/object.h"
#include "machine/reloc.h"

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

static int expect_dump(const MachineContainerFile *container_file, const char *expected_text) {
    char *actual_text = NULL;
    MachineContainerError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_container_dump_file(container_file, &actual_text, &error)) {
        fprintf(stderr, "[machine-container] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-container] FAIL: dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int test_machine_container_builds_from_machine_ir_report(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineContainerFile container_file;
    MachineContainerError container_error;
    MachineContainerLayoutSummary layout_summary;
    const MachineRelocFile *reloc_file = NULL;
    const MachineContainerSection *section = NULL;
    unsigned char *bytes = NULL;
    size_t object_byte_count = 0;
    size_t section_count = 0;
    size_t symbol_count = 0;
    size_t relocation_count = 0;
    size_t container_byte_count = 0;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&container_error, 0, sizeof(container_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_container_file_init(&container_file);

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
        fprintf(stderr, "[machine-container] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_container_file_free(&container_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-container] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_container_file_free(&container_file);
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
        fprintf(stderr, "[machine-container] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_container_file_free(&container_file);
        return 0;
    }

    if (!machine_container_build_from_machine_ir_report(&machine_report, &container_file, &container_error) ||
        !machine_container_file_get_summary(
            &container_file,
            &object_byte_count,
            &section_count,
            &symbol_count,
            &relocation_count,
            &container_byte_count) ||
        object_byte_count != 9 || section_count != 1 || symbol_count != 4 || relocation_count != 2 ||
        container_byte_count != 382 ||
        !machine_container_file_get_reloc_file(&container_file, &reloc_file) || !reloc_file ||
        reloc_file->relocation_count != 2 ||
        !machine_container_file_get_layout_summary(&container_file, &layout_summary) ||
        layout_summary.header_size != 44 || layout_summary.payload_offset != 373 || layout_summary.payload_size != 9 ||
        !machine_container_file_find_section_by_name(&container_file, ".text", NULL, &section) || !section ||
        section->file_offset != 373 || section->byte_count != 9 || section->relocation_count != 2 ||
        !machine_container_file_copy_bytes(&container_file, &bytes, &container_byte_count, &container_error) ||
        !bytes || container_byte_count != 382 ||
        bytes[0] != (unsigned char)'M' || bytes[1] != (unsigned char)'C' ||
        bytes[2] != (unsigned char)'N' || bytes[3] != (unsigned char)'1') {
        fprintf(stderr, "[machine-container] FAIL: container query mismatch: %s\n", container_error.message);
        ok = 0;
    }

    ok &= expect_dump(
        &container_file,
        "machine_container profile=generic container_bytes=382 object_bytes=9 sections=1 symbols=4 relocations=2\n"
        "policy: addends=zero-or-generic fallthrough=not-guaranteed\n"
        "layout header=0..44 section_table=44 symbol_table=72 relocation_table=216 string_table=320..373 payload=373..382\n"
        "sections:\n"
        "  csec.0 .text file@373 bytes=9 logical=0 relocations=2\n");

    free(bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_container_file_free(&container_file);
    return ok;
}

static int test_machine_container_builds_from_machine_reloc_with_external_call(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineContainerFile container_file;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    MachineContainerError container_error;
    MachineContainerLayoutSummary layout_summary;
    size_t object_byte_count = 0;
    size_t section_count = 0;
    size_t symbol_count = 0;
    size_t relocation_count = 0;
    size_t container_byte_count = 0;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    memset(&container_error, 0, sizeof(container_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);
    machine_container_file_init(&container_file);

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
        machine_container_file_free(&container_file);
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
        machine_container_file_free(&container_file);
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
        machine_container_file_free(&container_file);
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
        !machine_container_build_from_machine_reloc_file(&reloc_file, &container_file, &container_error) ||
        !machine_container_file_get_summary(
            &container_file,
            &object_byte_count,
            &section_count,
            &symbol_count,
            &relocation_count,
            &container_byte_count) ||
        object_byte_count != 5 || section_count != 1 || symbol_count != 3 || relocation_count != 1 ||
        !machine_container_file_get_layout_summary(&container_file, &layout_summary) ||
        layout_summary.payload_size != 5 || layout_summary.total_byte_count != container_byte_count ||
        container_byte_count <= object_byte_count) {
        fprintf(stderr, "[machine-container] FAIL: external container mismatch: %s\n", container_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    machine_container_file_free(&container_file);
    return ok;
}

static int test_machine_container_dump_uses_explicit_i386_preview_profile_name(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineContainerFile container_file;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    MachineContainerError container_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    memset(&container_error, 0, sizeof(container_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);
    machine_container_file_init(&container_file);

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
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
        machine_container_file_free(&container_file);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0u;
    emit_program.functions[0].blocks[0].original_layout_index = 0u;
    emit_program.functions[0].blocks[0].original_block_id = 0u;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1u;
    emit_program.functions[0].blocks[0].op_capacity = 1u;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1u, sizeof(MachineEmitOp));
    emit_program.functions[1].blocks[0].emit_index = 0u;
    emit_program.functions[1].blocks[0].original_layout_index = 0u;
    emit_program.functions[1].blocks[0].original_block_id = 0u;
    emit_program.functions[1].blocks[0].label_name = dup_text("F1.L0");
    if (!emit_program.functions[0].blocks[0].label_name ||
        !emit_program.functions[0].blocks[0].ops ||
        !emit_program.functions[1].blocks[0].label_name) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
        machine_container_file_free(&container_file);
        return 0;
    }

    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_VOID;
    emit_program.functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("sink");
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;
    emit_program.functions[1].blocks[0].has_terminator = 1;
    emit_program.functions[1].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN;

    if (!emit_program.functions[0].blocks[0].ops[0].as.call.callee_name ||
        !machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_I386_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_object_build_from_machine_bytes_program(&bytes_program, &object_file, &object_error) ||
        !machine_reloc_build_from_machine_object_file(&object_file, &reloc_file, &reloc_error) ||
        !machine_container_build_from_machine_reloc_file(&reloc_file, &container_file, &container_error) ||
        !machine_container_dump_file(&container_file, &dump_text, &container_error) ||
        !dump_text ||
        strstr(dump_text, "machine_container profile=i386-preview ") == NULL ||
        strstr(dump_text, "policy: addends=zero-or-generic fallthrough=not-guaranteed") == NULL) {
        fprintf(stderr, "[machine-container] FAIL: i386 preview profile-name dump mismatch: %s\n", container_error.message);
        ok = 0;
    }

    free(dump_text);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    machine_container_file_free(&container_file);
    return ok;
}

static int test_machine_container_empty_file_copy_bytes_contract(void) {
    MachineContainerFile container_file;
    MachineContainerError container_error;
    unsigned char *bytes = NULL;
    size_t byte_count = 1u;

    memset(&container_error, 0, sizeof(container_error));
    machine_container_file_init(&container_file);

    if (!machine_container_file_copy_bytes(&container_file, &bytes, &byte_count, &container_error) ||
        bytes != NULL || byte_count != 0u) {
        fprintf(stderr, "[machine-container] FAIL: empty container byte-copy contract mismatch: %s\n",
            container_error.message);
        machine_container_file_free(&container_file);
        return 0;
    }

    machine_container_file_free(&container_file);
    return 1;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_container_builds_from_machine_ir_report();
    ok &= test_machine_container_builds_from_machine_reloc_with_external_call();
    ok &= test_machine_container_dump_uses_explicit_i386_preview_profile_name();
    ok &= test_machine_container_empty_file_copy_bytes_contract();

    if (!ok) {
        return 1;
    }

    printf("[machine-container] PASS\n");
    return 0;
}
