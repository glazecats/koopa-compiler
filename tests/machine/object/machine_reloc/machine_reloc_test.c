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

static int expect_report_dump(const MachineRelocReport *report, const char *expected_text) {
    char *actual_text = NULL;
    MachineRelocError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_reloc_dump_report(report, &actual_text, &error)) {
        fprintf(stderr, "[machine-reloc] FAIL: report dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-reloc] FAIL: report dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
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
    MachineRelocTargetPolicySummary target_policy_summary;
    MachineRelocRelocationFamilySummary relocation_family_summary;
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
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&relocation_family_summary, 0, sizeof(relocation_family_summary));
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
        !machine_reloc_file_get_target_policy_summary(&reloc_file, &target_policy_summary) ||
        !machine_reloc_file_get_relocation_family_summary(&reloc_file, &relocation_family_summary) ||
        target_policy_summary.target_profile != MACHINE_BYTES_TARGET_PROFILE_GENERIC ||
        target_policy_summary.bytes_policy.target_profile != MACHINE_BYTES_TARGET_PROFILE_GENERIC ||
        target_policy_summary.bytes_policy.max_logical_machine_register_count != 0u ||
        target_policy_summary.bytes_policy.preserves_known_internal_pc_relative_targets ||
        target_policy_summary.bytes_policy.preserves_direct_fallthrough_honesty ||
        target_policy_summary.bytes_policy.uses_paired_global_data_addressing ||
        target_policy_summary.bytes_policy.supports_rv32m_alu_ops ||
        target_policy_summary.preserves_preview_pc_relative_addends ||
        target_policy_summary.preserves_direct_fallthrough_honesty ||
        relocation_family_summary.call_relocation_count != 0u ||
        relocation_family_summary.primary_control_relocation_count != 1u ||
        relocation_family_summary.secondary_control_relocation_count != 1u ||
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
        "machine_reloc profile=generic total_bytes=9 object_sections=1 symbols=4 relocation_sections=1 relocations=2\n"
        "policy: addends=zero-or-generic fallthrough=not-guaranteed\n"
        "reloc-sections:\n"
        "  relsec.0 .text span=0..9 bytes=9 relocations=2\n"
        "relocations:\n"
        "  reloc.0 ctrl-primary sec=0 patch@4 bytes=1 target=F0.L2 sym=yes tgt_sec=yes tgt_off=yes addend=0\n"
        "  reloc.1 ctrl-secondary sec=0 patch@4 bytes=1 target=F0.L1 sym=yes tgt_sec=yes tgt_off=yes addend=0\n");

    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

