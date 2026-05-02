#include "machine/load.h"

#include "machine/elf.h"
#include "machine/ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int build_expected_load_dump_text(char *buffer, size_t buffer_size,
    const char *profile_name, size_t base_virtual_address) {
    if (!buffer || buffer_size == 0u || !profile_name) {
        return 0;
    }
    return snprintf(buffer, buffer_size,
               "machine_load profile=%s base=0x%zx entry=0x%zx segments=1 file_bytes=9 memory_bytes=4096\n"
               "entry_segment: lseg.0 .text perms=r-x\n"
               "segments:\n"
               "  lseg.0 .text xseg=0 vaddr=0x%zx file-bytes=9 mem-bytes=4096 zero-fill=4087 align=4096 perms=r-x\n",
               profile_name,
               base_virtual_address,
               base_virtual_address,
               base_virtual_address) > 0;
}

static int build_expected_load_report_dump_text(char *buffer, size_t buffer_size,
    const char *profile_name, size_t base_virtual_address) {
    if (!buffer || buffer_size == 0u || !profile_name) {
        return 0;
    }
    return snprintf(buffer, buffer_size,
               "machine_load profile=%s base=0x%zx entry=0x%zx segments=1 file_bytes=9 memory_bytes=4096\n"
               "entry_segment: lseg.0 .text perms=r-x\n"
               "segments:\n"
               "  lseg.0 .text xseg=0 vaddr=0x%zx file-bytes=9 mem-bytes=4096 zero-fill=4087 align=4096 perms=r-x\n"
               "report_overview:\n"
               "  segments=1 file_bytes=9 memory_bytes=4096 executable_segments=1 non_executable_segments=0 entry=0x%zx entry_segment=0\n"
               "  policy: profile=%s base=0x%zx align=4096\n"
               "  memory: base=0x%zx end=0x%zx entry-offset=0x0\n"
               "report_segment_filters:\n"
               "  executable[1] lseg.0(.text)\n"
               "  non-executable[0]\n"
               "  entry[1] lseg.0(.text)\n"
               "report_executable_segments:\n"
               "  lseg.0 .text mem-bytes=4096 zero-fill=4087 perms=r-x\n"
               "report_segments:\n"
               "  lseg.0 .text entry=yes executable=yes file-bytes=9 mem-bytes=4096 zero-fill=4087 perms=r-x\n",
               profile_name,
               base_virtual_address,
               base_virtual_address,
               base_virtual_address,
               base_virtual_address,
               profile_name,
               base_virtual_address,
               base_virtual_address,
               base_virtual_address + 4096u) > 0;
}

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
            "[machine-load] FAIL: %s mismatch\nactual:\n%s\n",
            label,
            actual_text ? actual_text : "<null>");
        return 0;
    }
    return 1;
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

