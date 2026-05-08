#include "machine/decode.h"

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
            "[machine-decode] FAIL: %s mismatch\nactual:\n%s\nexpected:\n%s\n",
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

static int verify_decode_file_with_profile(const MachineDecodeFile *decode_file,
    const char *context,
    MachineElfTargetProfile profile,
    size_t base_virtual_address) {
    MachineDecodeHeaderSummary header_summary;
    MachineDecodeTargetPolicySummary target_policy_summary;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineRuntimeSegmentSummary current_segment_summary;
    MachineStepFetchSummary fetch_summary;
    MachineDecodeTagSummary tag_summary;
    MachineDecodeError decode_error;
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
    memset(&tag_summary, 0, sizeof(tag_summary));
    memset(&decode_error, 0, sizeof(decode_error));
    memset(expected_dump, 0, sizeof(expected_dump));

    if (snprintf(expected_dump, sizeof(expected_dump),
            "machine_decode profile=%s status=ready pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n"
            "tag: class=op raw=0x1e value=0x0e known=yes name=load-local\n",
            machine_elf_target_profile_name(profile),
            base_virtual_address,
            base_virtual_address + 0x3000u,
            (size_t)0u,
            (size_t)8192u) <= 0) {
        return 0;
    }

    if (!machine_decode_verify_file(decode_file, &decode_error) ||
        !machine_decode_file_get_header_summary(decode_file, &header_summary) ||
        header_summary.target_profile != profile ||
        header_summary.step_status != MACHINE_STEP_STATUS_READY ||
        header_summary.program_counter != base_virtual_address ||
        header_summary.stack_pointer != base_virtual_address + 0x3000u ||
        header_summary.current_segment_index != 0u ||
        header_summary.mapped_byte_count != 8192u ||
        !machine_decode_file_get_target_policy_summary(decode_file, &target_policy_summary) ||
        target_policy_summary.opcode_tag_base != 0x10u ||
        target_policy_summary.terminator_tag_base != 0x80u ||
        !machine_decode_file_get_runtime_launch_summary(decode_file, &runtime_launch_summary) ||
        runtime_launch_summary.entry_virtual_address != base_virtual_address ||
        !machine_decode_file_get_initial_stack_summary(decode_file, &initial_stack_summary) ||
        initial_stack_summary.image_base_virtual_address != base_virtual_address + 0x3000u - 20u ||
        !machine_decode_file_get_runtime_memory_summary(decode_file, &runtime_memory_summary) ||
        runtime_memory_summary.end_virtual_address != base_virtual_address + 0x3000u ||
        !machine_decode_file_get_current_segment_summary(decode_file, NULL, &current_segment_summary) ||
        !current_segment_summary.name || strcmp(current_segment_summary.name, ".text") != 0 ||
        !machine_decode_file_get_fetch_summary(decode_file, &fetch_summary) ||
        fetch_summary.byte_virtual_address != base_virtual_address ||
        fetch_summary.byte_value != 0x1eu ||
        !machine_decode_file_get_tag_summary(decode_file, &tag_summary) ||
        tag_summary.tag_class != MACHINE_DECODE_TAG_OP ||
        tag_summary.raw_byte != 0x1eu ||
        tag_summary.tag_value != 0x0eu ||
        !tag_summary.is_known ||
        !tag_summary.tag_name || strcmp(tag_summary.tag_name, "load-local") != 0 ||
        !machine_decode_dump_file(decode_file, &dump_text, &decode_error)) {
        fprintf(stderr,
            "[machine-decode] FAIL: %s validation mismatch: %s\n",
            context,
            decode_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_decode_report_with_profile(const MachineDecodeReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    size_t base_virtual_address) {
    MachineDecodeError decode_error;
    MachineDecodeReportOverviewArtifact overview_artifact;
    const MachineDecodeFile *decode_file = NULL;
    const MachineStepFile *step_file = NULL;
    const MachineDecodeHeaderSummary *header_summary = NULL;
    const MachineDecodeTargetPolicySummary *target_policy_summary = NULL;
    const MachineRuntimeLaunchSummary *runtime_launch_summary = NULL;
    const MachineRuntimeInitialStackSummary *initial_stack_summary = NULL;
    const MachineRuntimeMemorySummary *runtime_memory_summary = NULL;
    const MachineRuntimeSegmentSummary *current_segment_summary = NULL;
    const MachineStepFetchSummary *fetch_summary = NULL;
    const MachineDecodeTagSummary *tag_summary = NULL;
    char *dump_text = NULL;
    char expected_dump[2048];
    int ok = 1;

    memset(&decode_error, 0, sizeof(decode_error));
    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(expected_dump, 0, sizeof(expected_dump));

    if (snprintf(expected_dump, sizeof(expected_dump),
            "machine_decode profile=%s status=ready pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n"
            "tag: class=op raw=0x1e value=0x0e known=yes name=load-local\n"
            "report_overview:\n"
            "  status=ready current-segment=0 mapped-bytes=8192 pc=0x%zx sp=0x%zx\n"
            "  policy: profile=%s op-base=0x10 term-base=0x80\n"
            "  runtime-launch: pc=0x%zx sp=0x%zx stack-segment=1\n"
            "  fetch: vaddr=0x%zx mem-offset=0x0 byte=0x1e segment=0 .text\n"
            "  tag: class=op raw=0x1e value=0x0e known=yes name=load-local\n",
            machine_elf_target_profile_name(profile),
            base_virtual_address,
            base_virtual_address + 0x3000u,
            (size_t)0u,
            (size_t)8192u,
            base_virtual_address,
            base_virtual_address + 0x3000u,
            machine_elf_target_profile_name(profile),
            base_virtual_address,
            base_virtual_address + 0x3000u,
            base_virtual_address) <= 0) {
        return 0;
    }

    if (!machine_decode_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_decode_report_get_file(report, &decode_file) || !decode_file ||
        !machine_decode_report_get_step_file(report, &step_file) || !step_file ||
        !machine_decode_report_get_header_summary_artifact(report, &header_summary) || !header_summary ||
        header_summary->target_profile != profile ||
        !machine_decode_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary || target_policy_summary->opcode_tag_base != 0x10u ||
        !machine_decode_report_get_runtime_launch_summary_artifact(report, &runtime_launch_summary) ||
        !runtime_launch_summary || runtime_launch_summary->stack_segment_index != 1u ||
        !machine_decode_report_get_initial_stack_summary_artifact(report, &initial_stack_summary) ||
        !initial_stack_summary || initial_stack_summary->argc != 0u ||
        !machine_decode_report_get_runtime_memory_summary_artifact(report, &runtime_memory_summary) ||
        !runtime_memory_summary || runtime_memory_summary->mapped_byte_count != 8192u ||
        !machine_decode_report_get_current_segment_summary_artifact(report, &current_segment_summary) ||
        !current_segment_summary || strcmp(current_segment_summary->name, ".text") != 0 ||
        !machine_decode_report_get_fetch_summary_artifact(report, &fetch_summary) ||
        !fetch_summary || fetch_summary->byte_value != 0x1eu ||
        !machine_decode_report_get_tag_summary_artifact(report, &tag_summary) ||
        !tag_summary || strcmp(tag_summary->tag_name, "load-local") != 0 ||
        !machine_decode_report_overview_artifact_get_runtime_launch_summary_artifact(&overview_artifact, &runtime_launch_summary) ||
        !runtime_launch_summary || runtime_launch_summary->entry_virtual_address != base_virtual_address ||
        !machine_decode_report_overview_artifact_get_initial_stack_summary_artifact(&overview_artifact, &initial_stack_summary) ||
        !initial_stack_summary || initial_stack_summary->image_byte_count != 20u ||
        !machine_decode_report_overview_artifact_get_runtime_memory_summary_artifact(&overview_artifact, &runtime_memory_summary) ||
        !runtime_memory_summary || runtime_memory_summary->entry_offset != 0u ||
        !machine_decode_report_overview_artifact_get_current_segment_summary_artifact(&overview_artifact, &current_segment_summary) ||
        !current_segment_summary || !current_segment_summary->executable ||
        !machine_decode_report_overview_artifact_get_fetch_summary_artifact(&overview_artifact, &fetch_summary) ||
        !fetch_summary || fetch_summary->segment_index != 0u ||
        !machine_decode_report_overview_artifact_get_tag_summary_artifact(&overview_artifact, &tag_summary) ||
        !tag_summary || tag_summary->tag_class != MACHINE_DECODE_TAG_OP ||
        !machine_decode_dump_report(report, &dump_text, &decode_error)) {
        fprintf(stderr,
            "[machine-decode] FAIL: %s report mismatch: %s\n",
            context,
            decode_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_decode_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineStepReport step_report;
    MachineLaunchFile launch_file;
    MachineLaunchReport launch_report;
    MachineDecodeFile decode_file;
    MachineDecodeFile cloned_decode;
    MachineDecodeReport decode_report;
    MachineDecodeError decode_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_step_report_init(&step_report);
    machine_launch_file_init(&launch_file);
    machine_launch_report_init(&launch_report);
    machine_decode_file_init(&decode_file);
    machine_decode_file_init(&cloned_decode);
    machine_decode_report_init(&decode_report);
    memset(&decode_error, 0, sizeof(decode_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL) ||
        !machine_step_build_report_from_machine_ir_report(&ir_report, &step_report, NULL) ||
        !machine_launch_build_from_machine_ir_report(&ir_report, &launch_file, NULL) ||
        !machine_launch_build_report_from_machine_ir_report(&ir_report, &launch_report, NULL) ||
        !machine_decode_build_from_machine_step_file(&step_file, &decode_file, &decode_error)) {
        fprintf(stderr,
            "[machine-decode] FAIL: mainline setup failed: ir=%s decode=%s\n",
            ir_error.message,
            decode_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_decode_file_with_profile(
        &decode_file,
        "decode-generic-step-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u);

    if (!machine_decode_clone_file(&decode_file, &cloned_decode, &decode_error) ||
        cloned_decode.step_file.launch_file.registers == decode_file.step_file.launch_file.registers) {
        fprintf(stderr, "[machine-decode] FAIL: decode clone failed: %s\n", decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_decode_file_with_profile(
        &cloned_decode,
        "decode-generic-cloned-step-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u);

    machine_decode_file_free(&decode_file);
    if (!machine_decode_build_from_machine_step_report(&step_report, &decode_file, &decode_error)) {
        fprintf(stderr, "[machine-decode] FAIL: decode from step report failed: %s\n", decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_decode_file_with_profile(
        &decode_file,
        "decode-generic-step-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u);

    machine_decode_file_free(&decode_file);
    if (!machine_decode_build_from_machine_launch_file(&launch_file, &decode_file, &decode_error)) {
        fprintf(stderr, "[machine-decode] FAIL: decode from launch file failed: %s\n", decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_decode_file_with_profile(
        &decode_file,
        "decode-generic-launch-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u);

    machine_decode_file_free(&decode_file);
    if (!machine_decode_build_from_machine_launch_report(&launch_report, &decode_file, &decode_error)) {
        fprintf(stderr, "[machine-decode] FAIL: decode from launch report failed: %s\n", decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_decode_file_with_profile(
        &decode_file,
        "decode-generic-launch-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u);

    if (!machine_decode_build_report_from_machine_step_file(&step_file, &decode_report, &decode_error)) {
        fprintf(stderr, "[machine-decode] FAIL: decode report from step file failed: %s\n", decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_decode_report_with_profile(
        &decode_report,
        "decode-generic-step-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u);

    decode_report.header_summary.program_counter = 0u;
    decode_report.tag_summary.raw_byte = 0u;
    if (!machine_decode_report_refresh(&decode_report, &decode_error)) {
        fprintf(stderr, "[machine-decode] FAIL: decode report refresh failed: %s\n", decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_decode_report_with_profile(
        &decode_report,
        "decode-generic-refreshed-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u);

    machine_decode_report_free(&decode_report);
    if (!machine_decode_build_report_from_machine_step_report(&step_report, &decode_report, &decode_error)) {
        fprintf(stderr, "[machine-decode] FAIL: decode report from step report failed: %s\n", decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_decode_report_with_profile(
        &decode_report,
        "decode-generic-step-report-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u);

cleanup:
    machine_decode_report_free(&decode_report);
    machine_decode_file_free(&cloned_decode);
    machine_decode_file_free(&decode_file);
    machine_launch_report_free(&launch_report);
    machine_launch_file_free(&launch_file);
    machine_step_report_free(&step_report);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_decode_ir_bridge_and_profile(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineDecodeFile decode_file;
    MachineDecodeReport decode_report;
    MachineDecodeError decode_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_decode_file_init(&decode_file);
    machine_decode_report_init(&decode_report);
    memset(&decode_error, 0, sizeof(decode_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_decode_build_from_machine_ir_report(&ir_report, &decode_file, &decode_error) ||
        !machine_decode_build_report_from_machine_ir_report(&ir_report, &decode_report, &decode_error) ||
        !machine_decode_build_dump_from_machine_ir_report(&ir_report, &dump_text, &decode_error) ||
        !machine_decode_build_report_dump_from_machine_ir_report(&ir_report, &report_dump_text, &decode_error)) {
        fprintf(stderr,
            "[machine-decode] FAIL: ir bridge setup failed: ir=%s decode=%s\n",
            ir_error.message,
            decode_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_decode_file_with_profile(
        &decode_file,
        "decode-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u);
    ok &= verify_decode_report_with_profile(
        &decode_report,
        "decode-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u);
    ok &= expect_text("decode ir dump wrapper", dump_text,
        "machine_decode profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "tag: class=op raw=0x1e value=0x0e known=yes name=load-local\n");
    ok &= expect_text("decode ir report-dump wrapper", report_dump_text,
        "machine_decode profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "tag: class=op raw=0x1e value=0x0e known=yes name=load-local\n"
        "report_overview:\n"
        "  status=ready current-segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000\n"
        "  policy: profile=generic-elf32 op-base=0x10 term-base=0x80\n"
        "  runtime-launch: pc=0x1000 sp=0x4000 stack-segment=1\n"
        "  fetch: vaddr=0x1000 mem-offset=0x0 byte=0x1e segment=0 .text\n"
        "  tag: class=op raw=0x1e value=0x0e known=yes name=load-local\n");

    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    machine_decode_report_free(&decode_report);
    machine_decode_file_free(&decode_file);
    if (!machine_decode_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &decode_file, &decode_error) ||
        !machine_decode_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &decode_report, &decode_error) ||
        !machine_decode_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &decode_error) ||
        !machine_decode_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &decode_error)) {
        fprintf(stderr,
            "[machine-decode] FAIL: i386 bridge setup failed: ir=%s decode=%s\n",
            ir_error.message,
            decode_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_decode_file_with_profile(
        &decode_file,
        "decode-i386-ir-file",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        0x08048000u);
    ok &= verify_decode_report_with_profile(
        &decode_report,
        "decode-i386-ir-report",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        0x08048000u);
    ok &= expect_text("decode i386 dump wrapper", dump_text,
        "machine_decode profile=i386-preview status=ready pc=0x8048000 sp=0x804b000 current_segment=0 mapped_bytes=8192\n"
        "tag: class=op raw=0x1e value=0x0e known=yes name=load-local\n");
    ok &= expect_text("decode i386 report-dump wrapper", report_dump_text,
        "machine_decode profile=i386-preview status=ready pc=0x8048000 sp=0x804b000 current_segment=0 mapped_bytes=8192\n"
        "tag: class=op raw=0x1e value=0x0e known=yes name=load-local\n"
        "report_overview:\n"
        "  status=ready current-segment=0 mapped-bytes=8192 pc=0x8048000 sp=0x804b000\n"
        "  policy: profile=i386-preview op-base=0x10 term-base=0x80\n"
        "  runtime-launch: pc=0x8048000 sp=0x804b000 stack-segment=1\n"
        "  fetch: vaddr=0x8048000 mem-offset=0x0 byte=0x1e segment=0 .text\n"
        "  tag: class=op raw=0x1e value=0x0e known=yes name=load-local\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_decode_report_free(&decode_report);
    machine_decode_file_free(&decode_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_decode_mainline();
    ok &= test_machine_decode_ir_bridge_and_profile();
    return ok ? 0 : 1;
}
