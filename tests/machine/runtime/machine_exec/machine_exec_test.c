#include "machine/exec.h"

#include "machine/bytes.h"
#include "machine/container.h"
#include "machine/emit.h"
#include "machine/encode.h"
#include "machine/ir.h"
#include "machine/object.h"
#include "machine/reloc.h"

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *EXPECTED_EXEC_DUMP =
    "machine_exec profile=generic-elf32 base=0x1000 entry=0x1000 segments=1 bytes=9\n"
    "entry_segment: xseg.0 .text perms=r-x\n"
    "segments:\n"
    "  xseg.0 .text img-seg=0 vaddr=0x1000 bytes=9 perms=r-x\n";

static const char *EXPECTED_EXEC_REPORT_DUMP =
    "machine_exec profile=generic-elf32 base=0x1000 entry=0x1000 segments=1 bytes=9\n"
    "entry_segment: xseg.0 .text perms=r-x\n"
    "segments:\n"
    "  xseg.0 .text img-seg=0 vaddr=0x1000 bytes=9 perms=r-x\n"
    "report_overview:\n"
    "  segments=1 bytes=9 executable_segments=1 non_executable_segments=0 entry=0x1000 entry_segment=0\n"
    "report_segment_filters:\n"
    "  executable[1] xseg.0(.text)\n"
    "  non-executable[0]\n"
    "  entry[1] xseg.0(.text)\n"
    "report_executable_segments:\n"
    "  xseg.0 .text perms=r-x\n"
    "report_segments:\n"
    "  xseg.0 .text entry=yes executable=yes perms=r-x\n";

static int build_expected_exec_dump_text(char *buffer, size_t buffer_size,
    const char *profile_name, size_t base_virtual_address) {
    if (!buffer || buffer_size == 0u || !profile_name) {
        return 0;
    }
    return snprintf(buffer, buffer_size,
               "machine_exec profile=%s base=0x%zx entry=0x%zx segments=1 bytes=9\n"
               "entry_segment: xseg.0 .text perms=r-x\n"
               "segments:\n"
               "  xseg.0 .text img-seg=0 vaddr=0x%zx bytes=9 perms=r-x\n",
               profile_name,
               base_virtual_address,
               base_virtual_address,
               base_virtual_address) > 0;
}

static int build_expected_exec_report_dump_text(char *buffer, size_t buffer_size,
    const char *profile_name, size_t base_virtual_address) {
    if (!buffer || buffer_size == 0u || !profile_name) {
        return 0;
    }
    return snprintf(buffer, buffer_size,
               "machine_exec profile=%s base=0x%zx entry=0x%zx segments=1 bytes=9\n"
               "entry_segment: xseg.0 .text perms=r-x\n"
               "segments:\n"
               "  xseg.0 .text img-seg=0 vaddr=0x%zx bytes=9 perms=r-x\n"
               "report_overview:\n"
               "  segments=1 bytes=9 executable_segments=1 non_executable_segments=0 entry=0x%zx entry_segment=0\n"
               "report_segment_filters:\n"
               "  executable[1] xseg.0(.text)\n"
               "  non-executable[0]\n"
               "  entry[1] xseg.0(.text)\n"
               "report_executable_segments:\n"
               "  xseg.0 .text perms=r-x\n"
               "report_segments:\n"
               "  xseg.0 .text entry=yes executable=yes perms=r-x\n",
               profile_name,
               base_virtual_address,
               base_virtual_address,
               base_virtual_address,
               base_virtual_address) > 0;
}

static char *dup_text(const char *text);

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
    image_file->symbols[0].value_offset = 4u;
    image_file->symbols[0].virtual_address = 0u;

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

static int verify_exec_file_with_profile(const MachineExecFile *exec_file,
    const char *context,
    MachineElfTargetProfile profile,
    size_t base_virtual_address);
static int verify_exec_report_with_profile(const MachineExecReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    size_t base_virtual_address);

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

static int expect_text(const char *label, const char *actual_text, const char *expected_text) {
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[machine-exec] FAIL: %s mismatch\nactual:\n%s\n",
            label,
            actual_text ? actual_text : "<null>");
        return 0;
    }
    return 1;
}

static int build_resolved_machine_elf_artifacts(const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineElfFile *out_elf_file,
    MachineElfReport *out_elf_report,
    unsigned char **out_elf_bytes,
    size_t *out_elf_byte_count,
    MachineElfError *error) {
    if (!report || !out_elf_file || !out_elf_report || !out_elf_bytes || !out_elf_byte_count || !error) {
        return 0;
    }
    *out_elf_bytes = NULL;
    *out_elf_byte_count = 0u;
    machine_elf_file_init(out_elf_file);
    machine_elf_report_init(out_elf_report);
    memset(error, 0, sizeof(*error));
    return machine_elf_build_from_machine_ir_report_with_profile(report, profile, out_elf_file, error) &&
        machine_elf_build_report_from_machine_ir_report_with_profile(report, profile, out_elf_report, error) &&
        machine_elf_build_bytes_from_machine_ir_report_with_profile(
            report, profile, out_elf_bytes, out_elf_byte_count, error);
}

