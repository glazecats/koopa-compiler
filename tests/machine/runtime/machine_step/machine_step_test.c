#include "machine/step.h"

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
            "[machine-step] FAIL: %s mismatch\nactual:\n%s\nexpected:\n%s\n",
            label,
            actual_text ? actual_text : "<null>",
            expected_text ? expected_text : "<null>");
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

static int verify_step_file_with_profile(const MachineStepFile *step_file,
    const char *context,
    MachineElfTargetProfile profile,
    const char *pc_register_name,
    const char *sp_register_name,
    size_t base_virtual_address) {
    MachineStepHeaderSummary header_summary;
    MachineStepTargetPolicySummary target_policy_summary;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineRuntimeSegmentSummary current_segment_summary;
    MachineStepFetchSummary fetch_summary;
    MachineStepError step_error;
    char *dump_text = NULL;
    int ok = 1;
    char expected_dump[1024];

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&runtime_launch_summary, 0, sizeof(runtime_launch_summary));
    memset(&initial_stack_summary, 0, sizeof(initial_stack_summary));
    memset(&runtime_memory_summary, 0, sizeof(runtime_memory_summary));
    memset(&current_segment_summary, 0, sizeof(current_segment_summary));
    memset(&fetch_summary, 0, sizeof(fetch_summary));
    memset(&step_error, 0, sizeof(step_error));
    memset(expected_dump, 0, sizeof(expected_dump));

    if (snprintf(expected_dump, sizeof(expected_dump),
            "machine_step profile=%s status=ready pc=0x%zx sp=0x%zx launch_registers=2 runtime_segments=2 mapped_bytes=8192\n"
            "current_segment: rseg.0 .text\n"
            "fetch: vaddr=0x%zx mem-offset=0x0 byte=0x1c\n",
            machine_elf_target_profile_name(profile),
            base_virtual_address,
            base_virtual_address + 0x3000u,
            base_virtual_address) <= 0) {
        return 0;
    }

    if (!machine_step_verify_file(step_file, &step_error) ||
        !machine_step_file_get_header_summary(step_file, &header_summary) ||
        header_summary.target_profile != profile ||
        header_summary.status != MACHINE_STEP_STATUS_READY ||
        header_summary.program_counter != base_virtual_address ||
        header_summary.stack_pointer != base_virtual_address + 0x3000u ||
        header_summary.launch_register_count != 2u ||
        header_summary.runtime_segment_count != 2u ||
        header_summary.mapped_byte_count != 8192u ||
        header_summary.current_segment_index != 0u ||
        !machine_step_file_get_target_policy_summary(step_file, &target_policy_summary) ||
        strcmp(target_policy_summary.program_counter_register_name, pc_register_name) != 0 ||
        strcmp(target_policy_summary.stack_pointer_register_name, sp_register_name) != 0 ||
        target_policy_summary.fetch_byte_count != 1u ||
        !machine_step_file_get_runtime_launch_summary(step_file, &runtime_launch_summary) ||
        runtime_launch_summary.entry_virtual_address != base_virtual_address ||
        runtime_launch_summary.initial_stack_pointer != base_virtual_address + 0x3000u ||
        !machine_step_file_get_initial_stack_summary(step_file, &initial_stack_summary) ||
        initial_stack_summary.image_base_virtual_address != base_virtual_address + 0x3000u - 20u ||
        !machine_step_file_get_runtime_memory_summary(step_file, &runtime_memory_summary) ||
        runtime_memory_summary.base_virtual_address != base_virtual_address ||
        runtime_memory_summary.end_virtual_address != base_virtual_address + 0x3000u ||
        !machine_step_file_get_current_segment_summary(step_file, NULL, &current_segment_summary) ||
        !current_segment_summary.name || strcmp(current_segment_summary.name, ".text") != 0 ||
        !machine_step_file_get_fetch_summary(step_file, &fetch_summary) ||
        fetch_summary.byte_virtual_address != base_virtual_address ||
        fetch_summary.byte_memory_offset != 0u ||
        fetch_summary.segment_index != 0u ||
        !fetch_summary.segment_name || strcmp(fetch_summary.segment_name, ".text") != 0 ||
        fetch_summary.byte_value != 0x1cu ||
        !machine_step_dump_file(step_file, &dump_text, &step_error)) {
        fprintf(stderr,
            "[machine-step] FAIL: %s validation mismatch: %s\n",
            context,
            step_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_step_report_with_profile(const MachineStepReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    const char *pc_register_name,
    const char *sp_register_name,
    size_t base_virtual_address) {
    MachineStepError step_error;
    MachineStepReportOverviewArtifact overview_artifact;
    const MachineStepFile *step_file = NULL;
    const MachineLaunchFile *launch_file = NULL;
    const MachineStepHeaderSummary *header_summary = NULL;
    const MachineStepTargetPolicySummary *target_policy_summary = NULL;
    const MachineRuntimeLaunchSummary *runtime_launch_summary = NULL;
    const MachineRuntimeInitialStackSummary *initial_stack_summary = NULL;
    const MachineRuntimeMemorySummary *runtime_memory_summary = NULL;
    const MachineRuntimeSegmentSummary *current_segment_summary = NULL;
    const MachineStepFetchSummary *fetch_summary = NULL;
    char *dump_text = NULL;
    char expected_dump[2048];
    int ok = 1;

    memset(&step_error, 0, sizeof(step_error));
    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(expected_dump, 0, sizeof(expected_dump));

    if (snprintf(expected_dump, sizeof(expected_dump),
            "machine_step profile=%s status=ready pc=0x%zx sp=0x%zx launch_registers=2 runtime_segments=2 mapped_bytes=8192\n"
            "current_segment: rseg.0 .text\n"
            "fetch: vaddr=0x%zx mem-offset=0x0 byte=0x1c\n"
            "report_overview:\n"
            "  status=ready launch-registers=2 runtime-segments=2 mapped-bytes=8192 current-segment=0 pc=0x%zx sp=0x%zx\n"
            "  policy: profile=%s pc-reg=%s sp-reg=%s fetch-bytes=1\n"
            "  runtime-launch: pc=0x%zx sp=0x%zx stack-segment=1 stack-base=0x%zx stack-end=0x%zx stack-bytes=4096\n"
            "  initial-stack: base=0x%zx end=0x%zx bytes=20 word=4 argc=0\n"
            "  runtime-memory: base=0x%zx end=0x%zx span-bytes=12288 mapped-bytes=8192 entry-offset=0x0 sp-offset=0x3000\n"
            "  current-segment: rseg.0 .text perms=r-x\n"
            "  fetch: vaddr=0x%zx mem-offset=0x0 byte=0x1c segment=0 .text\n",
            machine_elf_target_profile_name(profile),
            base_virtual_address,
            base_virtual_address + 0x3000u,
            base_virtual_address,
            base_virtual_address,
            base_virtual_address + 0x3000u,
            machine_elf_target_profile_name(profile),
            pc_register_name,
            sp_register_name,
            base_virtual_address,
            base_virtual_address + 0x3000u,
            base_virtual_address + 0x2000u,
            base_virtual_address + 0x3000u,
            base_virtual_address + 0x3000u - 20u,
            base_virtual_address + 0x3000u,
            base_virtual_address,
            base_virtual_address + 0x3000u,
            base_virtual_address) <= 0) {
        return 0;
    }

    if (!machine_step_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_step_report_get_file(report, &step_file) || !step_file ||
        !machine_step_report_get_launch_file(report, &launch_file) || !launch_file ||
        !machine_step_report_get_header_summary_artifact(report, &header_summary) || !header_summary ||
        header_summary->target_profile != profile ||
        header_summary->status != MACHINE_STEP_STATUS_READY ||
        !machine_step_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary ||
        strcmp(target_policy_summary->program_counter_register_name, pc_register_name) != 0 ||
        strcmp(target_policy_summary->stack_pointer_register_name, sp_register_name) != 0 ||
        !machine_step_report_get_runtime_launch_summary_artifact(report, &runtime_launch_summary) ||
        !runtime_launch_summary || runtime_launch_summary->entry_virtual_address != base_virtual_address ||
        !machine_step_report_get_initial_stack_summary_artifact(report, &initial_stack_summary) ||
        !initial_stack_summary || initial_stack_summary->argc != 0u ||
        !machine_step_report_get_runtime_memory_summary_artifact(report, &runtime_memory_summary) ||
        !runtime_memory_summary || runtime_memory_summary->mapped_byte_count != 8192u ||
        !machine_step_report_get_current_segment_summary_artifact(report, NULL, &current_segment_summary) ||
        !current_segment_summary || strcmp(current_segment_summary->name, ".text") != 0 ||
        !machine_step_report_get_fetch_summary_artifact(report, &fetch_summary) ||
        !fetch_summary || fetch_summary->byte_virtual_address != base_virtual_address ||
        !machine_step_report_overview_artifact_get_runtime_launch_summary_artifact(&overview_artifact, &runtime_launch_summary) ||
        !runtime_launch_summary || runtime_launch_summary->stack_segment_index != 1u ||
        !machine_step_report_overview_artifact_get_initial_stack_summary_artifact(&overview_artifact, &initial_stack_summary) ||
        !initial_stack_summary || initial_stack_summary->image_byte_count != 20u ||
        !machine_step_report_overview_artifact_get_runtime_memory_summary_artifact(&overview_artifact, &runtime_memory_summary) ||
        !runtime_memory_summary || runtime_memory_summary->entry_offset != 0u ||
        !machine_step_report_overview_artifact_get_current_segment_summary_artifact(&overview_artifact, &current_segment_summary) ||
        !current_segment_summary || !current_segment_summary->executable ||
        !machine_step_report_overview_artifact_get_fetch_summary_artifact(&overview_artifact, &fetch_summary) ||
        !fetch_summary || fetch_summary->byte_value != 0x1cu ||
        !machine_step_dump_report(report, &dump_text, &step_error)) {
        fprintf(stderr,
            "[machine-step] FAIL: %s report mismatch: %s\n",
            context,
            step_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_step_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineLaunchFile launch_file;
    MachineLaunchReport launch_report;
    MachineRuntimeFile runtime_file;
    MachineRuntimeReport runtime_report;
    MachineLoadFile load_file;
    MachineLoadReport load_report;
    MachineStepFile step_file;
    MachineStepFile cloned_step;
    MachineStepReport step_report;
    MachineStepError step_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_launch_file_init(&launch_file);
    machine_launch_report_init(&launch_report);
    machine_runtime_file_init(&runtime_file);
    machine_runtime_report_init(&runtime_report);
    machine_load_file_init(&load_file);
    machine_load_report_init(&load_report);
    machine_step_file_init(&step_file);
    machine_step_file_init(&cloned_step);
    machine_step_report_init(&step_report);
    memset(&step_error, 0, sizeof(step_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_launch_build_from_machine_ir_report(&ir_report, &launch_file, NULL) ||
        !machine_launch_build_report_from_machine_ir_report(&ir_report, &launch_report, NULL) ||
        !machine_runtime_build_from_machine_ir_report(&ir_report, &runtime_file, NULL) ||
        !machine_runtime_build_report_from_machine_ir_report(&ir_report, &runtime_report, NULL) ||
        !machine_load_build_from_machine_ir_report(&ir_report, &load_file, NULL) ||
        !machine_load_build_report_from_machine_ir_report(&ir_report, &load_report, NULL) ||
        !machine_step_build_from_machine_launch_file(&launch_file, &step_file, &step_error)) {
        fprintf(stderr,
            "[machine-step] FAIL: mainline setup failed: ir=%s step=%s\n",
            ir_error.message,
            step_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_step_file_with_profile(
        &step_file,
        "step-generic-launch-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    if (!machine_step_clone_file(&step_file, &cloned_step, &step_error) ||
        cloned_step.launch_file.registers == step_file.launch_file.registers) {
        fprintf(stderr, "[machine-step] FAIL: step clone failed: %s\n", step_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_step_file_with_profile(
        &cloned_step,
        "step-generic-cloned-launch-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_step_file_free(&step_file);
    if (!machine_step_build_from_machine_launch_report(&launch_report, &step_file, &step_error)) {
        fprintf(stderr, "[machine-step] FAIL: step from launch report failed: %s\n", step_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_step_file_with_profile(
        &step_file,
        "step-generic-launch-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_step_file_free(&step_file);
    if (!machine_step_build_from_machine_runtime_file(&runtime_file, &step_file, &step_error)) {
        fprintf(stderr, "[machine-step] FAIL: step from runtime file failed: %s\n", step_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_step_file_with_profile(
        &step_file,
        "step-generic-runtime-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_step_file_free(&step_file);
    if (!machine_step_build_from_machine_runtime_report(&runtime_report, &step_file, &step_error)) {
        fprintf(stderr, "[machine-step] FAIL: step from runtime report failed: %s\n", step_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_step_file_with_profile(
        &step_file,
        "step-generic-runtime-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_step_file_free(&step_file);
    if (!machine_step_build_from_machine_load_file(&load_file, &step_file, &step_error)) {
        fprintf(stderr, "[machine-step] FAIL: step from load file failed: %s\n", step_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_step_file_with_profile(
        &step_file,
        "step-generic-load-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_step_file_free(&step_file);
    if (!machine_step_build_from_machine_load_report(&load_report, &step_file, &step_error)) {
        fprintf(stderr, "[machine-step] FAIL: step from load report failed: %s\n", step_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_step_file_with_profile(
        &step_file,
        "step-generic-load-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    if (!machine_step_build_report_from_machine_launch_file(&launch_file, &step_report, &step_error)) {
        fprintf(stderr, "[machine-step] FAIL: step report from launch file failed: %s\n", step_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_step_report_with_profile(
        &step_report,
        "step-generic-launch-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    step_report.header_summary.program_counter = 0u;
    step_report.fetch_summary.byte_virtual_address = 0u;
    if (!machine_step_report_refresh(&step_report, &step_error)) {
        fprintf(stderr, "[machine-step] FAIL: step report refresh failed: %s\n", step_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_step_report_with_profile(
        &step_report,
        "step-generic-refreshed-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

    machine_step_report_free(&step_report);
    if (!machine_step_build_report_from_machine_launch_report(&launch_report, &step_report, &step_error)) {
        fprintf(stderr, "[machine-step] FAIL: step report from launch report failed: %s\n", step_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_step_report_with_profile(
        &step_report,
        "step-generic-launch-report-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);

cleanup:
    machine_step_report_free(&step_report);
    machine_step_file_free(&cloned_step);
    machine_step_file_free(&step_file);
    machine_load_report_free(&load_report);
    machine_load_file_free(&load_file);
    machine_runtime_report_free(&runtime_report);
    machine_runtime_file_free(&runtime_file);
    machine_launch_report_free(&launch_report);
    machine_launch_file_free(&launch_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_step_ir_bridge_and_profile(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineStepReport step_report;
    MachineStepError step_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_step_report_init(&step_report);
    memset(&step_error, 0, sizeof(step_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, &step_error) ||
        !machine_step_build_report_from_machine_ir_report(&ir_report, &step_report, &step_error) ||
        !machine_step_build_dump_from_machine_ir_report(&ir_report, &dump_text, &step_error) ||
        !machine_step_build_report_dump_from_machine_ir_report(&ir_report, &report_dump_text, &step_error)) {
        fprintf(stderr,
            "[machine-step] FAIL: ir bridge setup failed: ir=%s step=%s\n",
            ir_error.message,
            step_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_step_file_with_profile(
        &step_file,
        "step-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);
    ok &= verify_step_report_with_profile(
        &step_report,
        "step-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        "pc",
        "sp",
        0x1000u);
    ok &= expect_text("step ir dump wrapper", dump_text,
        "machine_step profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 launch_registers=2 runtime_segments=2 mapped_bytes=8192\n"
        "current_segment: rseg.0 .text\n"
        "fetch: vaddr=0x1000 mem-offset=0x0 byte=0x1c\n");
    ok &= expect_text("step ir report-dump wrapper", report_dump_text,
        "machine_step profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 launch_registers=2 runtime_segments=2 mapped_bytes=8192\n"
        "current_segment: rseg.0 .text\n"
        "fetch: vaddr=0x1000 mem-offset=0x0 byte=0x1c\n"
        "report_overview:\n"
        "  status=ready launch-registers=2 runtime-segments=2 mapped-bytes=8192 current-segment=0 pc=0x1000 sp=0x4000\n"
        "  policy: profile=generic-elf32 pc-reg=pc sp-reg=sp fetch-bytes=1\n"
        "  runtime-launch: pc=0x1000 sp=0x4000 stack-segment=1 stack-base=0x3000 stack-end=0x4000 stack-bytes=4096\n"
        "  initial-stack: base=0x3fec end=0x4000 bytes=20 word=4 argc=0\n"
        "  runtime-memory: base=0x1000 end=0x4000 span-bytes=12288 mapped-bytes=8192 entry-offset=0x0 sp-offset=0x3000\n"
        "  current-segment: rseg.0 .text perms=r-x\n"
        "  fetch: vaddr=0x1000 mem-offset=0x0 byte=0x1c segment=0 .text\n");

    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    machine_step_report_free(&step_report);
    machine_step_file_free(&step_file);
    if (!machine_step_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &step_file, &step_error) ||
        !machine_step_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &step_report, &step_error) ||
        !machine_step_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &step_error) ||
        !machine_step_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &step_error)) {
        fprintf(stderr,
            "[machine-step] FAIL: i386 bridge setup failed: ir=%s step=%s\n",
            ir_error.message,
            step_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_step_file_with_profile(
        &step_file,
        "step-i386-ir-file",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        "eip",
        "esp",
        0x08048000u);
    ok &= verify_step_report_with_profile(
        &step_report,
        "step-i386-ir-report",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        "eip",
        "esp",
        0x08048000u);
    ok &= expect_text("step i386 dump wrapper", dump_text,
        "machine_step profile=i386-preview status=ready pc=0x8048000 sp=0x804b000 launch_registers=2 runtime_segments=2 mapped_bytes=8192\n"
        "current_segment: rseg.0 .text\n"
        "fetch: vaddr=0x8048000 mem-offset=0x0 byte=0x1c\n");
    ok &= expect_text("step i386 report-dump wrapper", report_dump_text,
        "machine_step profile=i386-preview status=ready pc=0x8048000 sp=0x804b000 launch_registers=2 runtime_segments=2 mapped_bytes=8192\n"
        "current_segment: rseg.0 .text\n"
        "fetch: vaddr=0x8048000 mem-offset=0x0 byte=0x1c\n"
        "report_overview:\n"
        "  status=ready launch-registers=2 runtime-segments=2 mapped-bytes=8192 current-segment=0 pc=0x8048000 sp=0x804b000\n"
        "  policy: profile=i386-preview pc-reg=eip sp-reg=esp fetch-bytes=1\n"
        "  runtime-launch: pc=0x8048000 sp=0x804b000 stack-segment=1 stack-base=0x804a000 stack-end=0x804b000 stack-bytes=4096\n"
        "  initial-stack: base=0x804afec end=0x804b000 bytes=20 word=4 argc=0\n"
        "  runtime-memory: base=0x8048000 end=0x804b000 span-bytes=12288 mapped-bytes=8192 entry-offset=0x0 sp-offset=0x3000\n"
        "  current-segment: rseg.0 .text perms=r-x\n"
        "  fetch: vaddr=0x8048000 mem-offset=0x0 byte=0x1c segment=0 .text\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_step_report_free(&step_report);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_step_mainline();
    ok &= test_machine_step_ir_bridge_and_profile();
    return ok ? 0 : 1;
}