static int verify_load_file_with_profile(const MachineLoadFile *load_file,
    const char *context,
    MachineElfTargetProfile profile,
    size_t base_virtual_address) {
    MachineLoadHeaderSummary header_summary;
    MachineLoadSegmentSummary segment_summary;
    MachineLoadError load_error;
    const MachineLoadSegment *segment = NULL;
    const MachineLoadSegment *entry_segment = NULL;
    size_t segment_count = 0u;
    size_t file_byte_count = 0u;
    size_t memory_byte_count = 0u;
    size_t executable_segment_count = 0u;
    size_t segment_index = 0u;
    size_t entry_segment_index = 0u;
    char *dump_text = NULL;
    char expected_dump[512];
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&segment_summary, 0, sizeof(segment_summary));
    memset(&load_error, 0, sizeof(load_error));
    memset(expected_dump, 0, sizeof(expected_dump));
    if (!build_expected_load_dump_text(
            expected_dump,
            sizeof(expected_dump),
            machine_elf_target_profile_name(profile),
            base_virtual_address)) {
        return 0;
    }

    if (!machine_load_verify_file(load_file, &load_error) ||
        !machine_load_file_get_summary(
            load_file,
            &segment_count,
            &file_byte_count,
            &memory_byte_count,
            &executable_segment_count) ||
        segment_count != 1u || file_byte_count != 9u || memory_byte_count != 4096u ||
        executable_segment_count != 1u ||
        !machine_load_file_get_header_summary(load_file, &header_summary) ||
        header_summary.target_profile != profile ||
        header_summary.base_virtual_address != base_virtual_address ||
        header_summary.entry_virtual_address != base_virtual_address ||
        header_summary.segment_count != 1u ||
        header_summary.file_byte_count != 9u ||
        header_summary.memory_byte_count != 4096u ||
        !machine_load_file_find_segment_covering_virtual_address(
            load_file, base_virtual_address + 4u, &segment_index, &segment) ||
        !segment || segment_index != 0u ||
        !machine_load_file_find_segment_covering_virtual_address(
            load_file, base_virtual_address + 2048u, &segment_index, &segment) ||
        !segment || segment_index != 0u ||
        !machine_load_file_find_segment_by_name(load_file, ".text", &segment_index, &segment) ||
        !segment || segment_index != 0u ||
        !machine_load_file_get_entry_segment(load_file, &entry_segment_index, &entry_segment) ||
        !entry_segment || entry_segment_index != 0u ||
        !machine_load_file_get_executable_segment_count(load_file, &executable_segment_count) ||
        executable_segment_count != 1u ||
        !machine_load_file_get_executable_segment_by_index(load_file, 0u, &segment_index, &segment) ||
        !segment || segment_index != 0u ||
        !machine_load_segment_get_summary(segment, &segment_summary) ||
        !segment_summary.name ||
        strcmp(segment_summary.name, ".text") != 0 ||
        segment_summary.exec_segment_index != 0u ||
        segment_summary.virtual_address != base_virtual_address ||
        segment_summary.file_byte_count != 9u ||
        segment_summary.memory_byte_count != 4096u ||
        !segment_summary.readable ||
        segment_summary.writable ||
        !segment_summary.executable ||
        !machine_load_dump_file(load_file, &dump_text, &load_error)) {
        fprintf(stderr,
            "[machine-load] FAIL: %s validation mismatch: %s\n",
            context,
            load_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_load_report_with_profile(const MachineLoadReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    size_t base_virtual_address) {
    MachineLoadError load_error;
    MachineLoadReportOverviewArtifact overview_artifact;
    MachineLoadReportSegmentArtifact segment_artifact;
    const MachineLoadHeaderSummary *header_summary = NULL;
    const MachineLoadTargetPolicySummary *target_policy_summary = NULL;
    const MachineLoadMemorySummary *memory_summary = NULL;
    const MachineLoadSegmentSummary *segment_summary = NULL;
    const MachineLoadSegmentSummary *entry_segment_summary = NULL;
    const MachineLoadFile *load_file = NULL;
    size_t segment_count = 0u;
    size_t file_byte_count = 0u;
    size_t memory_byte_count = 0u;
    size_t executable_segment_count = 0u;
    size_t non_executable_segment_count = 0u;
    size_t segment_index = 0u;
    size_t entry_segment_index = 0u;
    char *dump_text = NULL;
    char expected_report_dump[1024];
    int ok = 1;

    memset(&load_error, 0, sizeof(load_error));
    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&segment_artifact, 0, sizeof(segment_artifact));
    memset(expected_report_dump, 0, sizeof(expected_report_dump));
    if (!build_expected_load_report_dump_text(
            expected_report_dump,
            sizeof(expected_report_dump),
            machine_elf_target_profile_name(profile),
            base_virtual_address)) {
        return 0;
    }

    if (!machine_load_report_get_summary(
            report,
            &segment_count,
            &file_byte_count,
            &memory_byte_count,
            &executable_segment_count) ||
        segment_count != 1u || file_byte_count != 9u || memory_byte_count != 4096u ||
        executable_segment_count != 1u ||
        !machine_load_report_get_file(report, &load_file) ||
        !load_file ||
        !machine_load_report_get_header_summary_artifact(report, &header_summary) ||
        !header_summary ||
        !machine_load_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary ||
        !machine_load_report_get_memory_summary_artifact(report, &memory_summary) ||
        !memory_summary ||
        header_summary->target_profile != profile ||
        header_summary->base_virtual_address != base_virtual_address ||
        header_summary->entry_virtual_address != base_virtual_address ||
        header_summary->file_byte_count != 9u ||
        header_summary->memory_byte_count != 4096u ||
        target_policy_summary->target_profile != profile ||
        target_policy_summary->base_virtual_address != base_virtual_address ||
        target_policy_summary->segment_alignment != 0x1000u ||
        memory_summary->base_virtual_address != base_virtual_address ||
        memory_summary->end_virtual_address != base_virtual_address + 4096u ||
        memory_summary->memory_byte_count != 4096u ||
        memory_summary->entry_offset != 0u ||
        !machine_load_report_get_overview_artifact(report, &overview_artifact) ||
        !overview_artifact.target_policy_summary ||
        !overview_artifact.memory_summary ||
        overview_artifact.entry_segment_index != 0u ||
        !overview_artifact.entry_segment_summary ||
        strcmp(overview_artifact.entry_segment_summary->name, ".text") != 0 ||
        !machine_load_report_get_segment_filter_count(
            report, MACHINE_LOAD_SEGMENT_FILTER_NON_EXECUTABLE, &non_executable_segment_count) ||
        non_executable_segment_count != 0u ||
        !machine_load_report_find_segment_summary_covering_virtual_address(
            report, base_virtual_address + 4u, &segment_index, &segment_summary) ||
        !segment_summary || segment_index != 0u ||
        !machine_load_report_find_segment_summary_covering_virtual_address(
            report, base_virtual_address + 2048u, &segment_index, &segment_summary) ||
        !segment_summary || segment_index != 0u ||
        !machine_load_report_find_segment_artifact_by_name(
            report, ".text", &segment_index, &segment_artifact) ||
        segment_index != 0u || !segment_artifact.segment_summary ||
        !segment_artifact.is_entry_segment || !segment_artifact.is_executable_segment ||
        !machine_load_report_find_segment_artifact_covering_virtual_address(
            report, base_virtual_address + 4u, &segment_index, &segment_artifact) ||
        segment_index != 0u || !segment_artifact.segment_summary ||
        !machine_load_report_find_segment_summary_by_name(
            report, ".text", &segment_index, &segment_summary) ||
        !segment_summary || segment_index != 0u ||
        !segment_summary->name ||
        strcmp(segment_summary->name, ".text") != 0 ||
        !machine_load_report_get_entry_segment_summary_artifact(
            report, &entry_segment_index, &entry_segment_summary) ||
        !entry_segment_summary || entry_segment_index != 0u ||
        !machine_load_report_get_segment_summary_by_filter_index(
            report,
            MACHINE_LOAD_SEGMENT_FILTER_ENTRY,
            0u,
            &segment_index,
            &segment_summary) ||
        segment_index != 0u || !segment_summary ||
        !machine_load_report_get_segment_artifact_by_filter_index(
            report,
            MACHINE_LOAD_SEGMENT_FILTER_EXECUTABLE,
            0u,
            &segment_index,
            &segment_artifact) ||
        segment_index != 0u || !segment_artifact.segment_summary ||
        !machine_load_report_overview_artifact_get_segment_summary(
            &overview_artifact,
            MACHINE_LOAD_SEGMENT_FILTER_EXECUTABLE,
            0u,
            &segment_index,
            &segment_summary) ||
        segment_index != 0u || !segment_summary ||
        !machine_load_report_overview_artifact_get_segment_artifact(
            &overview_artifact,
            MACHINE_LOAD_SEGMENT_FILTER_ENTRY,
            0u,
            &segment_index,
            &segment_artifact) ||
        segment_index != 0u || !segment_artifact.segment_summary ||
        !machine_load_report_get_executable_segment_count(report, &executable_segment_count) ||
        executable_segment_count != 1u ||
        !machine_load_report_get_executable_segment_summary_by_index(
            report, 0u, &segment_index, &segment_summary) ||
        !segment_summary || segment_index != 0u ||
        !machine_load_dump_report(report, &dump_text, &load_error)) {
        fprintf(stderr,
            "[machine-load] FAIL: %s report mismatch: %s\n",
            context,
            load_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_load_file_with_profile(load_file, "report-owned load file", profile, base_virtual_address);
    ok &= expect_text(context, dump_text, expected_report_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_load_builds_from_machine_ir_and_upstream_artifacts(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineImageFile image_file;
    MachineImageReport image_report;
    MachineExecFile exec_file;
    MachineExecReport exec_report;
    MachineLoadFile load_file;
    MachineLoadReport load_report;
    MachineImageError image_error;
    MachineExecError exec_error;
    MachineLoadError load_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_image_file_init(&image_file);
    machine_image_report_init(&image_report);
    machine_exec_file_init(&exec_file);
    machine_exec_report_init(&exec_report);
    machine_load_file_init(&load_file);
    machine_load_report_init(&load_report);
    memset(&machine_error, 0, sizeof(machine_error));
    memset(&image_error, 0, sizeof(image_error));
    memset(&exec_error, 0, sizeof(exec_error));
    memset(&load_error, 0, sizeof(load_error));

    if (!build_resolved_machine_ir_report(&machine_report, &machine_error) ||
        !machine_image_build_from_machine_ir_report(&machine_report, &image_file, &image_error) ||
        !machine_image_build_report_from_machine_ir_report(&machine_report, &image_report, &image_error) ||
        !machine_exec_build_from_machine_ir_report(&machine_report, &exec_file, &exec_error) ||
        !machine_exec_build_report_from_machine_ir_report(&machine_report, &exec_report, &exec_error) ||
        !machine_load_build_from_machine_ir_report(&machine_report, &load_file, &load_error) ||
        !machine_load_build_report_from_machine_exec_report(&exec_report, &load_report, &load_error) ||
        !machine_load_report_refresh(&load_report, &load_error) ||
        !machine_load_build_dump_from_machine_image_file(&image_file, &dump_text, &load_error) ||
        !machine_load_build_report_dump_from_machine_ir_report(&machine_report, &report_dump_text, &load_error)) {
        fprintf(stderr,
            "[machine-load] FAIL: base bridge setup mismatch: machine=%s image=%s exec=%s load=%s\n",
            machine_error.message,
            image_error.message,
            exec_error.message,
            load_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_load_file_with_profile(
        &load_file, "load from machine-ir", MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32, 0x1000u);
    ok &= verify_load_report_with_profile(
        &load_report, "load report from exec report", MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32, 0x1000u);
    ok &= expect_text("image dump bridge", dump_text,
        "machine_load profile=generic-elf32 base=0x1000 entry=0x1000 segments=1 file_bytes=9 memory_bytes=4096\n"
        "entry_segment: lseg.0 .text perms=r-x\n"
        "segments:\n"
        "  lseg.0 .text xseg=0 vaddr=0x1000 file-bytes=9 mem-bytes=4096 zero-fill=4087 align=4096 perms=r-x\n");
    ok &= expect_text("ir report dump bridge", report_dump_text,
        "machine_load profile=generic-elf32 base=0x1000 entry=0x1000 segments=1 file_bytes=9 memory_bytes=4096\n"
        "entry_segment: lseg.0 .text perms=r-x\n"
        "segments:\n"
        "  lseg.0 .text xseg=0 vaddr=0x1000 file-bytes=9 mem-bytes=4096 zero-fill=4087 align=4096 perms=r-x\n"
        "report_overview:\n"
        "  segments=1 file_bytes=9 memory_bytes=4096 executable_segments=1 non_executable_segments=0 entry=0x1000 entry_segment=0\n"
        "  policy: profile=generic-elf32 base=0x1000 align=4096\n"
        "  memory: base=0x1000 end=0x2000 entry-offset=0x0\n"
        "report_segment_filters:\n"
        "  executable[1] lseg.0(.text)\n"
        "  non-executable[0]\n"
        "  entry[1] lseg.0(.text)\n"
        "report_executable_segments:\n"
        "  lseg.0 .text mem-bytes=4096 zero-fill=4087 perms=r-x\n"
        "report_segments:\n"
        "  lseg.0 .text entry=yes executable=yes file-bytes=9 mem-bytes=4096 zero-fill=4087 perms=r-x\n");

cleanup:
    free(dump_text);
    free(report_dump_text);
    machine_load_report_free(&load_report);
    machine_load_file_free(&load_file);
    machine_exec_report_free(&exec_report);
    machine_exec_file_free(&exec_file);
    machine_image_report_free(&image_report);
    machine_image_file_free(&image_file);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_load_elf_bridge_helpers(void) {
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
    MachineLoadFile load_file;
    MachineLoadReport load_report;
    MachineLoadError load_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&generic_elf_file);
    machine_elf_report_init(&generic_elf_report);
    machine_elf_file_init(&profiled_elf_file);
    machine_elf_report_init(&profiled_elf_report);
    machine_load_file_init(&load_file);
    machine_load_report_init(&load_report);
    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&load_error, 0, sizeof(load_error));

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
            "[machine-load] FAIL: elf bridge setup failed: machine=%s elf=%s\n",
            machine_error.message,
            elf_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_load_build_from_machine_elf_file(&generic_elf_file, &load_file, &load_error) ||
        !verify_load_file_with_profile(
            &load_file,
            "load from generic elf file",
            MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
            0x1000u) ||
        !machine_load_build_from_machine_elf_report(&generic_elf_report, &load_file, &load_error) ||
        !verify_load_file_with_profile(
            &load_file,
            "load from generic elf report",
            MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
            0x1000u) ||
        !machine_load_build_from_machine_elf_bytes(generic_elf_bytes, generic_elf_byte_count, &load_file, &load_error) ||
        !verify_load_file_with_profile(
            &load_file,
            "load from generic elf bytes",
            MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
            0x1000u) ||
        !machine_load_build_from_machine_elf_file_with_profile(
            &generic_elf_file,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &load_file,
            &load_error) ||
        !verify_load_file_with_profile(
            &load_file,
            "load from profiled elf file",
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            0x08048000u) ||
        !machine_load_build_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &load_file,
            &load_error) ||
        !verify_load_file_with_profile(
            &load_file,
            "load from profiled machine-ir bridge",
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            0x08048000u) ||
        !machine_load_build_report_from_machine_elf_report_with_profile(
            &generic_elf_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &load_report,
            &load_error) ||
        !verify_load_report_with_profile(
            &load_report,
            "load report from profiled elf report",
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            0x08048000u) ||
        !machine_load_build_dump_from_machine_elf_bytes_with_profile(
            generic_elf_bytes,
            generic_elf_byte_count,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &dump_text,
            &load_error) ||
        strstr(dump_text, "machine_load profile=i386-preview base=0x8048000 entry=0x8048000") == NULL ||
        !machine_load_build_report_dump_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &report_dump_text,
            &load_error) ||
        strstr(report_dump_text, "machine_load profile=i386-preview base=0x8048000 entry=0x8048000") == NULL ||
        strstr(report_dump_text, "report_segment_filters:") == NULL) {
        fprintf(stderr,
            "[machine-load] FAIL: elf bridge helper mismatch: %s\n",
            load_error.message);
        ok = 0;
    }

cleanup:
    free(dump_text);
    free(report_dump_text);
    free(generic_elf_bytes);
    free(profiled_elf_bytes);
    machine_load_report_free(&load_report);
    machine_load_file_free(&load_file);
    machine_elf_report_free(&profiled_elf_report);
    machine_elf_file_free(&profiled_elf_file);
    machine_elf_report_free(&generic_elf_report);
    machine_elf_file_free(&generic_elf_file);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

static int test_machine_load_rejects_non_executable_entry_segment(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_error;
    MachineExecFile exec_file;
    MachineExecError exec_error;
    MachineLoadFile load_file;
    MachineLoadError load_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_exec_file_init(&exec_file);
    machine_load_file_init(&load_file);
    memset(&machine_error, 0, sizeof(machine_error));
    memset(&exec_error, 0, sizeof(exec_error));
    memset(&load_error, 0, sizeof(load_error));

    if (!build_resolved_machine_ir_report(&machine_report, &machine_error) ||
        !machine_exec_build_from_machine_ir_report(&machine_report, &exec_file, &exec_error)) {
        fprintf(stderr,
            "[machine-load] FAIL: invalid-entry exec setup failed: machine=%s exec=%s\n",
            machine_error.message,
            exec_error.message);
        ok = 0;
        goto cleanup;
    }

    exec_file.segments[0].executable = 0u;
    if (machine_load_build_from_machine_exec_file(&exec_file, &load_file, &load_error) ||
        strstr(load_error.message, "invalid exec input") == NULL) {
        fprintf(stderr,
            "[machine-load] FAIL: invalid-entry rejection mismatch: %s\n",
            load_error.message);
        ok = 0;
    }

cleanup:
    machine_load_file_free(&load_file);
    machine_exec_file_free(&exec_file);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_load_builds_from_machine_ir_and_upstream_artifacts();
    ok &= test_machine_load_elf_bridge_helpers();
    ok &= test_machine_load_rejects_non_executable_entry_segment();

    if (!ok) {
        return 1;
    }

    printf("[machine-load] PASS\n");
    return 0;
}