static int build_resolved_machine_ir_report(
    MachineIrAllocateRewriteReport *report,
    MachineIrError *error) {
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;

    if (!report || !error) {
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(report);
    memset(error, 0, sizeof(*error));

    report->program.register_bank.register_count = 1u;
    report->program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1u, sizeof(MachineIrRegisterDesc));
    if (!report->program.register_bank.registers) {
        return 0;
    }
    report->program.register_bank.registers[0].register_id = 0u;
    report->program.register_bank.registers[0].name = dup_text("r0");
    report->program.register_bank.registers[0].allocatable = 1u;
    if (!report->program.register_bank.registers[0].name) {
        return 0;
    }

    if (!machine_ir_program_append_function(&report->program, "main", 1, &function, error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, error) ||
        !machine_ir_function_append_block(function, NULL, NULL, error) ||
        !machine_ir_function_append_block(function, NULL, NULL, error) ||
        !machine_ir_function_append_block(function, NULL, NULL, error)) {
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error)) {
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), error)) {
        return 0;
    }

    return 1;
}

static int build_global_object_machine_ir_report(
    MachineIrAllocateRewriteReport *report,
    MachineIrError *error) {
    MachineIrGlobal *global = NULL;

    if (!report || !error) {
        return 0;
    }

    if (!build_resolved_machine_ir_report(report, error)) {
        return 0;
    }
    if (!machine_ir_program_append_global(&report->program, "g", &global, error) ||
        !global ||
        !machine_ir_program_append_global(&report->program, "h", &global, error)) {
        machine_ir_allocate_rewrite_report_free(report);
        return 0;
    }
    report->program.globals[1].has_initializer = 1;
    report->program.globals[1].initializer_value = 7;

    return 1;
}

static int build_unresolved_machine_image_file(MachineImageFile *image_file) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineContainerFile container_file;
    MachineElfFile elf_file;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    MachineContainerError container_error;
    MachineElfError elf_error;
    MachineImageError image_error;
    int ok = 1;

    if (!image_file) {
        return 0;
    }

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    memset(&container_error, 0, sizeof(container_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&image_error, 0, sizeof(image_error));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);
    machine_container_file_init(&container_file);
    machine_elf_file_init(&elf_file);

    emit_program.function_count = 1u;
    emit_program.function_capacity = 1u;
    emit_program.functions = (MachineEmitFunction *)calloc(1u, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        ok = 0;
        goto cleanup;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1u;
    emit_program.functions[0].block_capacity = 1u;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1u, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        ok = 0;
        goto cleanup;
    }
    emit_program.functions[0].blocks[0].emit_index = 0u;
    emit_program.functions[0].blocks[0].original_layout_index = 0u;
    emit_program.functions[0].blocks[0].original_block_id = 0u;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1u;
    emit_program.functions[0].blocks[0].op_capacity = 1u;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1u, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name ||
        !emit_program.functions[0].blocks[0].ops) {
        ok = 0;
        goto cleanup;
    }
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_VOID_IMM;
    emit_program.functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("sink");
    emit_program.functions[0].blocks[0].ops[0].as.call.arg_count = 1u;
    emit_program.functions[0].blocks[0].ops[0].as.call.args =
        (MachineEmitOperand *)calloc(1u, sizeof(MachineEmitOperand));
    if (!emit_program.functions[0].blocks[0].ops[0].as.call.callee_name ||
        !emit_program.functions[0].blocks[0].ops[0].as.call.args) {
        ok = 0;
        goto cleanup;
    }
    emit_program.functions[0].blocks[0].ops[0].as.call.args[0] =
        machine_select_operand_immediate(5);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value =
        machine_select_operand_immediate(0);

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
        !machine_image_build_from_machine_elf_file(&elf_file, image_file, &image_error)) {
        fprintf(stderr,
            "[machine-exec] FAIL: unresolved image setup failed: %s%s%s%s%s%s%s\n",
            encode_error.message,
            bytes_error.message,
            object_error.message,
            reloc_error.message,
            container_error.message,
            elf_error.message,
            image_error.message);
        ok = 0;
    }

cleanup:
    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    machine_container_file_free(&container_file);
    machine_elf_file_free(&elf_file);
    return ok;
}

static int verify_exec_file(const MachineExecFile *exec_file, const char *context) {
    return verify_exec_file_with_profile(
        exec_file, context, MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32, 0x1000u);
}