static int test_machine_reloc_report_surface(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineRelocReport report;
    MachineRelocError reloc_error;
    const MachineRelocFile *file_view = NULL;
    const MachineRelocTargetPolicySummary *target_policy_summary = NULL;
    const MachineRelocRelocationFamilySummary *relocation_family_summary = NULL;
    const MachineRelocSectionSummary *section_summary = NULL;
    const MachineRelocationSummary *section_relocation_summaries = NULL;
    const MachineRelocationSummary *relocation_summary = NULL;
    size_t total_byte_count = 0u;
    size_t object_section_count = 0u;
    size_t symbol_count = 0u;
    size_t relocation_section_count = 0u;
    size_t relocation_count = 0u;
    size_t section_relocation_count = 0u;
    size_t section_index = (size_t)-1;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_reloc_report_init(&report);

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
        fprintf(stderr, "[machine-reloc] FAIL: report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_reloc_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-reloc] FAIL: report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_reloc_report_free(&report);
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
        fprintf(stderr, "[machine-reloc] FAIL: report setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_reloc_report_free(&report);
        return 0;
    }

    if (!machine_reloc_build_report_from_machine_ir_report(&machine_report, &report, &reloc_error) ||
        !machine_reloc_report_get_summary(
            &report,
            &total_byte_count,
            &object_section_count,
            &symbol_count,
            &relocation_section_count,
            &relocation_count) ||
        total_byte_count != 9u || object_section_count != 1u || symbol_count != 4u ||
        relocation_section_count != 1u || relocation_count != 2u ||
        !machine_reloc_report_get_file(&report, &file_view) || !file_view ||
        file_view->relocation_count != 2u ||
        !machine_reloc_report_get_target_policy_summary_artifact(&report, &target_policy_summary) ||
        !target_policy_summary ||
        target_policy_summary->target_profile != MACHINE_BYTES_TARGET_PROFILE_GENERIC ||
        target_policy_summary->preserves_preview_pc_relative_addends ||
        target_policy_summary->preserves_direct_fallthrough_honesty ||
        !machine_reloc_report_get_relocation_family_summary_artifact(&report, &relocation_family_summary) ||
        !relocation_family_summary ||
        relocation_family_summary->call_relocation_count != 0u ||
        relocation_family_summary->primary_control_relocation_count != 1u ||
        relocation_family_summary->secondary_control_relocation_count != 1u ||
        relocation_family_summary->data_address_relocation_count != 0u ||
        relocation_family_summary->data_load_relocation_count != 0u ||
        relocation_family_summary->data_store_relocation_count != 0u ||
        !machine_reloc_report_find_section_summary_by_name(&report, ".text", &section_index, &section_summary) ||
        section_index != 0u || !section_summary || section_summary->relocation_count != 2u ||
        !machine_reloc_report_get_section_relocation_summaries(
            &report, section_index, &section_relocation_count, &section_relocation_summaries) ||
        section_relocation_count != 2u || !section_relocation_summaries ||
        section_relocation_summaries[0].kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        !machine_reloc_report_get_relocation_summary(&report, 0u, &relocation_summary) ||
        !relocation_summary ||
        relocation_summary->kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        relocation_summary->patch_byte_offset != 4u ||
        relocation_summary->addend != 0) {
        fprintf(stderr, "[machine-reloc] FAIL: report surface mismatch: %s\n", reloc_error.message);
        ok = 0;
    }

    ok &= expect_report_dump(
        &report,
        "machine_reloc-report total_bytes=9 object_sections=1 symbols=4 relocation_sections=1 relocations=2\n"
        "target_policy profile=generic preview_addends=0 fallthrough=0\n"
        "relocation_families: call=0 primary=1 secondary=1 data_addr=0 data_load=0 data_store=0\n"
        "section_summaries:\n"
        "  relsec.0 .text span=0..9 bytes=9 relocations=2\n"
        "relocation_summaries:\n"
        "  reloc.0 ctrl-primary sec=0 patch@4 bytes=1 target=F0.L2 addend=0\n"
        "  reloc.1 ctrl-secondary sec=0 patch@4 bytes=1 target=F0.L1 addend=0\n"
        "\n"
        "machine_reloc profile=generic total_bytes=9 object_sections=1 symbols=4 relocation_sections=1 relocations=2\n"
        "policy: addends=zero-or-generic fallthrough=not-guaranteed\n"
        "reloc-sections:\n"
        "  relsec.0 .text span=0..9 bytes=9 relocations=2\n"
        "relocations:\n"
        "  reloc.0 ctrl-primary sec=0 patch@4 bytes=1 target=F0.L2 sym=yes tgt_sec=yes tgt_off=yes addend=0\n"
        "  reloc.1 ctrl-secondary sec=0 patch@4 bytes=1 target=F0.L1 sym=yes tgt_sec=yes tgt_off=yes addend=0\n");

    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_reloc_report_free(&report);
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

static int test_machine_reloc_preview_fallthrough_control_uses_single_real_relocation(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    MachineRelocTargetPolicySummary target_policy_summary;
    MachineRelocRelocationFamilySummary relocation_family_summary;
    const MachineObjectFile *embedded_object_file = NULL;
    const MachineRelocSection *section = NULL;
    const MachineRelocation *relocation = NULL;
    size_t total_byte_count = 0u;
    size_t object_section_count = 0u;
    size_t symbol_count = 0u;
    size_t relocation_section_count = 0u;
    size_t relocation_count = 0u;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&relocation_family_summary, 0, sizeof(relocation_family_summary));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);

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
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
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
        !machine_object_build_from_machine_bytes_program(&bytes_program, &object_file, &object_error) ||
        !machine_reloc_build_from_machine_object_file(&object_file, &reloc_file, &reloc_error) ||
        !machine_reloc_file_get_summary(
            &reloc_file,
            &total_byte_count,
            &object_section_count,
            &symbol_count,
            &relocation_section_count,
            &relocation_count) ||
        total_byte_count != 12u || object_section_count != 1u || symbol_count != 5u ||
        relocation_section_count != 1u || relocation_count != 1u ||
        !machine_reloc_file_get_object_file(&reloc_file, &embedded_object_file) || !embedded_object_file ||
        embedded_object_file->target_profile != MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW ||
        !machine_reloc_file_get_target_policy_summary(&reloc_file, &target_policy_summary) ||
        !machine_reloc_file_get_relocation_family_summary(&reloc_file, &relocation_family_summary) ||
        target_policy_summary.target_profile != MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW ||
        target_policy_summary.bytes_policy.target_profile != MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW ||
        target_policy_summary.bytes_policy.max_logical_machine_register_count != 8u ||
        !target_policy_summary.bytes_policy.preserves_known_internal_pc_relative_targets ||
        !target_policy_summary.bytes_policy.preserves_direct_fallthrough_honesty ||
        !target_policy_summary.bytes_policy.uses_paired_global_data_addressing ||
        !target_policy_summary.bytes_policy.supports_rv32m_alu_ops ||
        !target_policy_summary.preserves_preview_pc_relative_addends ||
        !target_policy_summary.preserves_direct_fallthrough_honesty ||
        relocation_family_summary.call_relocation_count != 0u ||
        relocation_family_summary.primary_control_relocation_count != 1u ||
        relocation_family_summary.secondary_control_relocation_count != 0u ||
        embedded_object_file->fixup_count != 1u ||
        !machine_reloc_file_find_section_by_name(&reloc_file, ".text", NULL, &section) || !section ||
        section->relocation_count != 1u ||
        !machine_reloc_file_get_relocation(&reloc_file, 0u, &relocation) || !relocation ||
        relocation->kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        relocation->patch_byte_offset != 0u ||
        relocation->patch_byte_count != 4u ||
        !relocation->has_target_symbol_index || relocation->target_symbol_index != 4u ||
        !relocation->has_target_section_index || relocation->target_section_index != 0u ||
        !relocation->has_target_byte_offset || relocation->target_byte_offset != 8u ||
        relocation->addend != 8) {
        fprintf(stderr, "[machine-reloc] FAIL: preview fallthrough reloc mismatch: %s\n", reloc_error.message);
        ok = 0;
    }

    ok &= expect_dump(
        &reloc_file,
        "machine_reloc profile=riscv32-preview total_bytes=12 object_sections=1 symbols=5 relocation_sections=1 relocations=1\n"
        "policy: addends=pc-relative-preview fallthrough=direct-preview-aware\n"
        "reloc-sections:\n"
        "  relsec.0 .text span=0..12 bytes=12 relocations=1\n"
        "relocations:\n"
        "  reloc.0 ctrl-primary sec=0 patch@0 bytes=4 target=F0.L3 sym=yes tgt_sec=yes tgt_off=yes addend=8\n");

    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

