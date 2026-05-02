#include "machine/launch.h"

#include "machine/ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int build_expected_launch_dump_text(char *buffer, size_t buffer_size,
    const char *profile_name,
    const char *pc_register_name,
    const char *sp_register_name,
    size_t base_virtual_address) {
    size_t initial_stack_pointer = base_virtual_address + 0x3000u;

    if (!buffer || buffer_size == 0u || !profile_name || !pc_register_name || !sp_register_name) {
        return 0;
    }
    return snprintf(buffer, buffer_size,
               "machine_launch profile=%s pc=0x%zx sp=0x%zx registers=2 runtime_segments=2 mapped_bytes=8192\n"
               "program_counter_register: lreg.0 %s\n"
               "stack_pointer_register: lreg.1 %s\n"
               "registers:\n"
               "  lreg.0 %s value=0x%zx\n"
               "  lreg.1 %s value=0x%zx\n",
               profile_name,
               base_virtual_address,
               initial_stack_pointer,
               pc_register_name,
               sp_register_name,
               pc_register_name,
               base_virtual_address,
               sp_register_name,
               initial_stack_pointer) > 0;
}

static int build_expected_launch_report_dump_text(char *buffer, size_t buffer_size,
    const char *profile_name,
    const char *pc_register_name,
    const char *sp_register_name,
    size_t base_virtual_address) {
    size_t initial_stack_pointer = base_virtual_address + 0x3000u;

    if (!buffer || buffer_size == 0u || !profile_name || !pc_register_name || !sp_register_name) {
        return 0;
    }
    return snprintf(buffer, buffer_size,
               "machine_launch profile=%s pc=0x%zx sp=0x%zx registers=2 runtime_segments=2 mapped_bytes=8192\n"
               "program_counter_register: lreg.0 %s\n"
               "stack_pointer_register: lreg.1 %s\n"
               "registers:\n"
               "  lreg.0 %s value=0x%zx\n"
               "  lreg.1 %s value=0x%zx\n"
               "report_overview:\n"
               "  registers=2 runtime-segments=2 mapped-bytes=8192 pc=0x%zx sp=0x%zx pc-register=0 sp-register=1\n"
               "  policy: profile=%s pc-reg=%s sp-reg=%s\n"
               "  runtime-launch: pc=0x%zx sp=0x%zx stack-segment=1 stack-base=0x%zx stack-end=0x%zx stack-bytes=4096\n"
               "  initial-stack: base=0x%zx end=0x%zx bytes=20 word=4 argc=0\n"
               "  runtime-memory: base=0x%zx end=0x%zx span-bytes=12288 mapped-bytes=8192 entry-offset=0x0 sp-offset=0x3000\n"
               "report_registers:\n"
               "  lreg.0 %s pc=yes sp=no value=0x%zx\n"
               "  lreg.1 %s pc=no sp=yes value=0x%zx\n",
               profile_name,
               base_virtual_address,
               initial_stack_pointer,
               pc_register_name,
               sp_register_name,
               pc_register_name,
               base_virtual_address,
               sp_register_name,
               initial_stack_pointer,
               base_virtual_address,
               initial_stack_pointer,
               profile_name,
               pc_register_name,
               sp_register_name,
               base_virtual_address,
               initial_stack_pointer,
               base_virtual_address + 0x2000u,
               initial_stack_pointer,
               initial_stack_pointer - 20u,
               initial_stack_pointer,
               base_virtual_address,
               initial_stack_pointer,
               pc_register_name,
               base_virtual_address,
               sp_register_name,
               initial_stack_pointer) > 0;
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
            "[machine-launch] FAIL: %s mismatch\nactual:\n%s\n",
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

static int verify_launch_file_with_profile(const MachineLaunchFile *launch_file,
    const char *context,
    MachineElfTargetProfile profile,
    const char *pc_register_name,
    const char *sp_register_name,
    size_t base_virtual_address) {
    MachineLaunchHeaderSummary header_summary;
    MachineLaunchTargetPolicySummary target_policy_summary;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineLaunchRegisterSummary register_summary;
    MachineLaunchError launch_error;
    const MachineLaunchRegister *reg = NULL;
    char *dump_text = NULL;
    char expected_dump[1024];
    size_t register_count = 0u;
    size_t runtime_segment_count = 0u;
    size_t mapped_byte_count = 0u;
    size_t register_index = 0u;
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&runtime_launch_summary, 0, sizeof(runtime_launch_summary));
    memset(&initial_stack_summary, 0, sizeof(initial_stack_summary));
    memset(&runtime_memory_summary, 0, sizeof(runtime_memory_summary));
    memset(&register_summary, 0, sizeof(register_summary));
    memset(&launch_error, 0, sizeof(launch_error));
    memset(expected_dump, 0, sizeof(expected_dump));
    if (!build_expected_launch_dump_text(
            expected_dump,
            sizeof(expected_dump),
            machine_elf_target_profile_name(profile),
            pc_register_name,
            sp_register_name,
            base_virtual_address)) {
        return 0;
    }

    if (!machine_launch_verify_file(launch_file, &launch_error) ||
        !machine_launch_file_get_summary(
            launch_file, &register_count, &runtime_segment_count, &mapped_byte_count) ||
        register_count != 2u || runtime_segment_count != 2u || mapped_byte_count != 8192u ||
        !machine_launch_file_get_header_summary(launch_file, &header_summary) ||
        header_summary.target_profile != profile ||
        header_summary.program_counter != base_virtual_address ||
        header_summary.stack_pointer != base_virtual_address + 0x3000u ||
        header_summary.register_count != 2u ||
        !machine_launch_file_get_target_policy_summary(launch_file, &target_policy_summary) ||
        !target_policy_summary.program_counter_register_name ||
        !target_policy_summary.stack_pointer_register_name ||
        strcmp(target_policy_summary.program_counter_register_name, pc_register_name) != 0 ||
        strcmp(target_policy_summary.stack_pointer_register_name, sp_register_name) != 0 ||
        !machine_launch_file_get_runtime_launch_summary(launch_file, &runtime_launch_summary) ||
        runtime_launch_summary.entry_virtual_address != base_virtual_address ||
        runtime_launch_summary.initial_stack_pointer != base_virtual_address + 0x3000u ||
        runtime_launch_summary.stack_segment_index != 1u ||
        !machine_launch_file_get_initial_stack_summary(launch_file, &initial_stack_summary) ||
        initial_stack_summary.image_base_virtual_address != base_virtual_address + 0x3000u - 20u ||
        initial_stack_summary.image_end_virtual_address != base_virtual_address + 0x3000u ||
        initial_stack_summary.image_byte_count != 20u ||
        !machine_launch_file_get_runtime_memory_summary(launch_file, &runtime_memory_summary) ||
        runtime_memory_summary.base_virtual_address != base_virtual_address ||
        runtime_memory_summary.end_virtual_address != base_virtual_address + 0x3000u ||
        runtime_memory_summary.mapped_byte_count != 8192u ||
        !machine_launch_file_get_register(launch_file, 0u, &reg) ||
        !reg || !reg->name || strcmp(reg->name, pc_register_name) != 0 || reg->value != base_virtual_address ||
        !machine_launch_file_get_register(launch_file, 1u, &reg) ||
        !reg || !reg->name || strcmp(reg->name, sp_register_name) != 0 || reg->value != base_virtual_address + 0x3000u ||
        !machine_launch_file_find_register_by_name(launch_file, sp_register_name, &register_index, &reg) ||
        register_index != 1u || !reg || reg->value != base_virtual_address + 0x3000u ||
        !machine_launch_file_get_program_counter_register(launch_file, &register_index, &reg) ||
        register_index != 0u || !reg || strcmp(reg->name, pc_register_name) != 0 ||
        !machine_launch_file_get_stack_pointer_register(launch_file, &register_index, &reg) ||
        register_index != 1u || !reg || strcmp(reg->name, sp_register_name) != 0 ||
        !machine_launch_register_get_summary(reg, &register_summary) ||
        !register_summary.name || strcmp(register_summary.name, sp_register_name) != 0 ||
        register_summary.value != base_virtual_address + 0x3000u ||
        !machine_launch_dump_file(launch_file, &dump_text, &launch_error)) {
        fprintf(stderr,
            "[machine-launch] FAIL: %s validation mismatch: %s\n",
            context,
            launch_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_launch_report_with_profile(const MachineLaunchReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    const char *pc_register_name,
    const char *sp_register_name,
    size_t base_virtual_address) {
    MachineLaunchError launch_error;
    MachineLaunchReportOverviewArtifact overview_artifact;
    MachineLaunchReportRegisterArtifact register_artifact;
    const MachineLaunchFile *launch_file = NULL;
    const MachineRuntimeFile *runtime_file = NULL;
    const MachineLaunchHeaderSummary *header_summary = NULL;
    const MachineLaunchTargetPolicySummary *target_policy_summary = NULL;
    const MachineRuntimeLaunchSummary *runtime_launch_summary = NULL;
    const MachineRuntimeInitialStackSummary *initial_stack_summary = NULL;
    const MachineRuntimeMemorySummary *runtime_memory_summary = NULL;
    const MachineLaunchRegisterSummary *register_summary = NULL;
    char *dump_text = NULL;
    char expected_dump[2048];
    size_t register_count = 0u;
    size_t runtime_segment_count = 0u;
    size_t mapped_byte_count = 0u;
    size_t register_index = 0u;
    int ok = 1;

    memset(&launch_error, 0, sizeof(launch_error));
    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&register_artifact, 0, sizeof(register_artifact));
    memset(expected_dump, 0, sizeof(expected_dump));
    if (!build_expected_launch_report_dump_text(
            expected_dump,
            sizeof(expected_dump),
            machine_elf_target_profile_name(profile),
            pc_register_name,
            sp_register_name,
            base_virtual_address)) {
        return 0;
    }

    if (!machine_launch_report_get_summary(
            report, &register_count, &runtime_segment_count, &mapped_byte_count) ||
        register_count != 2u || runtime_segment_count != 2u || mapped_byte_count != 8192u ||
        !machine_launch_report_get_overview_artifact(report, &overview_artifact) ||
        !overview_artifact.header_summary || !overview_artifact.target_policy_summary ||
        !overview_artifact.runtime_launch_summary || !overview_artifact.initial_stack_summary ||
        !overview_artifact.runtime_memory_summary ||
        overview_artifact.program_counter_register_index != 0u ||
        overview_artifact.stack_pointer_register_index != 1u ||
        !machine_launch_report_get_file(report, &launch_file) || !launch_file ||
        !machine_launch_report_get_runtime_file(report, &runtime_file) || !runtime_file ||
        !machine_launch_report_get_header_summary_artifact(report, &header_summary) ||
        !header_summary || header_summary->target_profile != profile ||
        header_summary->program_counter != base_virtual_address ||
        header_summary->stack_pointer != base_virtual_address + 0x3000u ||
        !machine_launch_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary ||
        strcmp(target_policy_summary->program_counter_register_name, pc_register_name) != 0 ||
        strcmp(target_policy_summary->stack_pointer_register_name, sp_register_name) != 0 ||
        !machine_launch_report_get_runtime_launch_summary_artifact(report, &runtime_launch_summary) ||
        !runtime_launch_summary ||
        runtime_launch_summary->entry_virtual_address != base_virtual_address ||
        runtime_launch_summary->initial_stack_pointer != base_virtual_address + 0x3000u ||
        !machine_launch_report_get_initial_stack_summary_artifact(report, &initial_stack_summary) ||
        !initial_stack_summary ||
        initial_stack_summary->image_base_virtual_address != base_virtual_address + 0x3000u - 20u ||
        !machine_launch_report_get_runtime_memory_summary_artifact(report, &runtime_memory_summary) ||
        !runtime_memory_summary ||
        runtime_memory_summary->end_virtual_address != base_virtual_address + 0x3000u ||
        !machine_launch_report_get_register_summary(report, 0u, &register_summary) ||
        !register_summary || strcmp(register_summary->name, pc_register_name) != 0 ||
        !machine_launch_report_get_register_artifact(report, 1u, &register_artifact) ||
        !register_artifact.register_summary ||
        strcmp(register_artifact.register_summary->name, sp_register_name) != 0 ||
        !register_artifact.is_stack_pointer_register ||
        !machine_launch_report_find_register_summary_by_name(report, sp_register_name, &register_index, &register_summary) ||
        register_index != 1u || !register_summary ||
        !machine_launch_report_find_register_artifact_by_name(report, pc_register_name, &register_index, &register_artifact) ||
        register_index != 0u || !register_artifact.is_program_counter_register ||
        !machine_launch_report_get_program_counter_register_summary_artifact(report, &register_index, &register_summary) ||
        register_index != 0u || !register_summary ||
        strcmp(register_summary->name, pc_register_name) != 0 ||
        !machine_launch_report_get_stack_pointer_register_summary_artifact(report, &register_index, &register_summary) ||
        register_index != 1u || !register_summary ||
        strcmp(register_summary->name, sp_register_name) != 0 ||
        !machine_launch_report_get_program_counter_register_artifact(report, &register_artifact) ||
        !register_artifact.is_program_counter_register ||
        !machine_launch_report_get_stack_pointer_register_artifact(report, &register_artifact) ||
        !register_artifact.is_stack_pointer_register ||
        !machine_launch_report_overview_artifact_get_program_counter_register_summary_artifact(
            &overview_artifact, &register_summary) ||
        !register_summary || strcmp(register_summary->name, pc_register_name) != 0 ||
        !machine_launch_report_overview_artifact_get_stack_pointer_register_summary_artifact(
            &overview_artifact, &register_summary) ||
        !register_summary || strcmp(register_summary->name, sp_register_name) != 0 ||
        !machine_launch_report_overview_artifact_get_runtime_launch_summary_artifact(
            &overview_artifact, &runtime_launch_summary) ||
        !runtime_launch_summary ||
        runtime_launch_summary->stack_segment_index != 1u ||
        !machine_launch_report_overview_artifact_get_initial_stack_summary_artifact(
            &overview_artifact, &initial_stack_summary) ||
        !initial_stack_summary || initial_stack_summary->argc != 0u ||
        !machine_launch_report_overview_artifact_get_runtime_memory_summary_artifact(
            &overview_artifact, &runtime_memory_summary) ||
        !runtime_memory_summary || runtime_memory_summary->mapped_byte_count != 8192u ||
        !machine_launch_report_overview_artifact_get_program_counter_register_artifact(
            &overview_artifact, &register_artifact) ||
        !register_artifact.is_program_counter_register ||
        !machine_launch_report_overview_artifact_get_stack_pointer_register_artifact(
            &overview_artifact, &register_artifact) ||
        !register_artifact.is_stack_pointer_register ||
        !machine_launch_dump_report(report, &dump_text, &launch_error)) {
        fprintf(stderr,
            "[machine-launch] FAIL: %s report mismatch: %s\n",
            context,
            launch_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_launch_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineRuntimeFile runtime_file;
    MachineRuntimeReport runtime_report;
    MachineLoadFile load_file;
    MachineLoadReport load_report;
    MachineLaunchFile launch_file;
    MachineLaunchFile cloned_launch;
    MachineLaunchReport launch_report;
    MachineLaunchError launch_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_runtime_file_init(&runtime_file);
    machine_runtime_report_init(&runtime_report);
    machine_load_file_init(&load_file);
    machine_load_report_init(&load_report);
    machine_launch_file_init(&launch_file);
    machine_launch_file_init(&cloned_launch);
    machine_launch_report_init(&launch_report);
    memset(&launch_error, 0, sizeof(launch_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_runtime_build_from_machine_ir_report(&ir_report, &runtime_file, NULL) ||
        !machine_runtime_build_report_from_machine_ir_report(&ir_report, &runtime_report, NULL) ||
        !machine_load_build_from_machine_ir_report(&ir_report, &load_file, NULL) ||
        !machine_load_build_report_from_machine_ir_report(&ir_report, &load_report, NULL) ||
        !machine_launch_build_from_machine_runtime_file(&runtime_file, &launch_file, &launch_error)) {
        fprintf(stderr,
            "[machine-launch] FAIL: mainline setup failed: ir=%s launch=%s\n",
            ir_error.message,
            launch_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_launch_file_with_profile(
        &launch_file,
        "launch-generic-runtime-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    if (!machine_launch_clone_file(&launch_file, &cloned_launch, &launch_error) ||
        cloned_launch.registers == launch_file.registers ||
        cloned_launch.registers[0].name == launch_file.registers[0].name) {
        fprintf(stderr, "[machine-launch] FAIL: launch clone failed: %s\n", launch_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_launch_file_with_profile(
        &cloned_launch,
        "launch-generic-cloned-runtime-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_launch_file_free(&launch_file);
    if (!machine_launch_build_from_machine_runtime_report(&runtime_report, &launch_file, &launch_error)) {
        fprintf(stderr, "[machine-launch] FAIL: launch from runtime report failed: %s\n", launch_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_launch_file_with_profile(
        &launch_file,
        "launch-generic-runtime-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_launch_file_free(&launch_file);
    if (!machine_launch_build_from_machine_load_file(&load_file, &launch_file, &launch_error)) {
        fprintf(stderr, "[machine-launch] FAIL: launch from load file failed: %s\n", launch_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_launch_file_with_profile(
        &launch_file,
        "launch-generic-load-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_launch_file_free(&launch_file);
    if (!machine_launch_build_from_machine_load_report(&load_report, &launch_file, &launch_error)) {
        fprintf(stderr, "[machine-launch] FAIL: launch from load report failed: %s\n", launch_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_launch_file_with_profile(
        &launch_file,
        "launch-generic-load-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    if (!machine_launch_build_report_from_machine_runtime_file(&runtime_file, &launch_report, &launch_error)) {
        fprintf(stderr, "[machine-launch] FAIL: launch report from runtime file failed: %s\n", launch_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_launch_report_with_profile(
        &launch_report,
        "launch-generic-runtime-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    launch_report.header_summary.program_counter = 0u;
    launch_report.runtime_launch_summary.entry_virtual_address = 0u;
    if (!machine_launch_report_refresh(&launch_report, &launch_error)) {
        fprintf(stderr, "[machine-launch] FAIL: launch report refresh failed: %s\n", launch_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_launch_report_with_profile(
        &launch_report,
        "launch-generic-refreshed-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_launch_report_free(&launch_report);
    if (!machine_launch_build_report_from_machine_runtime_report(&runtime_report, &launch_report, &launch_error)) {
        fprintf(stderr, "[machine-launch] FAIL: launch report from runtime report failed: %s\n", launch_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_launch_report_with_profile(
        &launch_report,
        "launch-generic-runtime-report-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_launch_report_free(&launch_report);
    if (!machine_launch_build_report_from_machine_load_file(&load_file, &launch_report, &launch_error)) {
        fprintf(stderr, "[machine-launch] FAIL: launch report from load file failed: %s\n", launch_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_launch_report_with_profile(
        &launch_report,
        "launch-generic-load-file-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_launch_report_free(&launch_report);
    if (!machine_launch_build_report_from_machine_load_report(&load_report, &launch_report, &launch_error)) {
        fprintf(stderr, "[machine-launch] FAIL: launch report from load report failed: %s\n", launch_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_launch_report_with_profile(
        &launch_report,
        "launch-generic-load-report-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

cleanup:
    machine_launch_report_free(&launch_report);
    machine_launch_file_free(&cloned_launch);
    machine_launch_file_free(&launch_file);
    machine_load_report_free(&load_report);
    machine_load_file_free(&load_file);
    machine_runtime_report_free(&runtime_report);
    machine_runtime_file_free(&runtime_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_launch_ir_bridge_and_wrappers(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineLaunchFile launch_file;
    MachineLaunchReport launch_report;
    MachineLaunchError launch_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_launch_file_init(&launch_file);
    machine_launch_report_init(&launch_report);
    memset(&launch_error, 0, sizeof(launch_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_launch_build_from_machine_ir_report(&ir_report, &launch_file, &launch_error) ||
        !machine_launch_build_report_from_machine_ir_report(&ir_report, &launch_report, &launch_error) ||
        !machine_launch_build_dump_from_machine_ir_report(&ir_report, &dump_text, &launch_error) ||
        !machine_launch_build_report_dump_from_machine_ir_report(&ir_report, &report_dump_text, &launch_error)) {
        fprintf(stderr,
            "[machine-launch] FAIL: ir bridge setup failed: ir=%s launch=%s\n",
            ir_error.message,
            launch_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_launch_file_with_profile(
        &launch_file,
        "launch-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);
    ok &= verify_launch_report_with_profile(
        &launch_report,
        "launch-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);
    ok &= expect_text("launch ir dump wrapper", dump_text,
        "machine_launch profile=generic-elf32 pc=0x1000 sp=0x4000 registers=2 runtime_segments=2 mapped_bytes=8192\n"
        "program_counter_register: lreg.0 pc\n"
        "stack_pointer_register: lreg.1 sp\n"
        "registers:\n"
        "  lreg.0 pc value=0x1000\n"
        "  lreg.1 sp value=0x4000\n");
    ok &= expect_text("launch ir report-dump wrapper", report_dump_text,
        "machine_launch profile=generic-elf32 pc=0x1000 sp=0x4000 registers=2 runtime_segments=2 mapped_bytes=8192\n"
        "program_counter_register: lreg.0 pc\n"
        "stack_pointer_register: lreg.1 sp\n"
        "registers:\n"
        "  lreg.0 pc value=0x1000\n"
        "  lreg.1 sp value=0x4000\n"
        "report_overview:\n"
        "  registers=2 runtime-segments=2 mapped-bytes=8192 pc=0x1000 sp=0x4000 pc-register=0 sp-register=1\n"
        "  policy: profile=generic-elf32 pc-reg=pc sp-reg=sp\n"
        "  runtime-launch: pc=0x1000 sp=0x4000 stack-segment=1 stack-base=0x3000 stack-end=0x4000 stack-bytes=4096\n"
        "  initial-stack: base=0x3fec end=0x4000 bytes=20 word=4 argc=0\n"
        "  runtime-memory: base=0x1000 end=0x4000 span-bytes=12288 mapped-bytes=8192 entry-offset=0x0 sp-offset=0x3000\n"
        "report_registers:\n"
        "  lreg.0 pc pc=yes sp=no value=0x1000\n"
        "  lreg.1 sp pc=no sp=yes value=0x4000\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_launch_report_free(&launch_report);
    machine_launch_file_free(&launch_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_launch_profile_bridge(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineLaunchFile launch_file;
    MachineLaunchReport launch_report;
    MachineLaunchError launch_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_launch_file_init(&launch_file);
    machine_launch_report_init(&launch_report);
    memset(&launch_error, 0, sizeof(launch_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_launch_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &launch_file, &launch_error) ||
        !machine_launch_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &launch_report, &launch_error) ||
        !machine_launch_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &launch_error) ||
        !machine_launch_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &launch_error)) {
        fprintf(stderr,
            "[machine-launch] FAIL: i386 bridge setup failed: ir=%s launch=%s\n",
            ir_error.message,
            launch_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_launch_file_with_profile(
        &launch_file,
        "launch-i386-ir-file",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        "eip",
        "esp",
        0x08048000u);
    ok &= verify_launch_report_with_profile(
        &launch_report,
        "launch-i386-ir-report",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        "eip",
        "esp",
        0x08048000u);
    ok &= expect_text("launch i386 dump wrapper", dump_text,
        "machine_launch profile=i386-preview pc=0x8048000 sp=0x804b000 registers=2 runtime_segments=2 mapped_bytes=8192\n"
        "program_counter_register: lreg.0 eip\n"
        "stack_pointer_register: lreg.1 esp\n"
        "registers:\n"
        "  lreg.0 eip value=0x8048000\n"
        "  lreg.1 esp value=0x804b000\n");
    ok &= expect_text("launch i386 report-dump wrapper", report_dump_text,
        "machine_launch profile=i386-preview pc=0x8048000 sp=0x804b000 registers=2 runtime_segments=2 mapped_bytes=8192\n"
        "program_counter_register: lreg.0 eip\n"
        "stack_pointer_register: lreg.1 esp\n"
        "registers:\n"
        "  lreg.0 eip value=0x8048000\n"
        "  lreg.1 esp value=0x804b000\n"
        "report_overview:\n"
        "  registers=2 runtime-segments=2 mapped-bytes=8192 pc=0x8048000 sp=0x804b000 pc-register=0 sp-register=1\n"
        "  policy: profile=i386-preview pc-reg=eip sp-reg=esp\n"
        "  runtime-launch: pc=0x8048000 sp=0x804b000 stack-segment=1 stack-base=0x804a000 stack-end=0x804b000 stack-bytes=4096\n"
        "  initial-stack: base=0x804afec end=0x804b000 bytes=20 word=4 argc=0\n"
        "  runtime-memory: base=0x8048000 end=0x804b000 span-bytes=12288 mapped-bytes=8192 entry-offset=0x0 sp-offset=0x3000\n"
        "report_registers:\n"
        "  lreg.0 eip pc=yes sp=no value=0x8048000\n"
        "  lreg.1 esp pc=no sp=yes value=0x804b000\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_launch_report_free(&launch_report);
    machine_launch_file_free(&launch_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_launch_mainline();
    ok &= test_machine_launch_ir_bridge_and_wrappers();
    ok &= test_machine_launch_profile_bridge();
    return ok ? 0 : 1;
}
