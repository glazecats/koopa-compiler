#include "machine/payload_decode.h"

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
            "[machine-payload-decode] FAIL: %s mismatch\nactual:\n%s\nexpected:\n%s\n",
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

static int overwrite_step_tag_byte(MachineStepFile *step_file, unsigned char tag_byte) {
    MachineRuntimeFile *runtime_file = NULL;
    MachineLoadFile *load_file = NULL;
    MachineImageFile *image_file = NULL;

    if (!step_file) {
        return 0;
    }
    runtime_file = &step_file->launch_file.runtime_file;
    load_file = &runtime_file->load_file;
    image_file = &load_file->exec_file.image_file;
    if (runtime_file->segment_count == 0u || !runtime_file->segments ||
        load_file->segment_count == 0u || !load_file->segments ||
        image_file->byte_count < 1u || !image_file->bytes ||
        runtime_file->segments[0].byte_count < 1u ||
        load_file->segments[0].memory_byte_count < 1u) {
        return 0;
    }
    step_file->current_byte = tag_byte;
    runtime_file->segments[0].bytes[0] = tag_byte;
    load_file->segments[0].bytes[0] = tag_byte;
    image_file->bytes[0] = tag_byte;
    return 1;
}

static int verify_payload_decode_file_with_profile(const MachinePayloadDecodeFile *decode_file,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    size_t base_virtual_address) {
    MachinePayloadDecodeHeaderSummary header_summary;
    MachinePayloadDecodeTargetPolicySummary target_policy_summary;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineRuntimeSegmentSummary current_segment_summary;
    MachineStepFetchSummary fetch_summary;
    MachineDecodeTagSummary decode_tag_summary;
    MachinePayloadDecodeSummary payload_summary;
    MachineElfArtifactSummary source_artifact_summary;
    MachinePayloadDecodeError decode_error;
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
    memset(&decode_tag_summary, 0, sizeof(decode_tag_summary));
    memset(&payload_summary, 0, sizeof(payload_summary));
    memset(&source_artifact_summary, 0, sizeof(source_artifact_summary));
    memset(&decode_error, 0, sizeof(decode_error));
    memset(expected_dump, 0, sizeof(expected_dump));

    if (snprintf(expected_dump, sizeof(expected_dump),
            "machine_payload_decode profile=%s elf_origin=%s elf_semantics=%s status=ready pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n"
            "payload: kind=op raw=0x1e value=0x0e known=yes name=load-local total-bytes=1 payload-bytes=0 imm=0 bytes=[]\n",
            machine_elf_target_profile_name(profile),
            machine_elf_target_profile_name(origin_profile),
            machine_elf_relocation_semantics_name(semantics),
            base_virtual_address,
            base_virtual_address + 0x3000u,
            (size_t)0u,
            (size_t)8192u) <= 0) {
        return 0;
    }

    if (!machine_payload_decode_verify_file(decode_file, &decode_error) ||
        !machine_payload_decode_file_get_source_elf_artifact_summary(decode_file, &source_artifact_summary) ||
        source_artifact_summary.target_profile != profile ||
        source_artifact_summary.origin_profile != origin_profile ||
        source_artifact_summary.relocation_semantics != semantics ||
        !machine_payload_decode_file_get_header_summary(decode_file, &header_summary) ||
        header_summary.target_profile != profile ||
        header_summary.step_status != MACHINE_STEP_STATUS_READY ||
        header_summary.program_counter != base_virtual_address ||
        header_summary.stack_pointer != base_virtual_address + 0x3000u ||
        header_summary.current_segment_index != 0u ||
        header_summary.mapped_byte_count != 8192u ||
        !machine_payload_decode_file_get_target_policy_summary(decode_file, &target_policy_summary) ||
        target_policy_summary.max_payload_byte_count != 3u ||
        !machine_payload_decode_file_get_runtime_launch_summary(decode_file, &runtime_launch_summary) ||
        runtime_launch_summary.entry_virtual_address != base_virtual_address ||
        !machine_payload_decode_file_get_initial_stack_summary(decode_file, &initial_stack_summary) ||
        initial_stack_summary.image_base_virtual_address != base_virtual_address + 0x3000u - 20u ||
        !machine_payload_decode_file_get_runtime_memory_summary(decode_file, &runtime_memory_summary) ||
        runtime_memory_summary.end_virtual_address != base_virtual_address + 0x3000u ||
        !machine_payload_decode_file_get_current_segment_summary(decode_file, &current_segment_summary) ||
        !current_segment_summary.name || strcmp(current_segment_summary.name, ".text") != 0 ||
        !machine_payload_decode_file_get_fetch_summary(decode_file, &fetch_summary) ||
        fetch_summary.byte_virtual_address != base_virtual_address ||
        fetch_summary.byte_value != 0x1eu ||
        !machine_payload_decode_file_get_decode_tag_summary(decode_file, &decode_tag_summary) ||
        decode_tag_summary.tag_class != MACHINE_DECODE_TAG_OP ||
        decode_tag_summary.raw_byte != 0x1eu ||
        decode_tag_summary.tag_value != 0x0eu ||
        !decode_tag_summary.is_known ||
        !decode_tag_summary.tag_name || strcmp(decode_tag_summary.tag_name, "load-local") != 0 ||
        !machine_payload_decode_file_get_payload_summary(decode_file, &payload_summary) ||
        payload_summary.kind != MACHINE_PAYLOAD_DECODE_KIND_OP ||
        payload_summary.raw_byte != 0x1eu ||
        payload_summary.tag_value != 0x0eu ||
        !payload_summary.is_known ||
        !payload_summary.tag_name || strcmp(payload_summary.tag_name, "load-local") != 0 ||
        payload_summary.total_byte_count != 1u ||
        payload_summary.payload_byte_count != 0u ||
        payload_summary.immediate_value != 0u ||
        !machine_payload_decode_dump_file(decode_file, &dump_text, &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: %s validation mismatch: %s\n",
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

static int verify_payload_decode_report_with_profile(const MachinePayloadDecodeReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    size_t base_virtual_address) {
    MachinePayloadDecodeError decode_error;
    MachinePayloadDecodeReportOverviewArtifact overview_artifact;
    const MachinePayloadDecodeFile *payload_decode_file = NULL;
    const MachineDecodeFile *decode_file = NULL;
    const MachinePayloadDecodeHeaderSummary *header_summary = NULL;
    const MachinePayloadDecodeTargetPolicySummary *target_policy_summary = NULL;
    const MachineRuntimeLaunchSummary *runtime_launch_summary = NULL;
    const MachineRuntimeInitialStackSummary *initial_stack_summary = NULL;
    const MachineRuntimeMemorySummary *runtime_memory_summary = NULL;
    const MachineRuntimeSegmentSummary *current_segment_summary = NULL;
    const MachineStepFetchSummary *fetch_summary = NULL;
    const MachineDecodeTagSummary *decode_tag_summary = NULL;
    const MachinePayloadDecodeSummary *payload_summary = NULL;
    const MachineElfArtifactSummary *source_artifact_summary = NULL;
    char *dump_text = NULL;
    char expected_dump[2048];
    int ok = 1;

    memset(&decode_error, 0, sizeof(decode_error));
    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(expected_dump, 0, sizeof(expected_dump));

    if (snprintf(expected_dump, sizeof(expected_dump),
            "machine_payload_decode profile=%s elf_origin=%s elf_semantics=%s status=ready pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n"
            "payload: kind=op raw=0x1e value=0x0e known=yes name=load-local total-bytes=1 payload-bytes=0 imm=0 bytes=[]\n"
            "report_overview:\n"
            "  status=ready current-segment=0 mapped-bytes=8192 pc=0x%zx sp=0x%zx\n"
            "  elf_source: target=%s origin=%s semantics=%s\n"
            "  policy: profile=%s max-payload=3\n"
            "  payload: kind=op raw=0x1e value=0x0e known=yes name=load-local total-bytes=1 payload-bytes=0 imm=0 bytes=[]\n",
            machine_elf_target_profile_name(profile),
            machine_elf_target_profile_name(origin_profile),
            machine_elf_relocation_semantics_name(semantics),
            base_virtual_address,
            base_virtual_address + 0x3000u,
            (size_t)0u,
            (size_t)8192u,
            base_virtual_address,
            base_virtual_address + 0x3000u,
            machine_elf_target_profile_name(profile),
            machine_elf_target_profile_name(origin_profile),
            machine_elf_relocation_semantics_name(semantics),
            machine_elf_target_profile_name(profile)) <= 0) {
        return 0;
    }

    if (!machine_payload_decode_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_payload_decode_report_get_file(report, &payload_decode_file) || !payload_decode_file ||
        !machine_payload_decode_report_get_decode_file(report, &decode_file) || !decode_file ||
        !machine_payload_decode_report_get_source_elf_artifact_summary_artifact(report, &source_artifact_summary) ||
        !source_artifact_summary ||
        !machine_payload_decode_report_get_header_summary_artifact(report, &header_summary) || !header_summary ||
        header_summary->target_profile != profile ||
        header_summary->step_status != MACHINE_STEP_STATUS_READY ||
        source_artifact_summary->target_profile != profile ||
        source_artifact_summary->origin_profile != origin_profile ||
        source_artifact_summary->relocation_semantics != semantics ||
        !machine_payload_decode_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary || target_policy_summary->max_payload_byte_count != 3u ||
        !machine_payload_decode_report_get_runtime_launch_summary_artifact(report, &runtime_launch_summary) ||
        !runtime_launch_summary || runtime_launch_summary->stack_segment_index != 1u ||
        !machine_payload_decode_report_get_initial_stack_summary_artifact(report, &initial_stack_summary) ||
        !initial_stack_summary || initial_stack_summary->argc != 0u ||
        !machine_payload_decode_report_get_runtime_memory_summary_artifact(report, &runtime_memory_summary) ||
        !runtime_memory_summary || runtime_memory_summary->mapped_byte_count != 8192u ||
        !machine_payload_decode_report_get_current_segment_summary_artifact(report, &current_segment_summary) ||
        !current_segment_summary || strcmp(current_segment_summary->name, ".text") != 0 ||
        !machine_payload_decode_report_get_fetch_summary_artifact(report, &fetch_summary) ||
        !fetch_summary || fetch_summary->byte_value != 0x1eu ||
        !machine_payload_decode_report_get_decode_tag_summary_artifact(report, &decode_tag_summary) ||
        !decode_tag_summary || strcmp(decode_tag_summary->tag_name, "load-local") != 0 ||
        !machine_payload_decode_report_get_payload_summary_artifact(report, &payload_summary) ||
        !payload_summary || payload_summary->payload_byte_count != 0u ||
        payload_summary->immediate_value != 0u ||
        !machine_payload_decode_report_overview_artifact_get_runtime_launch_summary_artifact(&overview_artifact, &runtime_launch_summary) ||
        !runtime_launch_summary || runtime_launch_summary->entry_virtual_address != base_virtual_address ||
        !machine_payload_decode_report_overview_artifact_get_initial_stack_summary_artifact(&overview_artifact, &initial_stack_summary) ||
        !initial_stack_summary || initial_stack_summary->image_byte_count != 20u ||
        !machine_payload_decode_report_overview_artifact_get_runtime_memory_summary_artifact(&overview_artifact, &runtime_memory_summary) ||
        !runtime_memory_summary || runtime_memory_summary->entry_offset != 0u ||
        !machine_payload_decode_report_overview_artifact_get_current_segment_summary_artifact(&overview_artifact, &current_segment_summary) ||
        !current_segment_summary || !current_segment_summary->executable ||
        !machine_payload_decode_report_overview_artifact_get_fetch_summary_artifact(&overview_artifact, &fetch_summary) ||
        !fetch_summary || fetch_summary->segment_index != 0u ||
        !machine_payload_decode_report_overview_artifact_get_decode_tag_summary_artifact(&overview_artifact, &decode_tag_summary) ||
        !decode_tag_summary || decode_tag_summary->tag_class != MACHINE_DECODE_TAG_OP ||
        !machine_payload_decode_report_overview_artifact_get_payload_summary_artifact(&overview_artifact, &payload_summary) ||
        !payload_summary || payload_summary->kind != MACHINE_PAYLOAD_DECODE_KIND_OP ||
        !machine_payload_decode_dump_report(report, &dump_text, &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: %s report mismatch: %s\n",
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

static int test_machine_payload_decode_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineStepReport step_report;
    MachineDecodeFile decode_file;
    MachineDecodeReport decode_report;
    MachinePayloadDecodeFile payload_decode_file;
    MachinePayloadDecodeFile cloned_payload_decode_file;
    MachinePayloadDecodeReport payload_decode_report;
    MachinePayloadDecodeError decode_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_step_report_init(&step_report);
    machine_decode_file_init(&decode_file);
    machine_decode_report_init(&decode_report);
    machine_payload_decode_file_init(&payload_decode_file);
    machine_payload_decode_file_init(&cloned_payload_decode_file);
    machine_payload_decode_report_init(&payload_decode_report);
    memset(&decode_error, 0, sizeof(decode_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL) ||
        !machine_step_build_report_from_machine_ir_report(&ir_report, &step_report, NULL) ||
        !machine_decode_build_from_machine_step_file(&step_file, &decode_file, NULL) ||
        !machine_decode_build_report_from_machine_step_file(&step_file, &decode_report, NULL) ||
        !machine_payload_decode_build_from_machine_decode_file(&decode_file, &payload_decode_file, &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: mainline setup failed: ir=%s decode=%s\n",
            ir_error.message,
            decode_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_payload_decode_file_with_profile(
        &payload_decode_file,
        "payload-decode-generic-decode-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    if (!machine_payload_decode_clone_file(
            &payload_decode_file, &cloned_payload_decode_file, &decode_error) ||
        cloned_payload_decode_file.decode_file.step_file.launch_file.registers ==
            payload_decode_file.decode_file.step_file.launch_file.registers) {
        fprintf(stderr, "[machine-payload-decode] FAIL: clone failed: %s\n", decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_payload_decode_file_with_profile(
        &cloned_payload_decode_file,
        "payload-decode-generic-clone",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_payload_decode_file_free(&payload_decode_file);
    if (!machine_payload_decode_build_from_machine_decode_report(
            &decode_report, &payload_decode_file, &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: build from decode report failed: %s\n",
            decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_payload_decode_file_with_profile(
        &payload_decode_file,
        "payload-decode-generic-decode-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_payload_decode_file_free(&payload_decode_file);
    if (!machine_payload_decode_build_from_machine_step_file(
            &step_file, &payload_decode_file, &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: build from step file failed: %s\n",
            decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_payload_decode_file_with_profile(
        &payload_decode_file,
        "payload-decode-generic-step-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_payload_decode_file_free(&payload_decode_file);
    if (!machine_payload_decode_build_from_machine_step_report(
            &step_report, &payload_decode_file, &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: build from step report failed: %s\n",
            decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_payload_decode_file_with_profile(
        &payload_decode_file,
        "payload-decode-generic-step-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    if (!machine_payload_decode_build_report_from_machine_decode_file(
            &decode_file, &payload_decode_report, &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: report from decode file failed: %s\n",
            decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_payload_decode_report_with_profile(
        &payload_decode_report,
        "payload-decode-generic-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    payload_decode_report.header_summary.program_counter = 0u;
    payload_decode_report.payload_summary.raw_byte = 0u;
    if (!machine_payload_decode_report_refresh(&payload_decode_report, &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: report refresh failed: %s\n",
            decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_payload_decode_report_with_profile(
        &payload_decode_report,
        "payload-decode-generic-refreshed-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_payload_decode_report_free(&payload_decode_report);
    if (!machine_payload_decode_build_report_from_machine_step_report(
            &step_report, &payload_decode_report, &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: report from step report failed: %s\n",
            decode_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_payload_decode_report_with_profile(
        &payload_decode_report,
        "payload-decode-generic-step-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

cleanup:
    machine_payload_decode_report_free(&payload_decode_report);
    machine_payload_decode_file_free(&cloned_payload_decode_file);
    machine_payload_decode_file_free(&payload_decode_file);
    machine_decode_report_free(&decode_report);
    machine_decode_file_free(&decode_file);
    machine_step_report_free(&step_report);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_payload_decode_ir_bridge_and_profile(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachinePayloadDecodeFile payload_decode_file;
    MachinePayloadDecodeReport payload_decode_report;
    MachinePayloadDecodeError decode_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_payload_decode_file_init(&payload_decode_file);
    machine_payload_decode_report_init(&payload_decode_report);
    memset(&decode_error, 0, sizeof(decode_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_payload_decode_build_from_machine_ir_report(
            &ir_report, &payload_decode_file, &decode_error) ||
        !machine_payload_decode_build_report_from_machine_ir_report(
            &ir_report, &payload_decode_report, &decode_error) ||
        !machine_payload_decode_build_dump_from_machine_ir_report(
            &ir_report, &dump_text, &decode_error) ||
        !machine_payload_decode_build_report_dump_from_machine_ir_report(
            &ir_report, &report_dump_text, &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: ir bridge setup failed: ir=%s decode=%s\n",
            ir_error.message,
            decode_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_payload_decode_file_with_profile(
        &payload_decode_file,
        "payload-decode-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    ok &= verify_payload_decode_report_with_profile(
        &payload_decode_report,
        "payload-decode-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    ok &= expect_text("payload decode ir dump wrapper", dump_text,
        "machine_payload_decode profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "payload: kind=op raw=0x1e value=0x0e known=yes name=load-local total-bytes=1 payload-bytes=0 imm=0 bytes=[]\n");
    ok &= expect_text("payload decode ir report-dump wrapper", report_dump_text,
        "machine_payload_decode profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "payload: kind=op raw=0x1e value=0x0e known=yes name=load-local total-bytes=1 payload-bytes=0 imm=0 bytes=[]\n"
        "report_overview:\n"
        "  status=ready current-segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000\n"
        "  elf_source: target=generic-elf32 origin=generic-elf32 semantics=direct-patch-spans\n"
        "  policy: profile=generic-elf32 max-payload=3\n"
        "  payload: kind=op raw=0x1e value=0x0e known=yes name=load-local total-bytes=1 payload-bytes=0 imm=0 bytes=[]\n");

    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    machine_payload_decode_report_free(&payload_decode_report);
    machine_payload_decode_file_free(&payload_decode_file);
    if (!machine_payload_decode_build_from_machine_ir_report_with_profile(
            &ir_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &payload_decode_file,
            &decode_error) ||
        !machine_payload_decode_build_report_from_machine_ir_report_with_profile(
            &ir_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &payload_decode_report,
            &decode_error) ||
        !machine_payload_decode_build_dump_from_machine_ir_report_with_profile(
            &ir_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &dump_text,
            &decode_error) ||
        !machine_payload_decode_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &report_dump_text,
            &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: i386 bridge setup failed: ir=%s decode=%s\n",
            ir_error.message,
            decode_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_payload_decode_file_with_profile(
        &payload_decode_file,
        "payload-decode-i386-ir-file",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);
    ok &= verify_payload_decode_report_with_profile(
        &payload_decode_report,
        "payload-decode-i386-ir-report",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);
    ok &= expect_text("payload decode i386 dump wrapper", dump_text,
        "machine_payload_decode profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans status=ready pc=0x8048000 sp=0x804b000 current_segment=0 mapped_bytes=8192\n"
        "payload: kind=op raw=0x1e value=0x0e known=yes name=load-local total-bytes=1 payload-bytes=0 imm=0 bytes=[]\n");
    ok &= expect_text("payload decode i386 report-dump wrapper", report_dump_text,
        "machine_payload_decode profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans status=ready pc=0x8048000 sp=0x804b000 current_segment=0 mapped_bytes=8192\n"
        "payload: kind=op raw=0x1e value=0x0e known=yes name=load-local total-bytes=1 payload-bytes=0 imm=0 bytes=[]\n"
        "report_overview:\n"
        "  status=ready current-segment=0 mapped-bytes=8192 pc=0x8048000 sp=0x804b000\n"
        "  elf_source: target=i386-preview origin=i386-preview semantics=direct-patch-spans\n"
        "  policy: profile=i386-preview max-payload=3\n"
        "  payload: kind=op raw=0x1e value=0x0e known=yes name=load-local total-bytes=1 payload-bytes=0 imm=0 bytes=[]\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_payload_decode_report_free(&payload_decode_report);
    machine_payload_decode_file_free(&payload_decode_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_payload_decode_recognizes_indirect_memory_op_tags(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineDecodeFile decode_file;
    MachinePayloadDecodeFile payload_decode_file;
    MachinePayloadDecodeSummary payload_summary;
    MachinePayloadDecodeError decode_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_decode_file_init(&decode_file);
    machine_payload_decode_file_init(&payload_decode_file);
    memset(&payload_summary, 0, sizeof(payload_summary));
    memset(&decode_error, 0, sizeof(decode_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL) ||
        !overwrite_step_tag_byte(&step_file, (unsigned char)(0x10u + MACHINE_SELECT_OP_LOAD_INDIRECT)) ||
        !machine_decode_build_from_machine_step_file(&step_file, &decode_file, NULL) ||
        !machine_payload_decode_build_from_machine_decode_file(&decode_file, &payload_decode_file, &decode_error) ||
        !machine_payload_decode_file_get_payload_summary(&payload_decode_file, &payload_summary) ||
        payload_summary.kind != MACHINE_PAYLOAD_DECODE_KIND_OP ||
        !payload_summary.is_known ||
        !payload_summary.tag_name || strcmp(payload_summary.tag_name, "load-indirect") != 0) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: load-indirect payload recognition mismatch: ir=%s decode=%s\n",
            ir_error.message,
            decode_error.message);
        ok = 0;
        goto cleanup;
    }

    machine_payload_decode_file_free(&payload_decode_file);
    machine_decode_file_free(&decode_file);
    if (!overwrite_step_tag_byte(&step_file, (unsigned char)(0x10u + MACHINE_SELECT_OP_STORE_INDIRECT)) ||
        !machine_decode_build_from_machine_step_file(&step_file, &decode_file, NULL) ||
        !machine_payload_decode_build_from_machine_decode_file(&decode_file, &payload_decode_file, &decode_error) ||
        !machine_payload_decode_file_get_payload_summary(&payload_decode_file, &payload_summary) ||
        payload_summary.kind != MACHINE_PAYLOAD_DECODE_KIND_OP ||
        !payload_summary.is_known ||
        !payload_summary.tag_name || strcmp(payload_summary.tag_name, "store-indirect") != 0) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: store-indirect payload recognition mismatch: %s\n",
            decode_error.message);
        ok = 0;
    }

cleanup:
    machine_payload_decode_file_free(&payload_decode_file);
    machine_decode_file_free(&decode_file);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_payload_decode_rejects_wrapped_payload_window(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineDecodeFile decode_file;
    MachinePayloadDecodeFile payload_decode_file;
    MachinePayloadDecodeError decode_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_decode_file_init(&decode_file);
    machine_payload_decode_file_init(&payload_decode_file);
    memset(&decode_error, 0, sizeof(decode_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL) ||
        !overwrite_step_tag_byte(&step_file, (unsigned char)(0x80u + MACHINE_LAYOUT_TERM_COMPARE_BRANCH)) ||
        !machine_decode_build_from_machine_step_file(&step_file, &decode_file, NULL) ||
        !machine_payload_decode_build_from_machine_decode_file(&decode_file, &payload_decode_file, &decode_error)) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: wrapped-window setup failed: ir=%s decode=%s\n",
            ir_error.message,
            decode_error.message);
        ok = 0;
        goto cleanup;
    }

    payload_decode_file.decode_file.step_file.status = MACHINE_STEP_STATUS_HALTED;
    payload_decode_file.decode_file.step_file.current_byte_memory_offset = (size_t)-2;
    if (machine_payload_decode_verify_file(&payload_decode_file, &decode_error) ||
        strstr(decode_error.message, "read past mapped memory") == NULL) {
        fprintf(stderr,
            "[machine-payload-decode] FAIL: wrapped-window rejection mismatch: %s\n",
            decode_error.message);
        ok = 0;
    }

cleanup:
    machine_payload_decode_file_free(&payload_decode_file);
    machine_decode_file_free(&decode_file);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_payload_decode_mainline();
    ok &= test_machine_payload_decode_ir_bridge_and_profile();
    ok &= test_machine_payload_decode_recognizes_indirect_memory_op_tags();
    ok &= test_machine_payload_decode_rejects_wrapped_payload_window();
    return ok ? 0 : 1;
}
