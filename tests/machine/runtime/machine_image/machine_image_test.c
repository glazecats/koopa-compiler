#include "machine/image.h"

#include "machine/elf.h"
#include "machine/container.h"
#include "machine/reloc.h"
#include "machine/object.h"
#include "machine/bytes.h"
#include "machine/encode.h"
#include "machine/emit.h"
#include "machine/ir.h"

#include <elf.h>
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
    copy = (char *)malloc(length + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, length + 1u);
    return copy;
}

static int build_wrapped_machine_image_file(MachineImageFile *image_file) {
    if (!image_file) {
        return 0;
    }

    machine_image_file_init(image_file);
    image_file->target_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    image_file->source_elf_artifact_summary.target_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    image_file->source_elf_artifact_summary.origin_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    image_file->source_elf_artifact_summary.relocation_semantics =
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS;
    image_file->base_virtual_address = ((size_t)-1) - 7u;
    image_file->has_entry = 1u;
    image_file->entry_symbol_index = 0u;
    image_file->entry_virtual_address = image_file->base_virtual_address;
    image_file->segment_count = 1u;
    image_file->segment_capacity = 1u;
    image_file->segments = (MachineImageSegment *)calloc(1u, sizeof(MachineImageSegment));
    image_file->symbol_count = 1u;
    image_file->symbol_capacity = 1u;
    image_file->symbols = (MachineImageSymbol *)calloc(1u, sizeof(MachineImageSymbol));
    image_file->relocation_count = 1u;
    image_file->relocation_capacity = 1u;
    image_file->relocations = (MachineImageRelocation *)calloc(1u, sizeof(MachineImageRelocation));
    image_file->byte_count = 8u;
    image_file->bytes = (unsigned char *)calloc(8u, sizeof(unsigned char));
    if (!image_file->segments || !image_file->symbols || !image_file->relocations || !image_file->bytes) {
        machine_image_file_free(image_file);
        return 0;
    }

    image_file->segments[0].name = dup_text(".text");
    image_file->segments[0].image_offset = 0u;
    image_file->segments[0].virtual_address = image_file->base_virtual_address;
    image_file->segments[0].byte_count = 8u;
    image_file->segments[0].align = 4096u;

    image_file->symbols[0].name = dup_text("main");
    image_file->symbols[0].binding = MACHINE_ELF_SYMBOL_GLOBAL;
    image_file->symbols[0].type = MACHINE_ELF_SYMBOL_FUNC;
    image_file->symbols[0].is_defined = 1u;
    image_file->symbols[0].segment_index = 0u;
    image_file->symbols[0].value_offset = 0u;
    image_file->symbols[0].virtual_address = image_file->base_virtual_address;

    image_file->relocations[0].source_kind = MACHINE_BYTES_FIXUP_CONTROL_PRIMARY;
    image_file->relocations[0].segment_index = 0u;
    image_file->relocations[0].segment_offset = 1u;
    image_file->relocations[0].site_virtual_address = image_file->base_virtual_address + 1u;
    image_file->relocations[0].is_resolved = 1u;
    image_file->relocations[0].target_virtual_address = image_file->base_virtual_address;
    image_file->relocations[0].type = 2u;
    image_file->relocations[0].symbol_index = 0u;
    image_file->relocations[0].symbol_name = dup_text("main");
    if (!image_file->segments[0].name || !image_file->symbols[0].name || !image_file->relocations[0].symbol_name) {
        machine_image_file_free(image_file);
        return 0;
    }
    return 1;
}