static int test_machine_reloc_preview_internal_call_uses_pc_relative_addend(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    MachineRelocTargetPolicySummary target_policy_summary;
    MachineRelocRelocationFamilySummary relocation_family_summary;
    const MachineRelocation *relocation = NULL;
    const MachineObjectFile *embedded_object_file = NULL;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&relocation_family_summary, 0, sizeof(relocation_family_summary));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);

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
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1;
    emit_program.functions[0].blocks[0].op_capacity = 1;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1, sizeof(MachineEmitOp));
    emit_program.functions[1].blocks[0].emit_index = 0;
    emit_program.functions[1].blocks[0].original_layout_index = 0;
    emit_program.functions[1].blocks[0].original_block_id = 0;
    emit_program.functions[1].blocks[0].label_name = dup_text("F1.L0");
    if (!emit_program.functions[0].blocks[0].label_name ||
        !emit_program.functions[0].blocks[0].ops ||
        !emit_program.functions[1].blocks[0].label_name) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
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
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_object_build_from_machine_bytes_program(&bytes_program, &object_file, &object_error) ||
        !machine_reloc_build_from_machine_object_file(&object_file, &reloc_file, &reloc_error) ||
        !machine_reloc_file_get_object_file(&reloc_file, &embedded_object_file) || !embedded_object_file ||
        embedded_object_file->target_profile != MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW ||
        !machine_reloc_file_get_target_policy_summary(&reloc_file, &target_policy_summary) ||
        !machine_reloc_file_get_relocation_family_summary(&reloc_file, &relocation_family_summary) ||
        target_policy_summary.target_profile != MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW ||
        target_policy_summary.bytes_policy.target_profile != MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW ||
        target_policy_summary.bytes_policy.max_logical_machine_register_count != 8u ||
        !target_policy_summary.bytes_policy.preserves_known_internal_pc_relative_targets ||
        !target_policy_summary.bytes_policy.preserves_direct_fallthrough_honesty ||
        !target_policy_summary.bytes_policy.uses_paired_global_data_addressing ||
        !target_policy_summary.bytes_policy.supports_rv32m_alu_ops ||
        !target_policy_summary.preserves_preview_pc_relative_addends ||
        !target_policy_summary.preserves_direct_fallthrough_honesty ||
        relocation_family_summary.call_relocation_count != 1u ||
        relocation_family_summary.primary_control_relocation_count != 0u ||
        relocation_family_summary.secondary_control_relocation_count != 0u ||
        !machine_reloc_file_get_relocation(&reloc_file, 0, &relocation) || !relocation ||
        relocation->kind != MACHINE_BYTES_FIXUP_CALL_TARGET ||
        !relocation->has_target_byte_offset ||
        relocation->target_byte_offset != 8u ||
        relocation->patch_byte_offset != 0u ||
        relocation->addend != 8) {
        fprintf(stderr, "[machine-reloc] FAIL: preview internal-call addend mismatch: %s\n", reloc_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

static int test_machine_reloc_dump_uses_explicit_i386_preview_profile_name(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineRelocReport report;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);
    machine_reloc_report_init(&report);

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
        machine_reloc_report_free(&report);
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
        machine_reloc_report_free(&report);
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
        !machine_reloc_dump_file(&reloc_file, &dump_text, &reloc_error) ||
        !dump_text ||
        strstr(dump_text, "machine_reloc profile=i386-preview ") == NULL ||
        !machine_reloc_build_report_from_file(&reloc_file, &report, &reloc_error) ||
        !machine_reloc_dump_report(&report, &report_dump_text, &reloc_error) ||
        !report_dump_text ||
        strstr(report_dump_text, "target_policy profile=i386-preview ") == NULL) {
        fprintf(stderr, "[machine-reloc] FAIL: i386 preview profile-name dump mismatch: %s\n", reloc_error.message);
        ok = 0;
    }

    free(dump_text);
    free(report_dump_text);
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    machine_reloc_report_free(&report);
    return ok;
}