static int verify_exec_file_with_profile(const MachineExecFile *exec_file,
    const char *context,
    MachineElfTargetProfile profile,
    size_t base_virtual_address) {
    MachineExecHeaderSummary header_summary;
    MachineExecSegmentSummary segment_summary;
    MachineExecError exec_error;
    const MachineExecSegment *segment = NULL;
    const MachineExecSegment *entry_segment = NULL;
    size_t segment_count = 0u;
    size_t byte_count = 0u;
    size_t executable_segment_count = 0u;
    size_t segment_index = 0u;
    size_t entry_segment_index = 0u;
    char *dump_text = NULL;
    char expected_dump[512];
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&segment_summary, 0, sizeof(segment_summary));
    memset(&exec_error, 0, sizeof(exec_error));
    memset(expected_dump, 0, sizeof(expected_dump));
    if (!build_expected_exec_dump_text(
            expected_dump,
            sizeof(expected_dump),
            machine_elf_target_profile_name(profile),
            base_virtual_address)) {
        return 0;
    }

    if (!machine_exec_verify_file(exec_file, &exec_error) ||
        !machine_exec_file_get_summary(
            exec_file,
            &segment_count,
            &byte_count,
            &executable_segment_count) ||
        segment_count != 1u || byte_count != 9u || executable_segment_count != 1u ||
        !machine_exec_file_get_header_summary(exec_file, &header_summary) ||
        header_summary.target_profile != profile ||
        header_summary.base_virtual_address != base_virtual_address ||
        header_summary.entry_virtual_address != base_virtual_address ||
        header_summary.segment_count != 1u ||
        header_summary.byte_count != 9u ||
        !machine_exec_file_find_segment_covering_virtual_address(
            exec_file, base_virtual_address + 4u, &segment_index, &segment) ||
        !segment || segment_index != 0u ||
        !machine_exec_file_find_segment_by_name(exec_file, ".text", &segment_index, &segment) ||
        !segment || segment_index != 0u ||
        !machine_exec_file_get_entry_segment(exec_file, &entry_segment_index, &entry_segment) ||
        !entry_segment || entry_segment_index != 0u ||
        !machine_exec_file_get_executable_segment_count(exec_file, &executable_segment_count) ||
        executable_segment_count != 1u ||
        !machine_exec_file_get_executable_segment_by_index(exec_file, 0u, &segment_index, &segment) ||
        !segment || segment_index != 0u ||
        !machine_exec_segment_get_summary(segment, &segment_summary) ||
        !segment_summary.name ||
        strcmp(segment_summary.name, ".text") != 0 ||
        segment_summary.image_segment_index != 0u ||
        segment_summary.virtual_address != base_virtual_address ||
        segment_summary.byte_count != 9u ||
        !segment_summary.readable ||
        segment_summary.writable ||
        !segment_summary.executable ||
        !machine_exec_dump_file(exec_file, &dump_text, &exec_error)) {
        fprintf(stderr,
            "[machine-exec] FAIL: %s validation mismatch: %s\n",
            context,
            exec_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_exec_report(const MachineExecReport *report, const char *context) {
    return verify_exec_report_with_profile(
        report, context, MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32, 0x1000u);
}

static int verify_exec_report_with_profile(const MachineExecReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    size_t base_virtual_address) {
    MachineExecError exec_error;
    MachineExecReportOverviewArtifact overview_artifact;
    MachineExecReportSegmentArtifact segment_artifact;
    const MachineExecHeaderSummary *header_summary = NULL;
    const MachineExecSegmentSummary *segment_summary = NULL;
    const MachineExecSegmentSummary *entry_segment_summary = NULL;
    const MachineExecFile *exec_file = NULL;
    size_t segment_count = 0u;
    size_t byte_count = 0u;
    size_t executable_segment_count = 0u;
    size_t non_executable_segment_count = 0u;
    size_t segment_index = 0u;
    size_t entry_segment_index = 0u;
    char *dump_text = NULL;
    char expected_report_dump[1024];
    int ok = 1;

    memset(&exec_error, 0, sizeof(exec_error));
    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&segment_artifact, 0, sizeof(segment_artifact));
    memset(expected_report_dump, 0, sizeof(expected_report_dump));
    if (!build_expected_exec_report_dump_text(
            expected_report_dump,
            sizeof(expected_report_dump),
            machine_elf_target_profile_name(profile),
            base_virtual_address)) {
        return 0;
    }
    if (!machine_exec_report_get_summary(
            report,
            &segment_count,
            &byte_count,
            &executable_segment_count) ||
        segment_count != 1u || byte_count != 9u || executable_segment_count != 1u ||
        !machine_exec_report_get_file(report, &exec_file) ||
        !exec_file ||
        !machine_exec_report_get_header_summary_artifact(report, &header_summary) ||
        !header_summary ||
        header_summary->target_profile != profile ||
        header_summary->base_virtual_address != base_virtual_address ||
        header_summary->entry_virtual_address != base_virtual_address ||
        !machine_exec_report_get_overview_artifact(report, &overview_artifact) ||
        overview_artifact.entry_segment_index != 0u ||
        !overview_artifact.entry_segment_summary ||
        strcmp(overview_artifact.entry_segment_summary->name, ".text") != 0 ||
        !machine_exec_report_get_segment_filter_count(
            report, MACHINE_EXEC_SEGMENT_FILTER_NON_EXECUTABLE, &non_executable_segment_count) ||
        non_executable_segment_count != 0u ||
        !machine_exec_report_find_segment_summary_covering_virtual_address(
            report, base_virtual_address + 4u, &segment_index, &segment_summary) ||
        !segment_summary || segment_index != 0u ||
        !machine_exec_report_find_segment_artifact_by_name(
            report, ".text", &segment_index, &segment_artifact) ||
        segment_index != 0u || !segment_artifact.segment_summary ||
        !segment_artifact.is_entry_segment || !segment_artifact.is_executable_segment ||
        !machine_exec_report_find_segment_artifact_covering_virtual_address(
            report, base_virtual_address + 4u, &segment_index, &segment_artifact) ||
        segment_index != 0u || !segment_artifact.segment_summary ||
        !machine_exec_report_find_segment_summary_by_name(
            report, ".text", &segment_index, &segment_summary) ||
        !segment_summary || segment_index != 0u ||
        !segment_summary->name ||
        strcmp(segment_summary->name, ".text") != 0 ||
        !machine_exec_report_get_entry_segment_summary_artifact(
            report, &entry_segment_index, &entry_segment_summary) ||
        !entry_segment_summary || entry_segment_index != 0u ||
        !machine_exec_report_get_segment_summary_by_filter_index(
            report,
            MACHINE_EXEC_SEGMENT_FILTER_ENTRY,
            0u,
            &segment_index,
            &segment_summary) ||
        segment_index != 0u || !segment_summary ||
        !machine_exec_report_get_segment_artifact_by_filter_index(
            report,
            MACHINE_EXEC_SEGMENT_FILTER_EXECUTABLE,
            0u,
            &segment_index,
            &segment_artifact) ||
        segment_index != 0u || !segment_artifact.segment_summary ||
        !machine_exec_report_overview_artifact_get_segment_summary(
            &overview_artifact,
            MACHINE_EXEC_SEGMENT_FILTER_EXECUTABLE,
            0u,
            &segment_index,
            &segment_summary) ||
        segment_index != 0u || !segment_summary ||
        !machine_exec_report_overview_artifact_get_segment_artifact(
            &overview_artifact,
            MACHINE_EXEC_SEGMENT_FILTER_ENTRY,
            0u,
            &segment_index,
            &segment_artifact) ||
        segment_index != 0u || !segment_artifact.segment_summary ||
        !machine_exec_report_get_executable_segment_count(report, &executable_segment_count) ||
        executable_segment_count != 1u ||
        !machine_exec_report_get_executable_segment_summary_by_index(
            report, 0u, &segment_index, &segment_summary) ||
        !segment_summary || segment_index != 0u ||
        !machine_exec_dump_report(report, &dump_text, &exec_error)) {
        fprintf(stderr,
            "[machine-exec] FAIL: %s report mismatch: %s\n",
            context,
            exec_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_exec_file_with_profile(exec_file, "report-owned exec file", profile, base_virtual_address);
    ok &= expect_text(context, dump_text, expected_report_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_exec_builds_from_machine_ir_and_image_artifacts(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineImageFile image_file;
    MachineImageReport image_report;
    MachineExecFile exec_from_ir;
    MachineExecFile exec_from_image_file;
    MachineExecFile exec_from_image_report;
    MachineExecReport exec_report;
    MachineIrError machine_error;
    MachineImageError image_error;
    MachineExecError exec_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    char *direct_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_image_file_init(&image_file);
    machine_image_report_init(&image_report);
    machine_exec_file_init(&exec_from_ir);
    machine_exec_file_init(&exec_from_image_file);
    machine_exec_file_init(&exec_from_image_report);
    machine_exec_report_init(&exec_report);
    memset(&machine_error, 0, sizeof(machine_error));
    memset(&image_error, 0, sizeof(image_error));
    memset(&exec_error, 0, sizeof(exec_error));

    if (!build_resolved_machine_ir_report(&machine_report, &machine_error) ||
        !machine_image_build_from_machine_ir_report(&machine_report, &image_file, &image_error) ||
        !machine_image_build_report_from_machine_ir_report(&machine_report, &image_report, &image_error) ||
        !machine_exec_build_from_machine_ir_report(&machine_report, &exec_from_ir, &exec_error) ||
        !machine_exec_build_from_machine_image_file(&image_file, &exec_from_image_file, &exec_error) ||
        !machine_exec_build_from_machine_image_report(
            &image_report, &exec_from_image_report, &exec_error) ||
        !machine_exec_build_report_from_machine_image_report(
            &image_report, &exec_report, &exec_error) ||
        !machine_exec_report_refresh(&exec_report, &exec_error) ||
        !machine_exec_build_dump_from_machine_image_file(&image_file, &dump_text, &exec_error) ||
        !machine_exec_build_dump_from_file(&exec_from_ir, &direct_dump_text, &exec_error) ||
        !machine_exec_build_report_dump_from_machine_ir_report(
            &machine_report, &report_dump_text, &exec_error)) {
        fprintf(stderr,
            "[machine-exec] FAIL: build path mismatch: machine=%s image=%s exec=%s\n",
            machine_error.message,
            image_error.message,
            exec_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_exec_file(&exec_from_ir, "exec from machine-ir");
    ok &= verify_exec_file(&exec_from_image_file, "exec from image file");
    ok &= verify_exec_file(&exec_from_image_report, "exec from image report");
    ok &= verify_exec_report(&exec_report, "exec report");
    ok &= expect_text("dump helper", dump_text, EXPECTED_EXEC_DUMP);
    ok &= expect_text("direct dump helper", direct_dump_text, EXPECTED_EXEC_DUMP);
    ok &= expect_text("report dump helper", report_dump_text, EXPECTED_EXEC_REPORT_DUMP);

cleanup:
    free(direct_dump_text);
    free(dump_text);
    free(report_dump_text);
    machine_exec_report_free(&exec_report);
    machine_exec_file_free(&exec_from_image_report);
    machine_exec_file_free(&exec_from_image_file);
    machine_exec_file_free(&exec_from_ir);
    machine_image_report_free(&image_report);
    machine_image_file_free(&image_file);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_exec_rejects_entry_outside_executable_segment(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineImageFile image_file;
    MachineIrError machine_error;
    MachineImageError image_error;
    MachineExecFile exec_file;
    MachineExecError exec_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_image_file_init(&image_file);
    machine_exec_file_init(&exec_file);
    memset(&machine_error, 0, sizeof(machine_error));
    memset(&image_error, 0, sizeof(image_error));
    memset(&exec_error, 0, sizeof(exec_error));

    if (!build_resolved_machine_ir_report(&machine_report, &machine_error) ||
        !machine_image_build_from_machine_ir_report(&machine_report, &image_file, &image_error)) {
        fprintf(stderr,
            "[machine-exec] FAIL: non-executable-entry image setup failed: machine=%s image=%s\n",
            machine_error.message,
            image_error.message);
        ok = 0;
        goto cleanup;
    }

    free(image_file.segments[0].name);
    image_file.segments[0].name = dup_text(".data");
    if (!image_file.segments[0].name) {
        ok = 0;
        goto cleanup;
    }

    if (machine_exec_build_from_machine_image_file(&image_file, &exec_file, &exec_error) ||
        strstr(exec_error.message, "executable segment") == NULL) {
        fprintf(stderr,
            "[machine-exec] FAIL: non-executable entry rejection mismatch: %s\n",
            exec_error.message);
        ok = 0;
    }

cleanup:
    machine_exec_file_free(&exec_file);
    machine_image_file_free(&image_file);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_exec_empty_report_has_no_entry_filter_rows(void) {
    MachineExecReport report;
    size_t count = 1u;
    size_t segment_index = (size_t)-1;
    const MachineExecSegmentSummary *segment_summary = (const MachineExecSegmentSummary *)1;
    int ok = 1;

    machine_exec_report_init(&report);
    if (!machine_exec_report_get_segment_filter_count(
            &report, MACHINE_EXEC_SEGMENT_FILTER_ENTRY, &count) ||
        count != 0u ||
        machine_exec_report_get_segment_summary_by_filter_index(
            &report, MACHINE_EXEC_SEGMENT_FILTER_ENTRY, 0u, &segment_index, &segment_summary) ||
        segment_summary != (const MachineExecSegmentSummary *)1) {
        fprintf(stderr, "[machine-exec] FAIL: empty report entry filter should be empty\n");
        ok = 0;
    }
    machine_exec_report_free(&report);
    return ok;
}

static int test_machine_exec_marks_global_data_segments_writable(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineExecFile exec_file;
    MachineExecReport exec_report;
    MachineIrError machine_error;
    MachineExecError exec_error;
    const MachineExecSegment *text_segment = NULL;
    const MachineExecSegment *sbss_segment = NULL;
    const MachineExecSegment *sdata_segment = NULL;
    const MachineExecSegmentSummary *segment_summary = NULL;
    char *dump_text = NULL;
    size_t segment_count = 0u;
    size_t byte_count = 0u;
    size_t executable_segment_count = 0u;
    size_t non_executable_segment_count = 0u;
    size_t segment_index = 0u;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_exec_file_init(&exec_file);
    machine_exec_report_init(&exec_report);
    memset(&machine_error, 0, sizeof(machine_error));
    memset(&exec_error, 0, sizeof(exec_error));

    if (!build_global_object_machine_ir_report(&machine_report, &machine_error) ||
        !machine_exec_build_from_machine_ir_report(&machine_report, &exec_file, &exec_error) ||
        !machine_exec_build_report_from_machine_ir_report(&machine_report, &exec_report, &exec_error) ||
        !machine_exec_file_get_summary(&exec_file, &segment_count, &byte_count, &executable_segment_count) ||
        segment_count != 3u || byte_count < 12u || executable_segment_count != 1u ||
        !machine_exec_file_find_segment_by_name(&exec_file, ".text", &segment_index, &text_segment) ||
        !text_segment || segment_index != 0u ||
        !machine_exec_file_find_segment_by_name(&exec_file, ".sbss", &segment_index, &sbss_segment) ||
        !sbss_segment || segment_index != 1u ||
        !machine_exec_file_find_segment_by_name(&exec_file, ".sdata", &segment_index, &sdata_segment) ||
        !sdata_segment || segment_index != 2u ||
        !text_segment->readable || text_segment->writable || !text_segment->executable ||
        !sbss_segment->readable || !sbss_segment->writable || sbss_segment->executable ||
        !sdata_segment->readable || !sdata_segment->writable || sdata_segment->executable ||
        !machine_exec_report_get_segment_filter_count(
            &exec_report, MACHINE_EXEC_SEGMENT_FILTER_NON_EXECUTABLE, &non_executable_segment_count) ||
        non_executable_segment_count != 2u ||
        !machine_exec_report_get_segment_summary_by_filter_index(
            &exec_report,
            MACHINE_EXEC_SEGMENT_FILTER_NON_EXECUTABLE,
            0u,
            &segment_index,
            &segment_summary) ||
        !segment_summary || segment_index != 1u || strcmp(segment_summary->name, ".sbss") != 0 ||
        !segment_summary->writable ||
        !machine_exec_report_get_segment_summary_by_filter_index(
            &exec_report,
            MACHINE_EXEC_SEGMENT_FILTER_NON_EXECUTABLE,
            1u,
            &segment_index,
            &segment_summary) ||
        !segment_summary || segment_index != 2u || strcmp(segment_summary->name, ".sdata") != 0 ||
        !segment_summary->writable ||
        !machine_exec_dump_file(&exec_file, &dump_text, &exec_error) ||
        strstr(dump_text, "xseg.1 .sbss img-seg=1") == NULL ||
        strstr(dump_text, "xseg.2 .sdata img-seg=2") == NULL ||
        strstr(dump_text, ".sbss img-seg=1 vaddr=") == NULL ||
        strstr(dump_text, ".sdata img-seg=2 vaddr=") == NULL ||
        strstr(dump_text, ".sbss img-seg=1 vaddr=") == NULL ||
        strstr(dump_text, "perms=rw-") == NULL) {
        fprintf(stderr,
            "[machine-exec] FAIL: global-data segment permission mismatch: ir=%s exec=%s\n",
            machine_error.message,
            exec_error.message);
        ok = 0;
    }

    free(dump_text);
    machine_exec_report_free(&exec_report);
    machine_exec_file_free(&exec_file);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_exec_rejects_unresolved_relocations(void) {
    MachineImageFile image_file;
    MachineExecFile exec_file;
    MachineExecReport exec_report;
    MachineExecError exec_error;
    int ok = 1;

    machine_image_file_init(&image_file);
    machine_exec_file_init(&exec_file);
    machine_exec_report_init(&exec_report);
    memset(&exec_error, 0, sizeof(exec_error));

    if (!build_unresolved_machine_image_file(&image_file)) {
        machine_image_file_free(&image_file);
        machine_exec_file_free(&exec_file);
        machine_exec_report_free(&exec_report);
        return 0;
    }

    if (machine_exec_build_from_machine_image_file(&image_file, &exec_file, &exec_error) ||
        machine_exec_build_report_from_machine_image_file(&image_file, &exec_report, &exec_error) ||
        strstr(exec_error.message, "unresolved relocations") == NULL) {
        fprintf(stderr,
            "[machine-exec] FAIL: unresolved relocation rejection mismatch: %s\n",
            exec_error.message);
        ok = 0;
    }

    machine_image_file_free(&image_file);
    machine_exec_file_free(&exec_file);
    machine_exec_report_free(&exec_report);
    return ok;
}


static int test_machine_exec_rejects_wrapped_image_spans(void) {
    MachineImageFile image_file;
    MachineExecFile exec_file;
    MachineImageError image_error;
    MachineExecError exec_error;
    int ok = 1;

    machine_image_file_init(&image_file);
    machine_exec_file_init(&exec_file);
    memset(&image_error, 0, sizeof(image_error));
    memset(&exec_error, 0, sizeof(exec_error));

    if (!build_wrapped_machine_image_file(&image_file)) {
        return 0;
    }

    if (machine_exec_build_from_machine_image_file(&image_file, &exec_file, &exec_error) ||
        strstr(exec_error.message, "invalid image input") == NULL) {
        fprintf(stderr,
            "[machine-exec] FAIL: wrapped image rejection mismatch: %s\n",
            exec_error.message);
        ok = 0;
    }

    machine_exec_file_free(&exec_file);
    machine_image_file_free(&image_file);
    return ok;
}
static int test_machine_exec_elf_bridge_helpers(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineElfFile generic_elf_file;
    MachineElfReport generic_elf_report;
    unsigned char *generic_elf_bytes = NULL;
    size_t generic_elf_byte_count = 0u;
    MachineElfFile profiled_elf_file;
    MachineElfReport profiled_elf_report;
    unsigned char *profiled_elf_bytes = NULL;
    size_t profiled_elf_byte_count = 0u;
    MachineElfError elf_error;
    MachineExecFile exec_file;
    MachineExecReport exec_report;
    MachineExecError exec_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&generic_elf_file);
    machine_elf_report_init(&generic_elf_report);
    machine_elf_file_init(&profiled_elf_file);
    machine_elf_report_init(&profiled_elf_report);
    machine_exec_file_init(&exec_file);
    machine_exec_report_init(&exec_report);
    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&exec_error, 0, sizeof(exec_error));

    if (!build_resolved_machine_ir_report(&machine_report, &machine_error) ||
        !build_resolved_machine_elf_artifacts(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
            &generic_elf_file,
            &generic_elf_report,
            &generic_elf_bytes,
            &generic_elf_byte_count,
            &elf_error) ||
        !build_resolved_machine_elf_artifacts(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &profiled_elf_file,
            &profiled_elf_report,
            &profiled_elf_bytes,
            &profiled_elf_byte_count,
            &elf_error)) {
        fprintf(stderr,
            "[machine-exec] FAIL: elf bridge setup failed: machine=%s elf=%s\n",
            machine_error.message,
            elf_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_exec_build_from_machine_elf_file(&generic_elf_file, &exec_file, &exec_error) ||
        !verify_exec_file_with_profile(
            &exec_file,
            "exec from generic elf file",
            MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
            0x1000u) ||
        !machine_exec_build_from_machine_elf_report(&generic_elf_report, &exec_file, &exec_error) ||
        !verify_exec_file_with_profile(
            &exec_file,
            "exec from generic elf report",
            MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
            0x1000u) ||
        !machine_exec_build_from_machine_elf_bytes(generic_elf_bytes, generic_elf_byte_count, &exec_file, &exec_error) ||
        !verify_exec_file_with_profile(
            &exec_file,
            "exec from generic elf bytes",
            MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
            0x1000u) ||
        !machine_exec_build_from_machine_elf_file_with_profile(
            &generic_elf_file,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &exec_file,
            &exec_error) ||
        !verify_exec_file_with_profile(
            &exec_file,
            "exec from profiled elf file",
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            0x08048000u) ||
        !machine_exec_build_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &exec_file,
            &exec_error) ||
        !verify_exec_file_with_profile(
            &exec_file,
            "exec from profiled machine-ir bridge",
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            0x08048000u) ||
        !machine_exec_build_report_from_machine_elf_report_with_profile(
            &generic_elf_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &exec_report,
            &exec_error) ||
        !verify_exec_report_with_profile(
            &exec_report,
            "exec report from profiled elf report",
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            0x08048000u) ||
        !machine_exec_build_dump_from_machine_elf_bytes_with_profile(
            generic_elf_bytes,
            generic_elf_byte_count,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &dump_text,
            &exec_error) ||
        strstr(dump_text, "machine_exec profile=i386-preview base=0x8048000 entry=0x8048000") == NULL ||
        !machine_exec_build_report_dump_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &report_dump_text,
            &exec_error) ||
        strstr(report_dump_text, "machine_exec profile=i386-preview base=0x8048000 entry=0x8048000") == NULL ||
        strstr(report_dump_text, "report_segment_filters:") == NULL) {
        fprintf(stderr,
            "[machine-exec] FAIL: elf bridge helper mismatch: %s\n",
            exec_error.message);
        ok = 0;
    }

cleanup:
    free(dump_text);
    free(report_dump_text);
    free(generic_elf_bytes);
    free(profiled_elf_bytes);
    machine_exec_report_free(&exec_report);
    machine_exec_file_free(&exec_file);
    machine_elf_report_free(&profiled_elf_report);
    machine_elf_file_free(&profiled_elf_file);
    machine_elf_report_free(&generic_elf_report);
    machine_elf_file_free(&generic_elf_file);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_exec_query_helpers_reject_malformed_segment_tables(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineExecFile exec_file;
    MachineExecReport exec_report;
    MachineExecError exec_error;
    MachineExecSegment *saved_file_segments = NULL;
    MachineExecSegment *saved_report_file_segments = NULL;
    MachineExecSegmentSummary *saved_report_segment_summaries = NULL;
    size_t *saved_executable_indices = NULL;
    size_t *saved_non_executable_indices = NULL;
    const MachineExecSegment *segment = NULL;
    const MachineExecSegmentSummary *segment_summary = NULL;
    MachineExecReportSegmentArtifact segment_artifact;
    size_t segment_index = 0u;
    size_t count = 0u;
    size_t base_virtual_address = 0u;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_exec_file_init(&exec_file);
    machine_exec_report_init(&exec_report);
    memset(&machine_error, 0, sizeof(machine_error));
    memset(&exec_error, 0, sizeof(exec_error));
    memset(&segment_artifact, 0, sizeof(segment_artifact));

    if (!build_resolved_machine_ir_report(&machine_report, &machine_error) ||
        !machine_exec_build_from_machine_ir_report(&machine_report, &exec_file, &exec_error) ||
        !machine_exec_build_report_from_machine_ir_report(&machine_report, &exec_report, &exec_error)) {
        fprintf(stderr,
            "[machine-exec] FAIL: malformed-exec-query setup failed: machine=%s exec=%s\n",
            machine_error.message,
            exec_error.message);
        ok = 0;
        goto cleanup;
    }

    base_virtual_address = exec_file.segments[0].virtual_address;
    saved_file_segments = exec_file.segments;
    exec_file.segments = NULL;
    if (machine_exec_file_get_summary(&exec_file, &count, NULL, NULL) ||
        machine_exec_file_get_segment(&exec_file, 0u, &segment) ||
        machine_exec_file_find_segment_by_name(&exec_file, ".text", &segment_index, &segment) ||
        machine_exec_file_find_segment_covering_virtual_address(
            &exec_file, base_virtual_address, &segment_index, &segment) ||
        machine_exec_file_get_entry_segment(&exec_file, &segment_index, &segment) ||
        machine_exec_file_get_executable_segment_count(&exec_file, &count) ||
        machine_exec_file_get_executable_segment_by_index(&exec_file, 0u, &segment_index, &segment)) {
        fprintf(stderr, "[machine-exec] FAIL: malformed exec-file query unexpectedly succeeded\n");
        ok = 0;
    }
    exec_file.segments = saved_file_segments;

    saved_report_file_segments = exec_report.file.segments;
    saved_report_segment_summaries = exec_report.segment_summaries;
    saved_executable_indices = exec_report.executable_segment_indices;
    saved_non_executable_indices = exec_report.non_executable_segment_indices;
    exec_report.file.segments = NULL;
    exec_report.segment_summaries = NULL;
    exec_report.executable_segment_indices = NULL;
    exec_report.non_executable_segment_indices = NULL;
    if (machine_exec_report_get_summary(&exec_report, &count, NULL, NULL) ||
        machine_exec_report_get_entry_segment_summary_artifact(
            &exec_report, &segment_index, &segment_summary) ||
        machine_exec_report_get_segment_summary(&exec_report, 0u, &segment_summary) ||
        machine_exec_report_get_segment_artifact(&exec_report, 0u, &segment_artifact) ||
        machine_exec_report_find_segment_summary_by_name(
            &exec_report, ".text", &segment_index, &segment_summary) ||
        machine_exec_report_find_segment_summary_covering_virtual_address(
            &exec_report, base_virtual_address, &segment_index, &segment_summary) ||
        machine_exec_report_get_segment_filter_count(
            &exec_report, MACHINE_EXEC_SEGMENT_FILTER_EXECUTABLE, &count) ||
        machine_exec_report_get_segment_summary_by_filter_index(
            &exec_report,
            MACHINE_EXEC_SEGMENT_FILTER_EXECUTABLE,
            0u,
            &segment_index,
            &segment_summary) ||
        machine_exec_report_get_executable_segment_count(&exec_report, &count) ||
        machine_exec_report_get_executable_segment_summary_by_index(
            &exec_report, 0u, &segment_index, &segment_summary)) {
        fprintf(stderr, "[machine-exec] FAIL: malformed exec-report query unexpectedly succeeded\n");
        ok = 0;
    }
    exec_report.file.segments = saved_report_file_segments;
    exec_report.segment_summaries = saved_report_segment_summaries;
    exec_report.executable_segment_indices = saved_executable_indices;
    exec_report.non_executable_segment_indices = saved_non_executable_indices;

cleanup:
    exec_report.file.segments = saved_report_file_segments ? saved_report_file_segments : exec_report.file.segments;
    exec_report.segment_summaries =
        saved_report_segment_summaries ? saved_report_segment_summaries : exec_report.segment_summaries;
    exec_report.executable_segment_indices =
        saved_executable_indices ? saved_executable_indices : exec_report.executable_segment_indices;
    exec_report.non_executable_segment_indices =
        saved_non_executable_indices ? saved_non_executable_indices : exec_report.non_executable_segment_indices;
    exec_file.segments = saved_file_segments ? saved_file_segments : exec_file.segments;
    machine_exec_report_free(&exec_report);
    machine_exec_file_free(&exec_file);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_exec_builds_from_machine_ir_and_image_artifacts();
    ok &= test_machine_exec_elf_bridge_helpers();
    ok &= test_machine_exec_marks_global_data_segments_writable();
    ok &= test_machine_exec_rejects_unresolved_relocations();
    ok &= test_machine_exec_rejects_wrapped_image_spans();
    ok &= test_machine_exec_rejects_entry_outside_executable_segment();
    ok &= test_machine_exec_empty_report_has_no_entry_filter_rows();
    ok &= test_machine_exec_query_helpers_reject_malformed_segment_tables();

    if (!ok) {
        return 1;
    }

    printf("[machine-exec] PASS\n");
    return 0;
}