static int expect_dump(const MachineImageFile *image_file, const char *expected_text) {
    MachineImageError error;
    char *actual_text = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_image_dump_file(image_file, &actual_text, &error)) {
        fprintf(stderr, "[machine-image] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-image] FAIL: dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int test_machine_image_builds_from_machine_ir_report(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineImageFile image_file;
    MachineImageError image_error;
    MachineImageHeaderSummary header_summary;
    const MachineImageSegment *segment = NULL;
    const MachineImageSymbol *symbol = NULL;
    const MachineImageRelocation *relocation = NULL;
    size_t resolved_count = 0u;
    size_t unresolved_count = 0u;
    size_t relocation_index = 0u;
    unsigned char *bytes = NULL;
    size_t segment_count = 0u;
    size_t symbol_count = 0u;
    size_t relocation_count = 0u;
    size_t byte_count = 0u;
    MachineElfTargetProfile target_profile;
    MachineElfArtifactSummary source_artifact_summary;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&image_error, 0, sizeof(image_error));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&source_artifact_summary, 0, sizeof(source_artifact_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_image_file_init(&image_file);

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
        fprintf(stderr, "[machine-image] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_image_file_free(&image_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-image] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_image_file_free(&image_file);
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
        fprintf(stderr, "[machine-image] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_image_file_free(&image_file);
        return 0;
    }

    if (!machine_image_build_from_machine_ir_report(&machine_report, &image_file, &image_error) ||
        !machine_image_file_get_summary(&image_file, &segment_count, &symbol_count, &relocation_count, &byte_count) ||
        segment_count != 1u || symbol_count != 5u || relocation_count != 2u || byte_count != 9u ||
        !machine_image_file_get_target_profile(&image_file, &target_profile) ||
        target_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        !machine_image_file_get_source_elf_artifact_summary(&image_file, &source_artifact_summary) ||
        source_artifact_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        source_artifact_summary.origin_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        source_artifact_summary.relocation_semantics != MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS ||
        !machine_image_file_get_header_summary(&image_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        header_summary.base_virtual_address != 0x1000u ||
        !header_summary.has_entry ||
        header_summary.entry_symbol_index != 4u ||
        header_summary.entry_virtual_address != 0x1000u ||
        !machine_image_file_get_segment(&image_file, 0u, &segment) || !segment ||
        segment->image_offset != 0u || segment->virtual_address != 0x1000u || segment->byte_count != 9u ||
        !machine_image_file_find_segment_covering_virtual_address(&image_file, 0x1004u, NULL, &segment) || !segment ||
        strcmp(segment->name, ".text") != 0 ||
        !machine_image_file_find_symbol_by_name(&image_file, "main", NULL, &symbol) || !symbol ||
        symbol->segment_index != 0u || symbol->value_offset != 0u || symbol->virtual_address != 0x1000u ||
        !machine_image_file_find_symbol_by_virtual_address(&image_file, 0x1007u, NULL, &symbol) || !symbol ||
        strcmp(symbol->name, "F0.L2") != 0 ||
        !machine_image_file_find_symbol_by_name(&image_file, "F0.L2", NULL, &symbol) || !symbol ||
        symbol->virtual_address != 0x1007u ||
        !machine_image_file_get_entry_symbol(&image_file, NULL, &symbol) || !symbol ||
        strcmp(symbol->name, "main") != 0 ||
        !machine_image_file_find_relocation_by_site_virtual_address(&image_file, 0x1004u, &relocation_index, &relocation) ||
        !relocation || relocation_index != 0u ||
        !machine_image_file_get_relocation(&image_file, 0u, &relocation) || !relocation ||
        relocation->source_kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        relocation->segment_index != 0u || relocation->segment_offset != 4u ||
        relocation->site_virtual_address != 0x1004u ||
        !relocation->is_resolved || relocation->target_virtual_address != 0x1007u ||
        relocation->symbol_index != 3u || relocation->type != 2u ||
        !machine_image_file_get_resolved_relocation_count(&image_file, &resolved_count) ||
        !machine_image_file_get_unresolved_relocation_count(&image_file, &unresolved_count) ||
        resolved_count != 2u || unresolved_count != 0u ||
        !machine_image_file_get_resolved_relocation_by_index(&image_file, 1u, &relocation_index, &relocation) ||
        !relocation || relocation_index != 1u || relocation->target_virtual_address != 0x1005u ||
        !machine_image_file_copy_bytes(&image_file, &bytes, &byte_count, &image_error) ||
        !bytes || byte_count != 9u) {
        fprintf(stderr, "[machine-image] FAIL: image query mismatch: %s\n", image_error.message);
        ok = 0;
    }

    ok &= expect_dump(
        &image_file,
        "machine_image profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans base=0x1000 bytes=9 segments=1 symbols=5 relocations=2 entry=main\n"
        "entry_address: 0x1000\n"
        "segments:\n"
        "  iseg.0 .text img@0 vaddr=0x1000 bytes=9 align=4096\n"
        "symbols:\n"
        "  isym.0  bind=local type=0 defined=0 seg=- off=- vaddr=-\n"
        "  isym.1 F0.L0 bind=local type=0 defined=1 seg=0 off=0x0 vaddr=0x1000\n"
        "  isym.2 F0.L1 bind=local type=0 defined=1 seg=0 off=0x5 vaddr=0x1005\n"
        "  isym.3 F0.L2 bind=local type=0 defined=1 seg=0 off=0x7 vaddr=0x1007\n"
        "  isym.4 main bind=global type=2 defined=1 seg=0 off=0x0 vaddr=0x1000\n"
        "relocations:\n"
        "  irel.0 kind=ctrl-primary seg=0 off=0x4 site=0x1004 resolved=yes target=0x1007 sym=3 type=2 name=F0.L2\n"
        "  irel.1 kind=ctrl-secondary seg=0 off=0x4 site=0x1004 resolved=yes target=0x1005 sym=2 type=3 name=F0.L1\n");

    free(bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_image_file_free(&image_file);
    return ok;
}

static int test_machine_image_preserves_global_object_segments(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrGlobal *global = NULL;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfError elf_error;
    MachineImageFile image_file;
    MachineImageError image_error;
    const MachineImageSegment *text_segment = NULL;
    const MachineImageSegment *sbss_segment = NULL;
    const MachineImageSegment *sdata_segment = NULL;
    const MachineImageSymbol *symbol = NULL;
    unsigned char *segment_bytes = NULL;
    size_t segment_byte_count = 0u;
    size_t segment_count = 0u;
    size_t symbol_count = 0u;
    size_t relocation_count = 0u;
    size_t byte_count = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&image_error, 0, sizeof(image_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_image_file_init(&image_file);

    machine_report.program.register_bank.register_count = 1u;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0u;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;
    if (!machine_report.program.register_bank.registers[0].name) {
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_image_file_free(&image_file);
        return 0;
    }

    if (!machine_ir_program_append_global(&machine_report.program, "g", &global, &machine_error) ||
        !global ||
        !machine_ir_program_append_global(&machine_report.program, "h", &global, &machine_error) ||
        !global) {
        fprintf(stderr, "[machine-image] FAIL: global-segment setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_image_file_free(&image_file);
        return 0;
    }
    machine_report.program.globals[1].has_initializer = 1;
    machine_report.program.globals[1].initializer_value = 7;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[0], machine_ir_operand_immediate(0), &machine_error)) {
        fprintf(stderr, "[machine-image] FAIL: global-segment setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_image_file_free(&image_file);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_image_build_from_machine_elf_file(&elf_file, &image_file, &image_error) ||
        !machine_image_file_get_summary(&image_file, &segment_count, &symbol_count, &relocation_count, &byte_count) ||
        segment_count != 3u || symbol_count != 5u || relocation_count != 0u || byte_count < 12u ||
        !machine_image_file_find_segment_by_name(&image_file, ".text", NULL, &text_segment) || !text_segment ||
        !machine_image_file_find_segment_by_name(&image_file, ".sbss", NULL, &sbss_segment) || !sbss_segment ||
        !machine_image_file_find_segment_by_name(&image_file, ".sdata", NULL, &sdata_segment) || !sdata_segment ||
        text_segment->virtual_address != 0x1000u ||
        sbss_segment->virtual_address <= text_segment->virtual_address ||
        sdata_segment->virtual_address <= sbss_segment->virtual_address ||
        (sbss_segment->image_offset % 4096u) != 0u ||
        (sdata_segment->image_offset % 4096u) != 0u ||
        sbss_segment->byte_count != 4u || sdata_segment->byte_count != 4u ||
        !machine_image_file_find_symbol_by_name(&image_file, "g", NULL, &symbol) || !symbol ||
        !symbol->is_defined || symbol->segment_index != 1u ||
        symbol->virtual_address != sbss_segment->virtual_address ||
        !machine_image_file_find_symbol_by_name(&image_file, "h", NULL, &symbol) || !symbol ||
        !symbol->is_defined || symbol->segment_index != 2u ||
        symbol->virtual_address != sdata_segment->virtual_address ||
        !machine_image_segment_copy_bytes(&image_file, 1u, &segment_bytes, &segment_byte_count, &image_error) ||
        !segment_bytes || segment_byte_count != 4u ||
        segment_bytes[0] != 0u || segment_bytes[1] != 0u ||
        segment_bytes[2] != 0u || segment_bytes[3] != 0u) {
        fprintf(stderr, "[machine-image] FAIL: global object image mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }
    free(segment_bytes);
    segment_bytes = NULL;
    segment_byte_count = 0u;
    if (!machine_image_segment_copy_bytes(&image_file, 2u, &segment_bytes, &segment_byte_count, &image_error) ||
        !segment_bytes || segment_byte_count != 4u ||
        segment_bytes[0] != 7u || segment_bytes[1] != 0u ||
        segment_bytes[2] != 0u || segment_bytes[3] != 0u) {
        fprintf(stderr, "[machine-image] FAIL: global object data bytes mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

cleanup:
    free(segment_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_image_file_free(&image_file);
    return ok;
}

static int test_machine_image_preserves_unresolved_external_call_site(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineContainerFile container_file;
    MachineElfFile elf_file;
    MachineImageFile image_file;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    MachineContainerError container_error;
    MachineElfError elf_error;
    MachineImageError image_error;
    const MachineImageRelocation *relocation = NULL;
    const MachineImageSymbol *symbol = NULL;
    size_t relocation_index = 0u;
    size_t unresolved_count = 0u;
    MachineImageHeaderSummary header_summary;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    memset(&container_error, 0, sizeof(container_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&image_error, 0, sizeof(image_error));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);
    machine_container_file_init(&container_file);
    machine_elf_file_init(&elf_file);
    machine_image_file_init(&image_file);

    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1u;
    emit_program.functions[0].block_capacity = 1u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
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
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_VOID_IMM;
    emit_program.functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("sink");
    emit_program.functions[0].blocks[0].ops[0].as.call.arg_count = 1u;
    emit_program.functions[0].blocks[0].ops[0].as.call.args =
        (MachineEmitOperand *)calloc(1u, sizeof(MachineEmitOperand));
    if (!emit_program.functions[0].blocks[0].ops[0].as.call.callee_name ||
        !emit_program.functions[0].blocks[0].ops[0].as.call.args) {
        machine_emit_program_free(&emit_program);
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
        !machine_elf_build_from_machine_container_file_with_profile(
            &container_file,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &elf_file,
            &elf_error) ||
        !machine_image_build_from_machine_elf_file(&elf_file, &image_file, &image_error) ||
        !machine_image_file_get_header_summary(&image_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        header_summary.base_virtual_address != 0x08048000u ||
        !header_summary.has_entry ||
        header_summary.entry_virtual_address != 0x08048000u ||
        !machine_image_file_find_symbol_by_name(&image_file, "sink", NULL, &symbol) || !symbol ||
        symbol->is_defined ||
        !machine_image_file_find_symbol_by_virtual_address(&image_file, 0x08048000u, NULL, &symbol) || !symbol ||
        strcmp(symbol->name, "F0.L0") != 0 ||
        !machine_image_file_find_relocation_by_site_virtual_address(&image_file, 0x08048000u, &relocation_index, &relocation) ||
        !relocation || relocation_index != 0u ||
        !machine_image_file_get_relocation(&image_file, 0u, &relocation) || !relocation ||
        relocation->source_kind != MACHINE_BYTES_FIXUP_CALL_TARGET ||
        relocation->type != (uint32_t)R_386_PLT32 ||
        relocation->site_virtual_address != 0x08048000u ||
        relocation->is_resolved ||
        relocation->target_virtual_address != 0u ||
        !machine_image_file_get_unresolved_relocation_count(&image_file, &unresolved_count) ||
        unresolved_count != 1u ||
        !machine_image_file_get_unresolved_relocation_by_index(&image_file, 0u, &relocation_index, &relocation) ||
        !relocation || relocation_index != 0u ||
        strcmp(relocation->symbol_name, "sink") != 0) {
        fprintf(stderr,
            "[machine-image] FAIL: unresolved external call mismatch: %s"
            " profile=%d base=0x%zx has_entry=%d entry=0x%zx"
            " symbol=%s defined=%d"
            " reloc(kind=%d type=%u site=0x%zx resolved=%d target=0x%zx name=%s)\n",
            image_error.message,
            (int)header_summary.target_profile,
            header_summary.base_virtual_address,
            header_summary.has_entry,
            header_summary.entry_virtual_address,
            symbol && symbol->name ? symbol->name : "<null>",
            symbol ? symbol->is_defined : -1,
            relocation ? (int)relocation->source_kind : -1,
            relocation ? relocation->type : 0u,
            relocation ? relocation->site_virtual_address : 0u,
            relocation ? relocation->is_resolved : -1,
            relocation ? relocation->target_virtual_address : 0u,
            relocation && relocation->symbol_name ? relocation->symbol_name : "<null>");
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    machine_container_file_free(&container_file);
    machine_elf_file_free(&elf_file);
    machine_image_file_free(&image_file);
    return ok;
}

static int test_machine_image_report_helpers(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfReport elf_report;
    MachineImageReport image_report;
    MachineImageError image_error;
    const MachineImageHeaderSummary *header_summary = NULL;
    const MachineImageSymbolSummary *symbol_summary = NULL;
    const MachineImageRelocationSummary *relocation_summary = NULL;
    const MachineImageSegmentSummary *segment_summary = NULL;
    const MachineImageSymbol *symbol = NULL;
    const MachineImageRelocation *relocation = NULL;
    size_t segment_count = 0u;
    size_t symbol_count = 0u;
    size_t relocation_count = 0u;
    size_t byte_count = 0u;
    size_t resolved_count = 0u;
    size_t unresolved_count = 0u;
    size_t segment_symbol_count = 0u;
    size_t segment_relocation_count = 0u;
    size_t symbol_index = 0u;
    size_t relocation_index = 0u;
    char *dump_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&image_error, 0, sizeof(image_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_report_init(&elf_report);
    machine_image_report_init(&image_report);

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
        fprintf(stderr, "[machine-image] FAIL: report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_report_free(&elf_report);
        machine_image_report_free(&image_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-image] FAIL: report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_report_free(&elf_report);
        machine_image_report_free(&image_report);
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
        fprintf(stderr, "[machine-image] FAIL: report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_report_free(&elf_report);
        machine_image_report_free(&image_report);
        return 0;
    }

    if (!machine_elf_build_report_from_machine_ir_report(&machine_report, &elf_report, NULL) ||
        !machine_image_build_report_from_machine_elf_report(&elf_report, &image_report, &image_error) ||
        !machine_image_report_get_summary(&image_report, &segment_count, &symbol_count, &relocation_count, &byte_count) ||
        segment_count != 1u || symbol_count != 5u || relocation_count != 2u || byte_count != 9u ||
        !machine_image_report_get_header_summary_artifact(&image_report, &header_summary) || !header_summary ||
        header_summary->has_entry != 1 || header_summary->entry_virtual_address != 0x1000u ||
        !machine_image_report_get_entry_symbol_summary_artifact(&image_report, &symbol_summary) || !symbol_summary ||
        strcmp(symbol_summary->name, "main") != 0 || symbol_summary->virtual_address != 0x1000u ||
        !machine_image_report_find_segment_summary_by_name(&image_report, ".text", NULL, &segment_summary) || !segment_summary ||
        segment_summary->virtual_address != 0x1000u ||
        !machine_image_report_find_segment_summary_covering_virtual_address(&image_report, 0x1004u, NULL, &segment_summary) ||
        !segment_summary || strcmp(segment_summary->name, ".text") != 0 ||
        !machine_image_file_get_segment_symbol_count(&image_report.file, 0u, &segment_symbol_count) ||
        segment_symbol_count != 4u ||
        !machine_image_file_get_segment_symbol_by_index(&image_report.file, 0u, 3u, &symbol_index, &symbol) ||
        !symbol || symbol_index != 4u || strcmp(symbol->name, "main") != 0 ||
        !machine_image_report_find_symbol_summary_by_name(&image_report, "F0.L2", NULL, &symbol_summary) || !symbol_summary ||
        symbol_summary->virtual_address != 0x1007u ||
        !machine_image_report_get_segment_symbol_summary_count(&image_report, 0u, &segment_symbol_count) ||
        segment_symbol_count != 4u ||
        !machine_image_report_get_segment_symbol_summary_by_index(&image_report, 0u, 2u, &symbol_index, &symbol_summary) ||
        !symbol_summary || symbol_index != 3u || strcmp(symbol_summary->name, "F0.L2") != 0 ||
        !machine_image_report_find_symbol_summary_by_virtual_address(&image_report, 0x1007u, NULL, &symbol_summary) ||
        !symbol_summary || strcmp(symbol_summary->name, "F0.L2") != 0 ||
        !machine_image_file_get_segment_relocation_count(&image_report.file, 0u, &segment_relocation_count) ||
        segment_relocation_count != 2u ||
        !machine_image_file_get_segment_relocation_by_index(
            &image_report.file,
            0u,
            1u,
            &relocation_index,
            &relocation) ||
        !relocation || relocation_index != 1u || relocation->target_virtual_address != 0x1005u ||
        !machine_image_report_get_relocation_summary(&image_report, 0u, &relocation_summary) || !relocation_summary ||
        relocation_summary->is_resolved != 1 || relocation_summary->target_virtual_address != 0x1007u ||
        !machine_image_report_get_segment_relocation_summary_count(&image_report, 0u, &segment_relocation_count) ||
        segment_relocation_count != 2u ||
        !machine_image_report_get_segment_relocation_summary_by_index(
            &image_report,
            0u,
            0u,
            &relocation_index,
            &relocation_summary) ||
        !relocation_summary || relocation_index != 0u || relocation_summary->site_virtual_address != 0x1004u ||
        !machine_image_report_find_relocation_summary_by_site_virtual_address(&image_report, 0x1004u, &relocation_index, &relocation_summary) ||
        !relocation_summary || relocation_index != 0u ||
        !machine_image_report_get_resolved_relocation_count(&image_report, &resolved_count) ||
        !machine_image_report_get_unresolved_relocation_count(&image_report, &unresolved_count) ||
        resolved_count != 2u || unresolved_count != 0u ||
        !machine_image_report_get_resolved_relocation_index(&image_report, 1u, &relocation_index) ||
        relocation_index != 1u ||
        !machine_image_report_get_resolved_relocation_summary_by_index(
            &image_report,
            1u,
            &relocation_index,
            &relocation_summary) ||
        !relocation_summary || relocation_index != 1u || relocation_summary->target_virtual_address != 0x1005u ||
        !machine_image_dump_report(&image_report, &dump_text, &image_error) ||
        !dump_text ||
        strstr(dump_text, "machine_image profile=generic-elf32") == NULL ||
        strstr(dump_text, "entry=main") == NULL ||
        strstr(dump_text, "report_overview:") == NULL ||
        strstr(dump_text, "report_symbol_filters:") == NULL ||
        strstr(dump_text, "report_relocation_filters:") == NULL ||
        strstr(dump_text, "report_segments:") == NULL ||
        strstr(dump_text, "defined count=4") == NULL ||
        strstr(dump_text, "resolved count=2") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: report helper mismatch: %s\n", image_error.message);
        ok = 0;
    }

    free(dump_text);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_report_free(&elf_report);
    machine_image_report_free(&image_report);
    return ok;
}

static int test_machine_image_build_from_elf_bytes_helpers(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineImageFile image_file;
    MachineImageReport image_report;
    MachineElfError elf_error;
    MachineImageError image_error;
    MachineImageHeaderSummary header_summary;
    const MachineImageSymbolSummary *symbol_summary = NULL;
    unsigned char *elf_bytes = NULL;
    size_t elf_byte_count = 0u;
    char *dump_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&image_error, 0, sizeof(image_error));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_image_file_init(&image_file);
    machine_image_report_init(&image_report);

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
        fprintf(stderr, "[machine-image] FAIL: bytes-helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_image_file_free(&image_file);
        machine_image_report_free(&image_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-image] FAIL: bytes-helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_image_file_free(&image_file);
        machine_image_report_free(&image_report);
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
        fprintf(stderr, "[machine-image] FAIL: bytes-helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_image_file_free(&image_file);
        machine_image_report_free(&image_report);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &elf_bytes, &elf_byte_count, &elf_error) ||
        !machine_image_build_from_machine_elf_bytes(elf_bytes, elf_byte_count, &image_file, &image_error) ||
        !machine_image_file_get_header_summary(&image_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        header_summary.entry_virtual_address != 0x1000u ||
        !machine_image_build_report_from_machine_elf_bytes(
            elf_bytes,
            elf_byte_count,
            &image_report,
            &image_error) ||
        !machine_image_report_get_entry_symbol_summary_artifact(&image_report, &symbol_summary) ||
        !symbol_summary ||
        strcmp(symbol_summary->name, "main") != 0 ||
        !machine_image_build_report_dump_from_machine_elf_bytes(
            elf_bytes,
            elf_byte_count,
            &dump_text,
            &image_error) ||
        !dump_text ||
        strstr(dump_text, "machine_image profile=generic-elf32") == NULL ||
        strstr(dump_text, "entry=main") == NULL ||
        strstr(dump_text, "report_overview:") == NULL ||
        strstr(dump_text, "report_relocation_filters:") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: bytes-helper mismatch: %s\n", image_error.message);
        ok = 0;
    }

    free(elf_bytes);
    free(dump_text);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_image_file_free(&image_file);
    machine_image_report_free(&image_report);
    return ok;
}

static int test_machine_image_clone_and_dump_helpers(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfReport elf_report;
    MachineImageFile image_file;
    MachineImageFile cloned_image;
    MachineImageReport image_report;
    MachineElfError elf_error;
    MachineImageError image_error;
    const MachineImageSymbol *symbol = NULL;
    const MachineImageSymbolSummary *entry_symbol_summary = NULL;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&image_error, 0, sizeof(image_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_report_init(&elf_report);
    machine_image_file_init(&image_file);
    machine_image_file_init(&cloned_image);
    machine_image_report_init(&image_report);

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
        fprintf(stderr, "[machine-image] FAIL: clone-helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&elf_report);
        machine_image_file_free(&image_file);
        machine_image_file_free(&cloned_image);
        machine_image_report_free(&image_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-image] FAIL: clone-helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&elf_report);
        machine_image_file_free(&image_file);
        machine_image_file_free(&cloned_image);
        machine_image_report_free(&image_report);
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
        fprintf(stderr, "[machine-image] FAIL: clone-helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&elf_report);
        machine_image_file_free(&image_file);
        machine_image_file_free(&cloned_image);
        machine_image_report_free(&image_report);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_build_report_from_machine_ir_report(&machine_report, &elf_report, &elf_error) ||
        !machine_image_build_from_machine_elf_file(&elf_file, &image_file, &image_error) ||
        !machine_image_clone_file(&image_file, &cloned_image, &image_error) ||
        !cloned_image.symbols[4].name) {
        fprintf(stderr, "[machine-image] FAIL: clone-helper setup failed: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    free(cloned_image.symbols[4].name);
    cloned_image.symbols[4].name = dup_text("entry");
    cloned_image.has_entry = 1;
    cloned_image.entry_symbol_index = 4u;
    cloned_image.entry_virtual_address = cloned_image.symbols[4].virtual_address;
    if (!cloned_image.symbols[4].name ||
        !machine_image_verify_file(&cloned_image, &image_error) ||
        !machine_image_file_find_symbol_by_name(&cloned_image, "entry", NULL, &symbol) || !symbol ||
        machine_image_file_find_symbol_by_name(&image_file, "entry", NULL, NULL) ||
        !machine_image_build_report_from_file(&cloned_image, &image_report, &image_error) ||
        !machine_image_report_get_entry_symbol_summary_artifact(&image_report, &entry_symbol_summary) ||
        !entry_symbol_summary || strcmp(entry_symbol_summary->name, "entry") != 0 ||
        !machine_image_report_refresh(&image_report, &image_error) ||
        !machine_image_report_get_entry_symbol_summary_artifact(&image_report, &entry_symbol_summary) ||
        !entry_symbol_summary || strcmp(entry_symbol_summary->name, "entry") != 0 ||
        !machine_image_build_dump_from_file(&cloned_image, &dump_text, &image_error) ||
        !dump_text || strstr(dump_text, "entry=entry") == NULL ||
        !machine_image_build_report_dump_from_file(&cloned_image, &report_dump_text, &image_error) ||
        !report_dump_text || strstr(report_dump_text, "entry=entry") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: clone/report-refresh helper mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;
    if (!machine_image_build_dump_from_machine_elf_file(&elf_file, &dump_text, &image_error) ||
        !dump_text || strstr(dump_text, "machine_image profile=generic-elf32") == NULL ||
        !machine_image_build_report_dump_from_machine_elf_file(&elf_file, &report_dump_text, &image_error) ||
        !report_dump_text || strstr(report_dump_text, "entry=main") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: dump-from-elf-file helper mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;
    if (!machine_image_build_dump_from_machine_elf_report(&elf_report, &dump_text, &image_error) ||
        !dump_text || strstr(dump_text, "machine_image profile=generic-elf32") == NULL ||
        !machine_image_build_report_dump_from_machine_elf_report(&elf_report, &report_dump_text, &image_error) ||
        !report_dump_text || strstr(report_dump_text, "entry=main") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: dump-from-elf-report helper mismatch: %s\n", image_error.message);
        ok = 0;
    }

cleanup:
    free(dump_text);
    free(report_dump_text);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_report_free(&elf_report);
    machine_image_file_free(&image_file);
    machine_image_file_free(&cloned_image);
    machine_image_report_free(&image_report);
    return ok;
}

static int test_machine_image_profile_and_ir_dump_helpers(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfReport elf_report;
    MachineImageFile generic_image;
    MachineImageFile profiled_image;
    MachineImageFile temp_image;
    MachineImageReport profiled_report;
    MachineImageReport reprofiled_report;
    MachineElfError elf_error;
    MachineImageError image_error;
    MachineImageTargetPolicySummary expected_policy;
    MachineImageTargetPolicySummary file_policy;
    MachineElfArtifactSummary source_artifact_summary;
    const MachineElfArtifactSummary *report_source_artifact = NULL;
    const MachineImageTargetPolicySummary *report_policy = NULL;
    const MachineImageHeaderSummary *report_header = NULL;
    MachineImageHeaderSummary header_summary;
    MachineElfTargetProfile target_profile;
    unsigned char *elf_bytes = NULL;
    size_t elf_byte_count = 0u;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&image_error, 0, sizeof(image_error));
    memset(&expected_policy, 0, sizeof(expected_policy));
    memset(&file_policy, 0, sizeof(file_policy));
    memset(&source_artifact_summary, 0, sizeof(source_artifact_summary));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_report_init(&elf_report);
    machine_image_file_init(&generic_image);
    machine_image_file_init(&profiled_image);
    machine_image_file_init(&temp_image);
    machine_image_report_init(&profiled_report);
    machine_image_report_init(&reprofiled_report);

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
        fprintf(stderr, "[machine-image] FAIL: profile-helper setup failed: %s\n", machine_error.message);
        ok = 0;
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-image] FAIL: profile-helper setup failed: %s\n", machine_error.message);
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
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-image] FAIL: profile-helper setup failed: %s\n", machine_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_build_report_from_machine_ir_report(&machine_report, &elf_report, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &elf_bytes, &elf_byte_count, &elf_error) ||
        !machine_image_get_target_policy_summary(
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &expected_policy) ||
        expected_policy.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        expected_policy.base_virtual_address != 0x08048000u ||
        expected_policy.segment_alignment != 0x1000u ||
        !machine_image_build_from_machine_ir_report(&machine_report, &generic_image, &image_error) ||
        !machine_image_build_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &profiled_image,
            &image_error) ||
        !machine_image_file_get_source_elf_artifact_summary(&profiled_image, &source_artifact_summary) ||
        !machine_image_file_get_target_profile(&profiled_image, &target_profile) ||
        target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        source_artifact_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        source_artifact_summary.origin_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        source_artifact_summary.relocation_semantics != MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS ||
        !machine_image_file_get_target_policy_summary(&profiled_image, &file_policy) ||
        file_policy.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        file_policy.base_virtual_address != 0x08048000u ||
        file_policy.segment_alignment != 0x1000u ||
        !machine_image_file_get_header_summary(&profiled_image, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        header_summary.base_virtual_address != 0x08048000u ||
        header_summary.entry_virtual_address != 0x08048000u ||
        !machine_image_build_report_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &profiled_report,
            &image_error) ||
        !machine_image_report_get_source_elf_artifact_summary_artifact(&profiled_report, &report_source_artifact) ||
        !report_source_artifact ||
        !machine_image_report_get_target_policy_summary_artifact(&profiled_report, &report_policy) ||
        !report_policy ||
        report_source_artifact->target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        report_source_artifact->origin_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        report_source_artifact->relocation_semantics != MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS ||
        report_policy->target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        report_policy->base_virtual_address != 0x08048000u ||
        report_policy->segment_alignment != 0x1000u ||
        !machine_image_build_report_from_file_with_profile(
            &generic_image,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &reprofiled_report,
            &image_error) ||
        !machine_image_report_get_source_elf_artifact_summary_artifact(&reprofiled_report, &report_source_artifact) ||
        !report_source_artifact ||
        !machine_image_report_get_header_summary_artifact(&reprofiled_report, &report_header) ||
        !report_header ||
        report_source_artifact->target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        report_source_artifact->origin_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        report_source_artifact->relocation_semantics != MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS ||
        report_header->target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        report_header->base_virtual_address != 0x08048000u ||
        report_header->entry_virtual_address != 0x08048000u ||
        !machine_image_report_get_target_policy_summary_artifact(&reprofiled_report, &report_policy) ||
        !report_policy ||
        report_policy->target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        !machine_image_build_from_machine_elf_file_with_profile(
            &elf_file,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &temp_image,
            &image_error) ||
        !machine_image_file_get_source_elf_artifact_summary(&temp_image, &source_artifact_summary) ||
        !machine_image_file_get_header_summary(&temp_image, &header_summary) ||
        source_artifact_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        source_artifact_summary.origin_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        source_artifact_summary.relocation_semantics != MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        header_summary.base_virtual_address != 0x08048000u ||
        !machine_image_build_from_machine_elf_report_with_profile(
            &elf_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &temp_image,
            &image_error) ||
        !machine_image_file_get_source_elf_artifact_summary(&temp_image, &source_artifact_summary) ||
        !machine_image_file_get_header_summary(&temp_image, &header_summary) ||
        source_artifact_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        source_artifact_summary.origin_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        source_artifact_summary.relocation_semantics != MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        !machine_image_build_from_machine_elf_bytes_with_profile(
            elf_bytes,
            elf_byte_count,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &temp_image,
            &image_error) ||
        !machine_image_file_get_source_elf_artifact_summary(&temp_image, &source_artifact_summary) ||
        !machine_image_file_get_header_summary(&temp_image, &header_summary) ||
        source_artifact_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        source_artifact_summary.origin_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        source_artifact_summary.relocation_semantics != MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW) {
        fprintf(stderr, "[machine-image] FAIL: profile/image helper mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_image_build_dump_from_machine_elf_bytes_with_profile(
            elf_bytes,
            elf_byte_count,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &dump_text,
            &image_error) ||
        !dump_text ||
        strstr(dump_text, "machine_image profile=i386-preview") == NULL ||
        strstr(dump_text, "elf_origin=generic-elf32 elf_semantics=imported-relocation-table") == NULL ||
        strstr(dump_text, "base=0x8048000") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: dump-from-elf-bytes-with-profile mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    free(dump_text);
    dump_text = NULL;
    if (!machine_image_build_dump_from_machine_ir_report(&machine_report, &dump_text, &image_error) ||
        !dump_text ||
        strstr(dump_text, "machine_image profile=generic-elf32") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: dump-from-machine-ir-report mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    free(dump_text);
    dump_text = NULL;
    if (!machine_image_build_dump_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &dump_text,
            &image_error) ||
        !dump_text ||
        strstr(dump_text, "machine_image profile=i386-preview") == NULL ||
        strstr(dump_text, "elf_origin=i386-preview elf_semantics=direct-patch-spans") == NULL ||
        strstr(dump_text, "entry=main") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: dump-from-machine-ir-report-with-profile mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_image_build_report_dump_from_file_with_profile(
            &generic_image,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &report_dump_text,
            &image_error) ||
        !report_dump_text ||
        strstr(report_dump_text, "machine_image profile=i386-preview") == NULL ||
        strstr(report_dump_text, "elf_source: target=i386-preview origin=generic-elf32 semantics=direct-patch-spans") == NULL ||
        strstr(report_dump_text, "report_overview:") == NULL ||
        strstr(report_dump_text, "policy: profile=i386-preview") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: report-dump-from-file-with-profile mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    free(report_dump_text);
    report_dump_text = NULL;
    if (!machine_image_build_report_dump_from_machine_elf_file_with_profile(
            &elf_file,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &report_dump_text,
            &image_error) ||
        !report_dump_text ||
        strstr(report_dump_text, "machine_image profile=i386-preview") == NULL ||
        strstr(report_dump_text, "elf_source: target=i386-preview origin=generic-elf32 semantics=direct-patch-spans") == NULL ||
        strstr(report_dump_text, "report_symbol_filters:") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: report-dump-from-elf-file-with-profile mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    free(report_dump_text);
    report_dump_text = NULL;
    if (!machine_image_build_report_dump_from_machine_elf_report_with_profile(
            &elf_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &report_dump_text,
            &image_error) ||
        !report_dump_text ||
        strstr(report_dump_text, "machine_image profile=i386-preview") == NULL ||
        strstr(report_dump_text, "elf_source: target=i386-preview origin=generic-elf32 semantics=direct-patch-spans") == NULL ||
        strstr(report_dump_text, "report_segments:") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: report-dump-from-elf-report-with-profile mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    free(report_dump_text);
    report_dump_text = NULL;
    if (!machine_image_build_report_dump_from_machine_elf_bytes_with_profile(
            elf_bytes,
            elf_byte_count,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &report_dump_text,
            &image_error) ||
        !report_dump_text ||
        strstr(report_dump_text, "machine_image profile=i386-preview") == NULL ||
        strstr(report_dump_text, "elf_source: target=i386-preview origin=generic-elf32 semantics=imported-relocation-table") == NULL ||
        strstr(report_dump_text, "report_relocation_filters:") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: report-dump-from-elf-bytes-with-profile mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    free(report_dump_text);
    report_dump_text = NULL;
    if (!machine_image_build_report_dump_from_machine_ir_report(
            &machine_report,
            &report_dump_text,
            &image_error) ||
        !report_dump_text ||
        strstr(report_dump_text, "machine_image profile=generic-elf32") == NULL ||
        strstr(report_dump_text, "report_overview:") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: report-dump-from-machine-ir-report mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    free(report_dump_text);
    report_dump_text = NULL;
    if (!machine_image_build_report_dump_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &report_dump_text,
            &image_error) ||
        !report_dump_text ||
        strstr(report_dump_text, "machine_image profile=i386-preview") == NULL ||
        strstr(report_dump_text, "elf_source: target=i386-preview origin=i386-preview semantics=direct-patch-spans") == NULL ||
        strstr(report_dump_text, "entry=main") == NULL ||
        strstr(report_dump_text, "report_symbol_filters:") == NULL ||
        strstr(report_dump_text, "report_relocation_filters:") == NULL) {
        fprintf(stderr, "[machine-image] FAIL: report-dump-from-machine-ir-report-with-profile mismatch: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

cleanup:
    free(elf_bytes);
    free(dump_text);
    free(report_dump_text);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_report_free(&elf_report);
    machine_image_file_free(&generic_image);
    machine_image_file_free(&profiled_image);
    machine_image_file_free(&temp_image);
    machine_image_report_free(&profiled_report);
    machine_image_report_free(&reprofiled_report);
    return ok;
}

static int test_machine_image_segment_cached_subset_helpers(void) {
    MachineImageFile image_file;
    MachineImageReport image_report;
    MachineImageError image_error;
    MachineImageReportOverviewArtifact overview_artifact;
    MachineImageReportSegmentArtifact artifact;
    MachineImageReportSymbolArtifact symbol_artifact;
    MachineImageReportRelocationArtifact relocation_artifact;
    const MachineImageSymbol *symbol = NULL;
    const MachineImageRelocation *relocation = NULL;
    const MachineImageSymbolSummary *symbol_summary = NULL;
    const MachineImageRelocationSummary *relocation_summary = NULL;
    const MachineImageSegmentSummary *segment_summary = NULL;
    size_t count = 0u;
    size_t index = 0u;
    int ok = 1;

    memset(&image_error, 0, sizeof(image_error));
    machine_image_file_init(&image_file);
    machine_image_report_init(&image_report);

    image_file.target_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    image_file.source_elf_artifact_summary.target_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    image_file.source_elf_artifact_summary.origin_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    image_file.source_elf_artifact_summary.relocation_semantics = MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS;
    image_file.base_virtual_address = 0x1000u;
    image_file.has_entry = 1;
    image_file.entry_symbol_index = 1u;
    image_file.entry_virtual_address = 0x1000u;
    image_file.segment_count = 2u;
    image_file.segment_capacity = 2u;
    image_file.segments = (MachineImageSegment *)calloc(2u, sizeof(MachineImageSegment));
    image_file.symbol_count = 5u;
    image_file.symbol_capacity = 5u;
    image_file.symbols = (MachineImageSymbol *)calloc(5u, sizeof(MachineImageSymbol));
    image_file.relocation_count = 3u;
    image_file.relocation_capacity = 3u;
    image_file.relocations = (MachineImageRelocation *)calloc(3u, sizeof(MachineImageRelocation));
    image_file.byte_count = 12u;
    image_file.bytes = (unsigned char *)calloc(12u, sizeof(unsigned char));
    if (!image_file.segments || !image_file.symbols || !image_file.relocations || !image_file.bytes) {
        ok = 0;
        goto cleanup;
    }

    image_file.segments[0].name = dup_text(".text");
    image_file.segments[0].image_offset = 0u;
    image_file.segments[0].virtual_address = 0x1000u;
    image_file.segments[0].byte_count = 8u;
    image_file.segments[0].align = 4096u;
    image_file.segments[1].name = dup_text(".cold");
    image_file.segments[1].image_offset = 8u;
    image_file.segments[1].virtual_address = 0x1008u;
    image_file.segments[1].byte_count = 4u;
    image_file.segments[1].align = 4096u;

    image_file.symbols[0].name = dup_text("ext");
    image_file.symbols[0].binding = MACHINE_ELF_SYMBOL_GLOBAL;
    image_file.symbols[0].type = MACHINE_ELF_SYMBOL_FUNC;
    image_file.symbols[0].is_defined = 0;
    image_file.symbols[0].segment_index = (size_t)-1;
    image_file.symbols[0].value_offset = 0u;
    image_file.symbols[0].virtual_address = 0u;

    image_file.symbols[1].name = dup_text("main");
    image_file.symbols[1].binding = MACHINE_ELF_SYMBOL_GLOBAL;
    image_file.symbols[1].type = MACHINE_ELF_SYMBOL_FUNC;
    image_file.symbols[1].is_defined = 1;
    image_file.symbols[1].segment_index = 0u;
    image_file.symbols[1].value_offset = 0u;
    image_file.symbols[1].virtual_address = 0x1000u;

    image_file.symbols[2].name = dup_text("F0.L1");
    image_file.symbols[2].binding = MACHINE_ELF_SYMBOL_LOCAL;
    image_file.symbols[2].type = MACHINE_ELF_SYMBOL_NOTYPE;
    image_file.symbols[2].is_defined = 1;
    image_file.symbols[2].segment_index = 0u;
    image_file.symbols[2].value_offset = 4u;
    image_file.symbols[2].virtual_address = 0x1004u;

    image_file.symbols[3].name = dup_text("cold");
    image_file.symbols[3].binding = MACHINE_ELF_SYMBOL_GLOBAL;
    image_file.symbols[3].type = MACHINE_ELF_SYMBOL_FUNC;
    image_file.symbols[3].is_defined = 1;
    image_file.symbols[3].segment_index = 1u;
    image_file.symbols[3].value_offset = 0u;
    image_file.symbols[3].virtual_address = 0x1008u;

    image_file.symbols[4].name = dup_text("F1.L1");
    image_file.symbols[4].binding = MACHINE_ELF_SYMBOL_LOCAL;
    image_file.symbols[4].type = MACHINE_ELF_SYMBOL_NOTYPE;
    image_file.symbols[4].is_defined = 1;
    image_file.symbols[4].segment_index = 1u;
    image_file.symbols[4].value_offset = 2u;
    image_file.symbols[4].virtual_address = 0x100au;

    image_file.relocations[0].source_kind = MACHINE_BYTES_FIXUP_CONTROL_PRIMARY;
    image_file.relocations[0].segment_index = 0u;
    image_file.relocations[0].segment_offset = 1u;
    image_file.relocations[0].site_virtual_address = 0x1001u;
    image_file.relocations[0].is_resolved = 1;
    image_file.relocations[0].target_virtual_address = 0x1004u;
    image_file.relocations[0].type = 2u;
    image_file.relocations[0].symbol_index = 2u;
    image_file.relocations[0].symbol_name = dup_text("F0.L1");

    image_file.relocations[1].source_kind = MACHINE_BYTES_FIXUP_CALL_TARGET;
    image_file.relocations[1].segment_index = 0u;
    image_file.relocations[1].segment_offset = 3u;
    image_file.relocations[1].site_virtual_address = 0x1003u;
    image_file.relocations[1].is_resolved = 0;
    image_file.relocations[1].target_virtual_address = 0u;
    image_file.relocations[1].type = 1u;
    image_file.relocations[1].symbol_index = 0u;
    image_file.relocations[1].symbol_name = dup_text("ext");

    image_file.relocations[2].source_kind = MACHINE_BYTES_FIXUP_CONTROL_SECONDARY;
    image_file.relocations[2].segment_index = 1u;
    image_file.relocations[2].segment_offset = 1u;
    image_file.relocations[2].site_virtual_address = 0x1009u;
    image_file.relocations[2].is_resolved = 1;
    image_file.relocations[2].target_virtual_address = 0x1000u;
    image_file.relocations[2].type = 3u;
    image_file.relocations[2].symbol_index = 1u;
    image_file.relocations[2].symbol_name = dup_text("main");

    if (!image_file.segments[0].name || !image_file.segments[1].name ||
        !image_file.symbols[0].name || !image_file.symbols[1].name ||
        !image_file.symbols[2].name || !image_file.symbols[3].name ||
        !image_file.symbols[4].name || !image_file.relocations[0].symbol_name ||
        !image_file.relocations[1].symbol_name || !image_file.relocations[2].symbol_name ||
        !machine_image_verify_file(&image_file, &image_error) ||
        !machine_image_file_get_segment_symbol_count(&image_file, 0u, &count) || count != 2u ||
        !machine_image_file_get_segment_symbol_by_index(&image_file, 0u, 0u, &index, &symbol) ||
        !symbol || index != 1u || strcmp(symbol->name, "main") != 0 ||
        !machine_image_file_get_segment_symbol_by_index(&image_file, 1u, 1u, &index, &symbol) ||
        !symbol || index != 4u || strcmp(symbol->name, "F1.L1") != 0 ||
        !machine_image_file_get_segment_relocation_count(&image_file, 0u, &count) || count != 2u ||
        !machine_image_file_get_segment_resolved_relocation_count(&image_file, 0u, &count) || count != 1u ||
        !machine_image_file_get_segment_unresolved_relocation_count(&image_file, 0u, &count) || count != 1u ||
        !machine_image_file_get_segment_resolved_relocation_by_index(&image_file, 0u, 0u, &index, &relocation) ||
        !relocation || index != 0u || relocation->target_virtual_address != 0x1004u ||
        !machine_image_file_get_segment_unresolved_relocation_by_index(&image_file, 0u, 0u, &index, &relocation) ||
        !relocation || index != 1u || strcmp(relocation->symbol_name, "ext") != 0 ||
        !machine_image_file_get_segment_relocation_by_index(&image_file, 1u, 0u, &index, &relocation) ||
        !relocation || index != 2u || relocation->target_virtual_address != 0x1000u ||
        !machine_image_build_report_from_file(&image_file, &image_report, &image_error) ||
        !machine_image_report_get_overview_artifact(&image_report, &overview_artifact) ||
        !overview_artifact.header_summary || !overview_artifact.target_policy_summary ||
        overview_artifact.segment_count != 2u || overview_artifact.symbol_count != 5u ||
        overview_artifact.relocation_count != 3u || overview_artifact.byte_count != 12u ||
        overview_artifact.resolved_relocation_count != 2u || overview_artifact.unresolved_relocation_count != 1u ||
        !overview_artifact.has_entry_symbol_artifact ||
        strcmp(overview_artifact.entry_symbol_artifact.symbol_summary->name, "main") != 0 ||
        !machine_image_report_get_symbol_filter_count(
            &image_report,
            MACHINE_IMAGE_SYMBOL_FILTER_DEFINED,
            &count) || count != 4u ||
        !machine_image_report_get_symbol_filter_count(
            &image_report,
            MACHINE_IMAGE_SYMBOL_FILTER_UNDEFINED,
            &count) || count != 1u ||
        !machine_image_report_get_symbol_filter_count(
            &image_report,
            MACHINE_IMAGE_SYMBOL_FILTER_GLOBAL,
            &count) || count != 3u ||
        !machine_image_report_get_symbol_filter_count(
            &image_report,
            MACHINE_IMAGE_SYMBOL_FILTER_LOCAL,
            &count) || count != 2u ||
        !machine_image_report_get_symbol_filter_count(
            &image_report,
            MACHINE_IMAGE_SYMBOL_FILTER_DEFINED_GLOBAL,
            &count) || count != 2u ||
        !machine_image_report_get_symbol_filter_count(
            &image_report,
            MACHINE_IMAGE_SYMBOL_FILTER_UNDEFINED_GLOBAL,
            &count) || count != 1u ||
        !machine_image_report_get_symbol_summary_by_filter_index(
            &image_report,
            MACHINE_IMAGE_SYMBOL_FILTER_DEFINED,
            3u,
            &index,
            &symbol_summary) ||
        !symbol_summary || index != 4u || strcmp(symbol_summary->name, "F1.L1") != 0 ||
        !machine_image_report_get_symbol_artifact_by_filter_index(
            &image_report,
            MACHINE_IMAGE_SYMBOL_FILTER_DEFINED_GLOBAL,
            1u,
            &index,
            &symbol_artifact) ||
        index != 3u || !symbol_artifact.symbol_summary ||
        strcmp(symbol_artifact.symbol_summary->name, "cold") != 0 ||
        !machine_image_report_overview_artifact_get_symbol_summary(
            &overview_artifact,
            MACHINE_IMAGE_SYMBOL_FILTER_UNDEFINED_GLOBAL,
            0u,
            &index,
            &symbol_summary) ||
        !symbol_summary || index != 0u || strcmp(symbol_summary->name, "ext") != 0 ||
        !machine_image_report_overview_artifact_get_symbol_artifact(
            &overview_artifact,
            MACHINE_IMAGE_SYMBOL_FILTER_LOCAL,
            1u,
            &index,
            &symbol_artifact) ||
        index != 4u || !symbol_artifact.symbol_summary ||
        strcmp(symbol_artifact.symbol_summary->name, "F1.L1") != 0 ||
        !machine_image_report_get_relocation_filter_count(
            &image_report,
            MACHINE_IMAGE_RELOCATION_FILTER_RESOLVED,
            &count) || count != 2u ||
        !machine_image_report_get_relocation_filter_count(
            &image_report,
            MACHINE_IMAGE_RELOCATION_FILTER_UNRESOLVED,
            &count) || count != 1u ||
        !machine_image_report_get_relocation_filter_count(
            &image_report,
            MACHINE_IMAGE_RELOCATION_FILTER_CALL,
            &count) || count != 1u ||
        !machine_image_report_get_relocation_filter_count(
            &image_report,
            MACHINE_IMAGE_RELOCATION_FILTER_CONTROL_PRIMARY,
            &count) || count != 1u ||
        !machine_image_report_get_relocation_filter_count(
            &image_report,
            MACHINE_IMAGE_RELOCATION_FILTER_CONTROL_SECONDARY,
            &count) || count != 1u ||
        !machine_image_report_get_relocation_summary_by_filter_index(
            &image_report,
            MACHINE_IMAGE_RELOCATION_FILTER_CALL,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 1u || strcmp(relocation_summary->symbol_name, "ext") != 0 ||
        !machine_image_report_get_relocation_artifact_by_filter_index(
            &image_report,
            MACHINE_IMAGE_RELOCATION_FILTER_CONTROL_SECONDARY,
            0u,
            &index,
            &relocation_artifact) ||
        index != 2u || !relocation_artifact.relocation_summary ||
        relocation_artifact.relocation_summary->target_virtual_address != 0x1000u ||
        !machine_image_report_overview_artifact_get_relocation_summary(
            &overview_artifact,
            MACHINE_IMAGE_RELOCATION_FILTER_UNRESOLVED,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 1u || strcmp(relocation_summary->symbol_name, "ext") != 0 ||
        !machine_image_report_overview_artifact_get_relocation_artifact(
            &overview_artifact,
            MACHINE_IMAGE_RELOCATION_FILTER_RESOLVED,
            1u,
            &index,
            &relocation_artifact) ||
        index != 2u || !relocation_artifact.relocation_summary ||
        strcmp(relocation_artifact.source_segment_artifact.segment_summary->name, ".cold") != 0 ||
        !machine_image_report_get_entry_symbol_artifact(&image_report, &index, &symbol_artifact) ||
        index != 1u || !symbol_artifact.symbol_summary ||
        strcmp(symbol_artifact.symbol_summary->name, "main") != 0 ||
        !symbol_artifact.is_entry_symbol ||
        !symbol_artifact.has_segment_artifact ||
        strcmp(symbol_artifact.segment_artifact.segment_summary->name, ".text") != 0 ||
        !machine_image_report_find_symbol_artifact_by_name(&image_report, "cold", &index, &symbol_artifact) ||
        index != 3u || !symbol_artifact.symbol_summary ||
        strcmp(symbol_artifact.symbol_summary->name, "cold") != 0 ||
        !symbol_artifact.has_segment_artifact ||
        strcmp(symbol_artifact.segment_artifact.segment_summary->name, ".cold") != 0 ||
        symbol_artifact.incoming_relocation_count != 0u ||
        !machine_image_report_find_symbol_artifact_by_virtual_address(
            &image_report,
            0x100au,
            &index,
            &symbol_artifact) ||
        index != 4u || !symbol_artifact.symbol_summary ||
        strcmp(symbol_artifact.symbol_summary->name, "F1.L1") != 0 ||
        symbol_artifact.incoming_relocation_count != 0u ||
        !machine_image_report_find_symbol_artifact_by_name(&image_report, "main", &index, &symbol_artifact) ||
        index != 1u || !symbol_artifact.symbol_summary ||
        strcmp(symbol_artifact.symbol_summary->name, "main") != 0 ||
        symbol_artifact.incoming_relocation_count != 1u ||
        symbol_artifact.incoming_resolved_relocation_count != 1u ||
        symbol_artifact.incoming_unresolved_relocation_count != 0u ||
        !machine_image_report_symbol_artifact_get_relocation_summary(
            &symbol_artifact,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 2u || relocation_summary->target_virtual_address != 0x1000u ||
        !machine_image_report_symbol_artifact_get_resolved_relocation_summary(
            &symbol_artifact,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 2u || strcmp(relocation_summary->symbol_name, "main") != 0 ||
        !machine_image_report_find_symbol_artifact_by_name(&image_report, "ext", &index, &symbol_artifact) ||
        index != 0u || !symbol_artifact.symbol_summary ||
        strcmp(symbol_artifact.symbol_summary->name, "ext") != 0 ||
        symbol_artifact.incoming_relocation_count != 1u ||
        symbol_artifact.incoming_resolved_relocation_count != 0u ||
        symbol_artifact.incoming_unresolved_relocation_count != 1u ||
        !machine_image_report_symbol_artifact_get_unresolved_relocation_summary(
            &symbol_artifact,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 1u || strcmp(relocation_summary->symbol_name, "ext") != 0 ||
        !machine_image_report_get_segment_artifact(&image_report, 0u, &artifact) ||
        artifact.segment_index != 0u || !artifact.segment_summary ||
        strcmp(artifact.segment_summary->name, ".text") != 0 ||
        artifact.symbol_count != 2u || artifact.relocation_count != 2u ||
        artifact.resolved_relocation_count != 1u || artifact.unresolved_relocation_count != 1u ||
        !machine_image_report_find_segment_artifact_by_name(&image_report, ".cold", &index, &artifact) ||
        index != 1u || !artifact.segment_summary || strcmp(artifact.segment_summary->name, ".cold") != 0 ||
        artifact.symbol_count != 2u || artifact.relocation_count != 1u ||
        artifact.resolved_relocation_count != 1u || artifact.unresolved_relocation_count != 0u ||
        !machine_image_report_find_segment_artifact_covering_virtual_address(&image_report, 0x1002u, &index, &artifact) ||
        index != 0u || !artifact.segment_summary || strcmp(artifact.segment_summary->name, ".text") != 0 ||
        !machine_image_report_segment_artifact_get_symbol_summary(&artifact, 1u, &index, &symbol_summary) ||
        !symbol_summary || index != 2u || strcmp(symbol_summary->name, "F0.L1") != 0 ||
        !machine_image_report_segment_artifact_get_relocation_summary(&artifact, 1u, &index, &relocation_summary) ||
        !relocation_summary || index != 1u || strcmp(relocation_summary->symbol_name, "ext") != 0 ||
        !machine_image_report_segment_artifact_get_resolved_relocation_summary(
            &artifact,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 0u || relocation_summary->target_virtual_address != 0x1004u ||
        !machine_image_report_segment_artifact_get_unresolved_relocation_summary(
            &artifact,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 1u || strcmp(relocation_summary->symbol_name, "ext") != 0 ||
        !machine_image_report_get_relocation_artifact(&image_report, 2u, &relocation_artifact) ||
        !relocation_artifact.relocation_summary ||
        relocation_artifact.relocation_index != 2u ||
        strcmp(relocation_artifact.source_segment_artifact.segment_summary->name, ".cold") != 0 ||
        !relocation_artifact.has_target_symbol_summary ||
        relocation_artifact.target_symbol_index != 1u ||
        strcmp(relocation_artifact.target_symbol_summary->name, "main") != 0 ||
        !relocation_artifact.has_target_segment_artifact ||
        strcmp(relocation_artifact.target_segment_artifact.segment_summary->name, ".text") != 0 ||
        !machine_image_report_find_relocation_artifact_by_site_virtual_address(
            &image_report,
            0x1003u,
            &index,
            &relocation_artifact) ||
        index != 1u || !relocation_artifact.relocation_summary ||
        strcmp(relocation_artifact.relocation_summary->symbol_name, "ext") != 0 ||
        !relocation_artifact.has_target_symbol_summary ||
        strcmp(relocation_artifact.target_symbol_summary->name, "ext") != 0 ||
        relocation_artifact.has_target_segment_artifact ||
        !machine_image_report_get_symbol_relocation_summary_count(&image_report, 1u, &count) || count != 1u ||
        !machine_image_report_get_symbol_resolved_relocation_summary_count(&image_report, 1u, &count) ||
        count != 1u ||
        !machine_image_report_get_symbol_unresolved_relocation_summary_count(&image_report, 0u, &count) ||
        count != 1u ||
        !machine_image_report_get_symbol_relocation_summary_by_index(
            &image_report,
            1u,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 2u || strcmp(relocation_summary->symbol_name, "main") != 0 ||
        !machine_image_report_get_symbol_resolved_relocation_summary_by_index(
            &image_report,
            1u,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 2u || relocation_summary->target_virtual_address != 0x1000u ||
        !machine_image_report_get_symbol_unresolved_relocation_summary_by_index(
            &image_report,
            0u,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 1u || strcmp(relocation_summary->symbol_name, "ext") != 0 ||
        !machine_image_report_get_resolved_relocation_artifact_by_index(
            &image_report,
            1u,
            &index,
            &relocation_artifact) ||
        index != 2u || !relocation_artifact.relocation_summary ||
        relocation_artifact.relocation_summary->target_virtual_address != 0x1000u ||
        !machine_image_report_get_unresolved_relocation_artifact_by_index(
            &image_report,
            0u,
            &index,
            &relocation_artifact) ||
        index != 1u || !relocation_artifact.relocation_summary ||
        strcmp(relocation_artifact.relocation_summary->symbol_name, "ext") != 0 ||
        !machine_image_report_get_segment_summary(&image_report, 1u, &segment_summary) ||
        !segment_summary || strcmp(segment_summary->name, ".cold") != 0 ||
        !machine_image_report_get_segment_symbol_summary_count(&image_report, 0u, &count) || count != 2u ||
        !machine_image_report_get_segment_symbol_summary_by_index(&image_report, 0u, 1u, &index, &symbol_summary) ||
        !symbol_summary || index != 2u || strcmp(symbol_summary->name, "F0.L1") != 0 ||
        !machine_image_report_get_segment_symbol_summary_by_index(&image_report, 1u, 0u, &index, &symbol_summary) ||
        !symbol_summary || index != 3u || strcmp(symbol_summary->name, "cold") != 0 ||
        !machine_image_report_get_segment_relocation_summary_count(&image_report, 0u, &count) || count != 2u ||
        !machine_image_report_get_segment_resolved_relocation_summary_count(&image_report, 0u, &count) ||
        count != 1u ||
        !machine_image_report_get_segment_unresolved_relocation_summary_count(&image_report, 0u, &count) ||
        count != 1u ||
        !machine_image_report_get_segment_resolved_relocation_summary_by_index(
            &image_report,
            1u,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 2u || relocation_summary->target_virtual_address != 0x1000u ||
        !machine_image_report_get_segment_unresolved_relocation_summary_by_index(
            &image_report,
            0u,
            0u,
            &index,
            &relocation_summary) ||
        !relocation_summary || index != 1u || strcmp(relocation_summary->symbol_name, "ext") != 0) {
        fprintf(stderr, "[machine-image] FAIL: segment-cached subset helper mismatch: %s\n", image_error.message);
        ok = 0;
    }

cleanup:
    machine_image_report_free(&image_report);
    machine_image_file_free(&image_file);
    return ok;
}

static int test_machine_image_rejects_wrapped_spans_and_addresses(void) {
    MachineImageFile image_file;
    MachineImageError image_error;
    const MachineImageSegment *segment = NULL;
    size_t segment_index = (size_t)-1;
    int ok = 1;

    machine_image_file_init(&image_file);
    memset(&image_error, 0, sizeof(image_error));

    if (!build_wrapped_machine_image_file(&image_file)) {
        return 0;
    }

    image_file.symbols[0].value_offset = 4u;
    image_file.symbols[0].virtual_address = 0u;
    if (machine_image_verify_file(&image_file, &image_error) ||
        strstr(image_error.message, "defined image symbol has invalid address") == NULL) {
        fprintf(stderr,
            "[machine-image] FAIL: wrapped symbol-address rejection mismatch: %s\n",
            image_error.message);
        ok = 0;
        goto cleanup;
    }

    image_file.symbols[0].value_offset = 0u;
    image_file.symbols[0].virtual_address = image_file.base_virtual_address;
    image_file.relocations[0].segment_offset = 4u;
    image_file.relocations[0].site_virtual_address = 0u;
    memset(&image_error, 0, sizeof(image_error));
    if (machine_image_verify_file(&image_file, &image_error) ||
        strstr(image_error.message, "invalid image relocation") == NULL) {
        fprintf(stderr,
            "[machine-image] FAIL: wrapped relocation-address rejection mismatch: %s\n",
            image_error.message);
        ok = 0;
        goto cleanup;
    }

    image_file.relocations[0].segment_offset = 1u;
    image_file.relocations[0].site_virtual_address = image_file.base_virtual_address + 1u;
    image_file.segments[0].image_offset = ((size_t)-1) - 3u;
    memset(&image_error, 0, sizeof(image_error));
    if (machine_image_verify_file(&image_file, &image_error) ||
        strstr(image_error.message, "invalid image segment") == NULL) {
        fprintf(stderr,
            "[machine-image] FAIL: wrapped image-offset rejection mismatch: %s\n",
            image_error.message);
        ok = 0;
        goto cleanup;
    }

    image_file.segments[0].image_offset = 0u;
    if (machine_image_file_find_segment_covering_virtual_address(
            &image_file,
            image_file.base_virtual_address,
            &segment_index,
            &segment) ||
        segment != NULL || segment_index != (size_t)-1) {
        fprintf(stderr, "[machine-image] FAIL: wrapped segment coverage query should reject wrapped span\n");
        ok = 0;
    }

cleanup:
    machine_image_file_free(&image_file);
    return ok;
}

static int test_machine_image_query_helpers_reject_malformed_tables(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineImageFile image_file;
    MachineImageReport image_report;
    MachineImageError image_error;
    MachineImageSegment *saved_file_segments = NULL;
    MachineImageSymbol *saved_file_symbols = NULL;
    MachineImageRelocation *saved_file_relocations = NULL;
    MachineImageSegment *saved_report_file_segments = NULL;
    MachineImageSymbol *saved_report_file_symbols = NULL;
    MachineImageRelocation *saved_report_file_relocations = NULL;
    MachineImageSegmentSummary *saved_segment_summaries = NULL;
    MachineImageSymbolSummary *saved_symbol_summaries = NULL;
    MachineImageRelocationSummary *saved_relocation_summaries = NULL;
    size_t *saved_defined_symbol_indices = NULL;
    size_t *saved_resolved_relocation_indices = NULL;
    const MachineImageSegment *segment = NULL;
    const MachineImageSymbol *symbol = NULL;
    const MachineImageRelocation *relocation = NULL;
    const MachineImageSegmentSummary *segment_summary = NULL;
    const MachineImageSymbolSummary *symbol_summary = NULL;
    const MachineImageRelocationSummary *relocation_summary = NULL;
    MachineImageReportOverviewArtifact overview_artifact;
    MachineImageReportSegmentArtifact segment_artifact;
    MachineImageReportSymbolArtifact symbol_artifact;
    MachineImageReportRelocationArtifact relocation_artifact;
    MachineImageHeaderSummary header_summary;
    const MachineImageFile *report_file = NULL;
    size_t count = 0u;
    size_t index = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&image_error, 0, sizeof(image_error));
    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&segment_artifact, 0, sizeof(segment_artifact));
    memset(&symbol_artifact, 0, sizeof(symbol_artifact));
    memset(&relocation_artifact, 0, sizeof(relocation_artifact));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_image_file_init(&image_file);
    machine_image_report_init(&image_report);

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
        fprintf(stderr, "[machine-image] FAIL: malformed-image setup failed: %s\n", machine_error.message);
        ok = 0;
        goto cleanup;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-image] FAIL: malformed-image setup failed: %s\n", machine_error.message);
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
        !machine_image_build_from_machine_ir_report(&machine_report, &image_file, &image_error) ||
        !machine_image_build_report_from_machine_ir_report(&machine_report, &image_report, &image_error)) {
        fprintf(stderr, "[machine-image] FAIL: malformed-image setup failed: %s\n", image_error.message);
        ok = 0;
        goto cleanup;
    }

    saved_file_segments = image_file.segments;
    saved_file_symbols = image_file.symbols;
    saved_file_relocations = image_file.relocations;
    image_file.segments = NULL;
    image_file.symbols = NULL;
    image_file.relocations = NULL;
    if (machine_image_file_get_summary(&image_file, &count, NULL, NULL, NULL) ||
        machine_image_file_get_header_summary(&image_file, &header_summary) ||
        machine_image_file_get_segment(&image_file, 0u, &segment) ||
        machine_image_file_find_segment_by_name(&image_file, ".text", &index, &segment) ||
        machine_image_file_find_segment_covering_virtual_address(&image_file, 0x1000u, &index, &segment) ||
        machine_image_file_get_symbol(&image_file, 0u, &symbol) ||
        machine_image_file_find_symbol_by_name(&image_file, "main", &index, &symbol) ||
        machine_image_file_find_symbol_by_virtual_address(&image_file, 0x1000u, &index, &symbol) ||
        machine_image_file_get_relocation(&image_file, 0u, &relocation) ||
        machine_image_file_find_relocation_by_site_virtual_address(&image_file, 0x1004u, &index, &relocation) ||
        machine_image_file_get_resolved_relocation_count(&image_file, &count) ||
        machine_image_file_get_unresolved_relocation_count(&image_file, &count)) {
        fprintf(stderr, "[machine-image] FAIL: malformed image-file query unexpectedly succeeded\n");
        ok = 0;
    }
    image_file.segments = saved_file_segments;
    image_file.symbols = saved_file_symbols;
    image_file.relocations = saved_file_relocations;

    saved_report_file_segments = image_report.file.segments;
    saved_report_file_symbols = image_report.file.symbols;
    saved_report_file_relocations = image_report.file.relocations;
    saved_segment_summaries = image_report.segment_summaries;
    saved_symbol_summaries = image_report.symbol_summaries;
    saved_relocation_summaries = image_report.relocation_summaries;
    saved_defined_symbol_indices = image_report.defined_symbol_indices;
    saved_resolved_relocation_indices = image_report.resolved_relocation_indices;
    image_report.file.segments = NULL;
    image_report.file.symbols = NULL;
    image_report.file.relocations = NULL;
    image_report.segment_summaries = NULL;
    image_report.symbol_summaries = NULL;
    image_report.relocation_summaries = NULL;
    image_report.defined_symbol_indices = NULL;
    image_report.resolved_relocation_indices = NULL;
    if (machine_image_report_get_summary(&image_report, &count, NULL, NULL, NULL) ||
        machine_image_report_get_overview_artifact(&image_report, &overview_artifact) ||
        machine_image_report_get_file(&image_report, &report_file) ||
        machine_image_report_get_segment_summary(&image_report, 0u, &segment_summary) ||
        machine_image_report_get_segment_artifact(&image_report, 0u, &segment_artifact) ||
        machine_image_report_find_segment_summary_by_name(&image_report, ".text", &index, &segment_summary) ||
        machine_image_report_get_symbol_summary(&image_report, 0u, &symbol_summary) ||
        machine_image_report_get_symbol_artifact(&image_report, 0u, &symbol_artifact) ||
        machine_image_report_find_symbol_summary_by_name(&image_report, "main", &index, &symbol_summary) ||
        machine_image_report_get_relocation_summary(&image_report, 0u, &relocation_summary) ||
        machine_image_report_get_relocation_artifact(&image_report, 0u, &relocation_artifact) ||
        machine_image_report_get_symbol_filter_count(&image_report, MACHINE_IMAGE_SYMBOL_FILTER_DEFINED, &count) ||
        machine_image_report_get_relocation_filter_count(
            &image_report, MACHINE_IMAGE_RELOCATION_FILTER_RESOLVED, &count) ||
        machine_image_report_get_resolved_relocation_count(&image_report, &count)) {
        fprintf(stderr, "[machine-image] FAIL: malformed image-report query unexpectedly succeeded\n");
        ok = 0;
    }
    image_report.file.segments = saved_report_file_segments;
    image_report.file.symbols = saved_report_file_symbols;
    image_report.file.relocations = saved_report_file_relocations;
    image_report.segment_summaries = saved_segment_summaries;
    image_report.symbol_summaries = saved_symbol_summaries;
    image_report.relocation_summaries = saved_relocation_summaries;
    image_report.defined_symbol_indices = saved_defined_symbol_indices;
    image_report.resolved_relocation_indices = saved_resolved_relocation_indices;

cleanup:
    image_report.file.segments = saved_report_file_segments ? saved_report_file_segments : image_report.file.segments;
    image_report.file.symbols = saved_report_file_symbols ? saved_report_file_symbols : image_report.file.symbols;
    image_report.file.relocations =
        saved_report_file_relocations ? saved_report_file_relocations : image_report.file.relocations;
    image_report.segment_summaries = saved_segment_summaries ? saved_segment_summaries : image_report.segment_summaries;
    image_report.symbol_summaries = saved_symbol_summaries ? saved_symbol_summaries : image_report.symbol_summaries;
    image_report.relocation_summaries =
        saved_relocation_summaries ? saved_relocation_summaries : image_report.relocation_summaries;
    image_report.defined_symbol_indices =
        saved_defined_symbol_indices ? saved_defined_symbol_indices : image_report.defined_symbol_indices;
    image_report.resolved_relocation_indices =
        saved_resolved_relocation_indices ? saved_resolved_relocation_indices : image_report.resolved_relocation_indices;
    image_file.segments = saved_file_segments ? saved_file_segments : image_file.segments;
    image_file.symbols = saved_file_symbols ? saved_file_symbols : image_file.symbols;
    image_file.relocations = saved_file_relocations ? saved_file_relocations : image_file.relocations;
    machine_image_report_free(&image_report);
    machine_image_file_free(&image_file);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_image_builds_from_machine_ir_report();
    ok &= test_machine_image_preserves_global_object_segments();
    ok &= test_machine_image_preserves_unresolved_external_call_site();
    ok &= test_machine_image_report_helpers();
    ok &= test_machine_image_build_from_elf_bytes_helpers();
    ok &= test_machine_image_clone_and_dump_helpers();
    ok &= test_machine_image_profile_and_ir_dump_helpers();
    ok &= test_machine_image_segment_cached_subset_helpers();
    ok &= test_machine_image_query_helpers_reject_malformed_tables();
    ok &= test_machine_image_rejects_wrapped_spans_and_addresses();

    if (!ok) {
        return 1;
    }

    printf("[machine-image] PASS\n");
    return 0;
}
