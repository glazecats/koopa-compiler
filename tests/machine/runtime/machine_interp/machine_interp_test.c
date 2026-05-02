#include "machine/interp.h"

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
            "[machine-interp] FAIL: %s mismatch\nactual:\n%s\nexpected:\n%s\n",
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

static int overwrite_step_bytes(MachineStepFile *step_file,
    unsigned char tag_byte,
    const unsigned char *payload_bytes,
    size_t payload_byte_count) {
    size_t byte_index;
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
    for (byte_index = 0u; byte_index < payload_byte_count; ++byte_index) {
        if (runtime_file->segments[0].byte_count <= 1u + byte_index ||
            load_file->segments[0].memory_byte_count <= 1u + byte_index ||
            image_file->byte_count <= 1u + byte_index) {
            return 0;
        }
        runtime_file->segments[0].bytes[1u + byte_index] = payload_bytes[byte_index];
        load_file->segments[0].bytes[1u + byte_index] = payload_bytes[byte_index];
        image_file->bytes[1u + byte_index] = payload_bytes[byte_index];
    }
    return 1;
}

static int verify_interp_file(const MachineInterpFile *interp_file,
    const char *context,
    MachineElfTargetProfile profile,
    size_t base_virtual_address,
    MachineInterpActionKind action_kind,
    MachineStepStatus next_status,
    int has_next_program_counter,
    size_t next_program_counter,
    int has_primary_target,
    size_t primary_target,
    int has_secondary_target,
    size_t secondary_target,
    int has_return_immediate,
    size_t return_immediate,
    const char *expected_dump) {
    MachineInterpHeaderSummary header_summary;
    MachineInterpTargetPolicySummary target_policy_summary;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineRuntimeSegmentSummary current_segment_summary;
    MachineStepFetchSummary fetch_summary;
    MachineDecodeTagSummary decode_tag_summary;
    MachinePayloadDecodeSummary payload_summary;
    MachineInterpSummary interp_summary;
    MachineInterpError interp_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&runtime_launch_summary, 0, sizeof(runtime_launch_summary));
    memset(&initial_stack_summary, 0, sizeof(initial_stack_summary));
    memset(&runtime_memory_summary, 0, sizeof(runtime_memory_summary));
    memset(&current_segment_summary, 0, sizeof(current_segment_summary));
    memset(&fetch_summary, 0, sizeof(fetch_summary));
    memset(&decode_tag_summary, 0, sizeof(decode_tag_summary));
    memset(&payload_summary, 0, sizeof(payload_summary));
    memset(&interp_summary, 0, sizeof(interp_summary));
    memset(&interp_error, 0, sizeof(interp_error));

    if (!machine_interp_verify_file(interp_file, &interp_error) ||
        !machine_interp_file_get_header_summary(interp_file, &header_summary) ||
        header_summary.target_profile != profile ||
        header_summary.program_counter != base_virtual_address ||
        !machine_interp_file_get_target_policy_summary(interp_file, &target_policy_summary) ||
        target_policy_summary.max_control_target_count != 2u ||
        !machine_interp_file_get_runtime_launch_summary(interp_file, &runtime_launch_summary) ||
        runtime_launch_summary.entry_virtual_address != base_virtual_address ||
        !machine_interp_file_get_initial_stack_summary(interp_file, &initial_stack_summary) ||
        initial_stack_summary.image_end_virtual_address != base_virtual_address + 0x3000u ||
        !machine_interp_file_get_runtime_memory_summary(interp_file, &runtime_memory_summary) ||
        runtime_memory_summary.base_virtual_address != base_virtual_address ||
        !machine_interp_file_get_current_segment_summary(interp_file, &current_segment_summary) ||
        strcmp(current_segment_summary.name, ".text") != 0 ||
        !machine_interp_file_get_fetch_summary(interp_file, &fetch_summary) ||
        fetch_summary.byte_virtual_address != base_virtual_address ||
        !machine_interp_file_get_decode_tag_summary(interp_file, &decode_tag_summary) ||
        !machine_interp_file_get_payload_summary(interp_file, &payload_summary) ||
        !machine_interp_file_get_interp_summary(interp_file, &interp_summary) ||
        interp_summary.action_kind != action_kind ||
        interp_summary.next_status != next_status ||
        interp_summary.has_next_program_counter != has_next_program_counter ||
        (has_next_program_counter && interp_summary.next_program_counter != next_program_counter) ||
        interp_summary.has_primary_target_block != has_primary_target ||
        (has_primary_target && interp_summary.primary_target_block_index != primary_target) ||
        interp_summary.has_secondary_target_block != has_secondary_target ||
        (has_secondary_target && interp_summary.secondary_target_block_index != secondary_target) ||
        interp_summary.has_return_immediate != has_return_immediate ||
        (has_return_immediate && interp_summary.return_immediate != return_immediate) ||
        !machine_interp_dump_file(interp_file, &dump_text, &interp_error)) {
        fprintf(stderr, "[machine-interp] FAIL: %s validation mismatch: %s\n", context, interp_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_interp_report(const MachineInterpReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    MachineInterpActionKind action_kind,
    int has_next_program_counter,
    size_t next_program_counter,
    int has_primary_target,
    size_t primary_target,
    int has_secondary_target,
    size_t secondary_target,
    int has_return_immediate,
    size_t return_immediate,
    const char *expected_dump) {
    MachineInterpReportOverviewArtifact overview_artifact;
    const MachineInterpFile *interp_file = NULL;
    const MachinePayloadDecodeFile *payload_decode_file = NULL;
    const MachineInterpHeaderSummary *header_summary = NULL;
    const MachineInterpTargetPolicySummary *target_policy_summary = NULL;
    const MachineInterpSummary *interp_summary = NULL;
    MachineInterpError interp_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&interp_error, 0, sizeof(interp_error));

    if (!machine_interp_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_interp_report_get_file(report, &interp_file) || !interp_file ||
        !machine_interp_report_get_payload_decode_file(report, &payload_decode_file) || !payload_decode_file ||
        !machine_interp_report_get_header_summary_artifact(report, &header_summary) || !header_summary ||
        header_summary->target_profile != profile ||
        !machine_interp_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary || !target_policy_summary->resolves_linear_next_program_counter ||
        !machine_interp_report_get_interp_summary_artifact(report, &interp_summary) || !interp_summary ||
        interp_summary->action_kind != action_kind ||
        interp_summary->has_next_program_counter != has_next_program_counter ||
        (has_next_program_counter && interp_summary->next_program_counter != next_program_counter) ||
        interp_summary->has_primary_target_block != has_primary_target ||
        (has_primary_target && interp_summary->primary_target_block_index != primary_target) ||
        interp_summary->has_secondary_target_block != has_secondary_target ||
        (has_secondary_target && interp_summary->secondary_target_block_index != secondary_target) ||
        interp_summary->has_return_immediate != has_return_immediate ||
        (has_return_immediate && interp_summary->return_immediate != return_immediate) ||
        !machine_interp_report_overview_artifact_get_interp_summary_artifact(&overview_artifact, &interp_summary) ||
        !interp_summary ||
        !machine_interp_dump_report(report, &dump_text, &interp_error)) {
        fprintf(stderr, "[machine-interp] FAIL: %s report mismatch: %s\n", context, interp_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_interp_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineInterpFile interp_file;
    MachineInterpFile cloned_interp_file;
    MachineInterpReport interp_report;
    MachineInterpError interp_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_interp_file_init(&interp_file);
    machine_interp_file_init(&cloned_interp_file);
    machine_interp_report_init(&interp_report);
    memset(&interp_error, 0, sizeof(interp_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_interp_build_from_machine_ir_report(&ir_report, &interp_file, &interp_error) ||
        !machine_interp_build_report_from_machine_ir_report(&ir_report, &interp_report, &interp_error)) {
        fprintf(stderr,
            "[machine-interp] FAIL: mainline setup failed: ir=%s interp=%s\n",
            ir_error.message,
            interp_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_interp_file(
        &interp_file,
        "interp-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u,
        MACHINE_INTERP_ACTION_ADVANCE,
        MACHINE_STEP_STATUS_READY,
        1,
        0x1001u,
        0,
        0u,
        0,
        0u,
        0,
        0u,
        "machine_interp profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "interp: action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 next-status=ready next-pc=0x1001 targets=[] return-imm=-\n");

    if (!machine_interp_clone_file(&interp_file, &cloned_interp_file, &interp_error) ||
        cloned_interp_file.payload_decode_file.decode_file.step_file.launch_file.registers ==
            interp_file.payload_decode_file.decode_file.step_file.launch_file.registers) {
        fprintf(stderr, "[machine-interp] FAIL: clone failed: %s\n", interp_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_interp_file(
        &cloned_interp_file,
        "interp-generic-clone",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u,
        MACHINE_INTERP_ACTION_ADVANCE,
        MACHINE_STEP_STATUS_READY,
        1,
        0x1001u,
        0,
        0u,
        0,
        0u,
        0,
        0u,
        "machine_interp profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "interp: action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 next-status=ready next-pc=0x1001 targets=[] return-imm=-\n");

    ok &= verify_interp_report(
        &interp_report,
        "interp-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_INTERP_ACTION_ADVANCE,
        1,
        0x1001u,
        0,
        0u,
        0,
        0u,
        0,
        0u,
        "machine_interp profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "interp: action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 next-status=ready next-pc=0x1001 targets=[] return-imm=-\n"
        "report_overview:\n"
        "  status=ready current-segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000\n"
        "  policy: profile=generic-elf32 targets=2 linear-next-pc=yes block-targets=yes\n"
        "  interp: action=advance next-status=ready next-pc=0x1001 targets=[] return-imm=-\n");

    interp_report.header_summary.program_counter = 0u;
    interp_report.interp_summary.next_program_counter = 0u;
    if (!machine_interp_report_refresh(&interp_report, &interp_error)) {
        fprintf(stderr, "[machine-interp] FAIL: report refresh failed: %s\n", interp_error.message);
        ok = 0;
        goto cleanup;
    }

cleanup:
    machine_interp_report_free(&interp_report);
    machine_interp_file_free(&cloned_interp_file);
    machine_interp_file_free(&interp_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_interp_custom_step_cases(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineStepReport step_report;
    MachineInterpFile interp_file;
    MachineInterpReport interp_report;
    MachineInterpError interp_error;
    static const unsigned char return_imm_payload[] = {0x17u};
    static const unsigned char jump_payload[] = {0x01u};
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_step_report_init(&step_report);
    machine_interp_file_init(&interp_file);
    machine_interp_report_init(&interp_report);
    memset(&interp_error, 0, sizeof(interp_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL)) {
        fprintf(stderr, "[machine-interp] FAIL: custom step setup failed: %s\n", ir_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!overwrite_step_bytes(&step_file, 0x81u, return_imm_payload, 1u) ||
        !machine_step_build_report_from_file(&step_file, &step_report, NULL) ||
        !machine_interp_build_from_machine_step_file(&step_file, &interp_file, &interp_error) ||
        !machine_interp_build_report_from_machine_step_file(&step_file, &interp_report, &interp_error)) {
        fprintf(stderr, "[machine-interp] FAIL: return-imm setup failed: %s\n", interp_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_interp_file(
        &interp_file,
        "interp-return-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u,
        MACHINE_INTERP_ACTION_HALT,
        MACHINE_STEP_STATUS_HALTED,
        0,
        0u,
        0,
        0u,
        0,
        0u,
        1,
        7u,
        "machine_interp profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "interp: action=halt raw=0x81 value=0x01 known=yes name=return-imm bytes=2 next-status=halted next-pc=- targets=[] return-imm=7\n");

    ok &= verify_interp_report(
        &interp_report,
        "interp-return-imm-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_INTERP_ACTION_HALT,
        0,
        0u,
        0,
        0u,
        0,
        0u,
        1,
        7u,
        "machine_interp profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "interp: action=halt raw=0x81 value=0x01 known=yes name=return-imm bytes=2 next-status=halted next-pc=- targets=[] return-imm=7\n"
        "report_overview:\n"
        "  status=ready current-segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000\n"
        "  policy: profile=generic-elf32 targets=2 linear-next-pc=yes block-targets=yes\n"
        "  interp: action=halt next-status=halted next-pc=- targets=[] return-imm=7\n");

    machine_interp_report_free(&interp_report);
    machine_interp_file_free(&interp_file);
    machine_step_report_free(&step_report);
    machine_step_report_init(&step_report);

    if (!overwrite_step_bytes(&step_file, 0x84u, jump_payload, 1u) ||
        !machine_step_build_report_from_file(&step_file, &step_report, NULL) ||
        !machine_interp_build_from_machine_step_report(&step_report, &interp_file, &interp_error) ||
        !machine_interp_build_report_from_machine_step_report(&step_report, &interp_report, &interp_error)) {
        fprintf(stderr, "[machine-interp] FAIL: jump setup failed: %s\n", interp_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_interp_file(
        &interp_file,
        "interp-jump-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u,
        MACHINE_INTERP_ACTION_CONTROL_TRANSFER,
        MACHINE_STEP_STATUS_READY,
        0,
        0u,
        1,
        1u,
        0,
        0u,
        0,
        0u,
        "machine_interp profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "interp: action=control-transfer raw=0x84 value=0x04 known=yes name=jump bytes=2 next-status=ready next-pc=- targets=[1] return-imm=-\n");

    ok &= verify_interp_report(
        &interp_report,
        "interp-jump-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_INTERP_ACTION_CONTROL_TRANSFER,
        0,
        0u,
        1,
        1u,
        0,
        0u,
        0,
        0u,
        "machine_interp profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "interp: action=control-transfer raw=0x84 value=0x04 known=yes name=jump bytes=2 next-status=ready next-pc=- targets=[1] return-imm=-\n"
        "report_overview:\n"
        "  status=ready current-segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000\n"
        "  policy: profile=generic-elf32 targets=2 linear-next-pc=yes block-targets=yes\n"
        "  interp: action=control-transfer next-status=ready next-pc=- targets=[1] return-imm=-\n");

    machine_interp_report_free(&interp_report);
    machine_interp_file_free(&interp_file);
    machine_step_report_free(&step_report);
    machine_step_report_init(&step_report);

    if (!overwrite_step_bytes(&step_file, 0x00u, NULL, 0u) ||
        !machine_step_build_report_from_file(&step_file, &step_report, NULL) ||
        !machine_interp_build_from_machine_step_file(&step_file, &interp_file, &interp_error) ||
        !machine_interp_build_report_from_machine_step_file(&step_file, &interp_report, &interp_error)) {
        fprintf(stderr, "[machine-interp] FAIL: unsupported setup failed: %s\n", interp_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_interp_file(
        &interp_file,
        "interp-unsupported-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        0x1000u,
        MACHINE_INTERP_ACTION_UNSUPPORTED,
        MACHINE_STEP_STATUS_READY,
        0,
        0u,
        0,
        0u,
        0,
        0u,
        0,
        0u,
        "machine_interp profile=generic-elf32 status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "interp: action=unsupported raw=0x00 value=0x00 known=no name=- bytes=1 next-status=ready next-pc=- targets=[] return-imm=-\n");

cleanup:
    machine_interp_report_free(&interp_report);
    machine_interp_file_free(&interp_file);
    machine_step_report_free(&step_report);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_interp_i386_bridge(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineInterpFile interp_file;
    MachineInterpReport interp_report;
    MachineInterpError interp_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_interp_file_init(&interp_file);
    machine_interp_report_init(&interp_report);
    memset(&interp_error, 0, sizeof(interp_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_interp_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &interp_file, &interp_error) ||
        !machine_interp_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &interp_report, &interp_error) ||
        !machine_interp_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &interp_error) ||
        !machine_interp_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &interp_error)) {
        fprintf(stderr, "[machine-interp] FAIL: i386 bridge setup failed: %s / %s\n",
            ir_error.message,
            interp_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_interp_file(
        &interp_file,
        "interp-i386-ir-file",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        0x08048000u,
        MACHINE_INTERP_ACTION_ADVANCE,
        MACHINE_STEP_STATUS_READY,
        1,
        0x08048001u,
        0,
        0u,
        0,
        0u,
        0,
        0u,
        "machine_interp profile=i386-preview status=ready pc=0x8048000 sp=0x804b000 current_segment=0 mapped_bytes=8192\n"
        "interp: action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 next-status=ready next-pc=0x8048001 targets=[] return-imm=-\n");

    ok &= expect_text("interp i386 dump wrapper", dump_text,
        "machine_interp profile=i386-preview status=ready pc=0x8048000 sp=0x804b000 current_segment=0 mapped_bytes=8192\n"
        "interp: action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 next-status=ready next-pc=0x8048001 targets=[] return-imm=-\n");
    ok &= expect_text("interp i386 report dump wrapper", report_dump_text,
        "machine_interp profile=i386-preview status=ready pc=0x8048000 sp=0x804b000 current_segment=0 mapped_bytes=8192\n"
        "interp: action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 next-status=ready next-pc=0x8048001 targets=[] return-imm=-\n"
        "report_overview:\n"
        "  status=ready current-segment=0 mapped-bytes=8192 pc=0x8048000 sp=0x804b000\n"
        "  policy: profile=i386-preview targets=2 linear-next-pc=yes block-targets=yes\n"
        "  interp: action=advance next-status=ready next-pc=0x8048001 targets=[] return-imm=-\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_interp_report_free(&interp_report);
    machine_interp_file_free(&interp_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_interp_mainline();
    ok &= test_machine_interp_custom_step_cases();
    ok &= test_machine_interp_i386_bridge();
    return ok ? 0 : 1;
}