static int test_machine_reloc_preview_verifier_rejects_addend_drift(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);

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
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_object_build_from_machine_bytes_program(&bytes_program, &object_file, &object_error) ||
        !machine_reloc_build_from_machine_object_file(&object_file, &reloc_file, &reloc_error)) {
        fprintf(stderr, "[machine-reloc] FAIL: preview addend-drift setup failed: %s\n", reloc_error.message);
        ok = 0;
        goto cleanup;
    }

    reloc_file.relocations[0].addend -= 4;
    if (machine_reloc_verify_file(&reloc_file, &reloc_error) ||
        strstr(reloc_error.message, "MACHINE-RELOC-127") == NULL) {
        fprintf(stderr, "[machine-reloc] FAIL: preview addend drift was not rejected: %s\n", reloc_error.message);
        ok = 0;
    }

cleanup:
    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

static int test_machine_reloc_preserves_global_object_sections_without_fixups(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    const MachineRelocSection *section = NULL;
    size_t total_byte_count = 0u;
    size_t object_section_count = 0u;
    size_t symbol_count = 0u;
    size_t relocation_section_count = 0u;
    size_t relocation_count = 0u;
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
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
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
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
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
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
        return 0;
    }

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_bytes_lower_program_from_machine_encode(&encode_program, &bytes_program, &bytes_error) ||
        !machine_object_build_from_machine_bytes_program(&bytes_program, &object_file, &object_error) ||
        !machine_reloc_build_from_machine_object_file(&object_file, &reloc_file, &reloc_error) ||
        !machine_reloc_file_get_summary(
            &reloc_file, &total_byte_count, &object_section_count, &symbol_count, &relocation_section_count, &relocation_count) ||
        total_byte_count == 0u || object_section_count != 3u || symbol_count != 4u ||
        relocation_section_count != 3u || relocation_count != 0u ||
        !machine_reloc_file_get_section(&reloc_file, 1u, &section) || !section ||
        strcmp(section->name, ".sbss") != 0 || section->byte_count != 4u || section->relocation_count != 0u ||
        !machine_reloc_file_get_section(&reloc_file, 2u, &section) || !section ||
        strcmp(section->name, ".sdata") != 0 || section->byte_count != 4u || section->relocation_count != 0u) {
        fprintf(stderr, "[machine-reloc] FAIL: global section relocation mismatch: %s\n", reloc_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

static int test_machine_reloc_surfaces_global_data_relocations(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    const MachineRelocation *relocation = NULL;
    MachineRelocRelocationFamilySummary relocation_family_summary;
    int ok = 1;

    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    memset(&relocation_family_summary, 0, sizeof(relocation_family_summary));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);

    emit_program.global_count = 2u;
    emit_program.global_capacity = 2u;
    emit_program.globals = (MachineEmitGlobal *)calloc(2u, sizeof(MachineEmitGlobal));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.globals || !emit_program.functions) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
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
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
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
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
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
        !machine_object_build_from_machine_bytes_program(&bytes_program, &object_file, &object_error) ||
        !machine_reloc_build_from_machine_object_file(&object_file, &reloc_file, &reloc_error) ||
        !machine_reloc_file_get_relocation_family_summary(&reloc_file, &relocation_family_summary) ||
        relocation_family_summary.call_relocation_count != 0u ||
        relocation_family_summary.primary_control_relocation_count != 0u ||
        relocation_family_summary.secondary_control_relocation_count != 0u ||
        relocation_family_summary.data_address_relocation_count != 2u ||
        relocation_family_summary.data_load_relocation_count != 1u ||
        relocation_family_summary.data_store_relocation_count != 1u ||
        !machine_reloc_file_get_relocation(&reloc_file, 0u, &relocation) || !relocation ||
        relocation->kind != MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET ||
        !relocation->has_target_section_index || relocation->target_section_index != 1u ||
        !relocation->has_target_byte_offset || relocation->target_byte_offset != 0u ||
        relocation->addend != 0 ||
        !machine_reloc_file_get_relocation(&reloc_file, 1u, &relocation) || !relocation ||
        relocation->kind != MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET ||
        !relocation->has_target_section_index || relocation->target_section_index != 1u ||
        !relocation->has_target_byte_offset || relocation->target_byte_offset != 0u ||
        relocation->addend != 0 ||
        !machine_reloc_file_get_relocation(&reloc_file, 2u, &relocation) || !relocation ||
        relocation->kind != MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET ||
        !relocation->has_target_section_index || relocation->target_section_index != 2u ||
        !relocation->has_target_byte_offset || relocation->target_byte_offset != 0u ||
        relocation->addend != 0 ||
        !machine_reloc_file_get_relocation(&reloc_file, 3u, &relocation) || !relocation ||
        relocation->kind != MACHINE_BYTES_FIXUP_DATA_STORE_TARGET ||
        !relocation->has_target_section_index || relocation->target_section_index != 2u ||
        !relocation->has_target_byte_offset || relocation->target_byte_offset != 0u ||
        relocation->addend != 0) {
        fprintf(stderr, "[machine-reloc] FAIL: global data relocation mismatch: %s\n", reloc_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

static int test_machine_reloc_preserves_global_object_byte_sizes(void) {
    MachineEmitProgram emit_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    const MachineRelocSection *section = NULL;
    const MachineObjectSymbol *symbol = NULL;
    int ok = 1;
    size_t total_byte_count = 0u;
    size_t object_section_count = 0u;
    size_t symbol_count = 0u;
    size_t relocation_section_count = 0u;
    size_t relocation_count = 0u;

    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    machine_emit_program_init(&emit_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);

    emit_program.global_count = 2u;
    emit_program.global_capacity = 2u;
    emit_program.globals = (MachineEmitGlobal *)calloc(2u, sizeof(MachineEmitGlobal));
    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.globals || !emit_program.functions) {
        machine_emit_program_free(&emit_program);
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
        return 0;
    }

    emit_program.globals[0].id = 0u;
    emit_program.globals[0].name = dup_text("g");
    emit_program.globals[0].byte_size = 4u;
    emit_program.globals[1].id = 1u;
    emit_program.globals[1].name = dup_text("h");
    emit_program.globals[1].byte_size = 8u;
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
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
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
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
        return 0;
    }

    if (!machine_bytes_lower_program_from_machine_emit_with_profile(
            &emit_program,
            MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
            &bytes_program,
            &bytes_error) ||
        !machine_object_build_from_machine_bytes_program(&bytes_program, &object_file, &object_error) ||
        !machine_reloc_build_from_machine_object_file(&object_file, &reloc_file, &reloc_error) ||
        !machine_reloc_file_get_summary(
            &reloc_file, &total_byte_count, &object_section_count, &symbol_count, &relocation_section_count, &relocation_count) ||
        total_byte_count == 0u || object_section_count != 3u || symbol_count != 4u ||
        relocation_section_count != 3u || relocation_count != 0u ||
        !machine_reloc_file_get_section(&reloc_file, 1u, &section) || !section ||
        strcmp(section->name, ".sbss") != 0 || section->byte_count != 4u || section->relocation_count != 0u ||
        !machine_reloc_file_get_section(&reloc_file, 2u, &section) || !section ||
        strcmp(section->name, ".sdata") != 0 || section->byte_count != 8u || section->relocation_count != 0u ||
        !machine_object_file_find_symbol_by_name(&reloc_file.object_file, "g", NULL, &symbol) || !symbol ||
        symbol->byte_count != 4u ||
        !machine_object_file_find_symbol_by_name(&reloc_file.object_file, "h", NULL, &symbol) || !symbol ||
        symbol->byte_count != 8u) {
        fprintf(stderr, "[machine-reloc] FAIL: global byte-size relocation mismatch: %s\n", reloc_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_reloc_builds_from_machine_ir_report();
    ok &= test_machine_reloc_report_surface();
    ok &= test_machine_reloc_builds_from_machine_object_with_external_call();
    ok &= test_machine_reloc_preview_fallthrough_control_uses_single_real_relocation();
    ok &= test_machine_reloc_preview_internal_call_uses_pc_relative_addend();
    ok &= test_machine_reloc_dump_uses_explicit_i386_preview_profile_name();
    ok &= test_machine_reloc_preview_verifier_rejects_addend_drift();
    ok &= test_machine_reloc_preserves_global_object_sections_without_fixups();
    ok &= test_machine_reloc_preserves_global_object_byte_sizes();
    ok &= test_machine_reloc_surfaces_global_data_relocations();

    if (!ok) {
        return 1;
    }

    printf("[machine-reloc] PASS\n");
    return 0;
}
