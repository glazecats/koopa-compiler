#include "machine/apply.h"

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
            "[machine-apply] FAIL: %s mismatch\nactual:\n%s\nexpected:\n%s\n",
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
    size_t payload_byte_count,
    int has_next_byte,
    unsigned char next_byte) {
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
    if (has_next_byte) {
        size_t next_index = 1u + payload_byte_count;
        if (runtime_file->segments[0].byte_count <= next_index ||
            load_file->segments[0].memory_byte_count <= next_index ||
            image_file->byte_count <= next_index) {
            return 0;
        }
        runtime_file->segments[0].bytes[next_index] = next_byte;
        load_file->segments[0].bytes[next_index] = next_byte;
        image_file->bytes[next_index] = next_byte;
    }
    return 1;
}

static int verify_apply_file(const MachineApplyFile *apply_file,
    const char *context,
    MachineElfTargetProfile profile,
    MachineApplyResolutionKind resolution_kind,
    MachineApplyKind apply_kind,
    int has_applied_state,
    int has_preview_state,
    int has_immediate_hint,
    size_t immediate_hint,
    const char *expected_dump) {
    MachineApplyHeaderSummary header_summary;
    MachineApplyTargetPolicySummary target_policy_summary;
    MachineApplySummary apply_summary;
    MachineApplyError apply_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&apply_summary, 0, sizeof(apply_summary));
    memset(&apply_error, 0, sizeof(apply_error));

    if (!machine_apply_verify_file(apply_file, &apply_error) ||
        !machine_apply_file_get_header_summary(apply_file, &header_summary) ||
        header_summary.target_profile != profile ||
        !machine_apply_file_get_target_policy_summary(apply_file, &target_policy_summary) ||
        !target_policy_summary.surfaces_state_preview ||
        !machine_apply_file_get_apply_summary(apply_file, &apply_summary) ||
        apply_summary.resolution_kind != resolution_kind ||
        apply_summary.apply_kind != apply_kind ||
        apply_summary.has_applied_state != has_applied_state ||
        apply_summary.has_preview_state != has_preview_state ||
        apply_summary.has_immediate_hint != has_immediate_hint ||
        (has_immediate_hint && apply_summary.immediate_hint != immediate_hint) ||
        !machine_apply_dump_file(apply_file, &dump_text, &apply_error)) {
        fprintf(stderr, "[machine-apply] FAIL: %s validation mismatch: %s\n", context, apply_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_apply_report(const MachineApplyReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    MachineApplyResolutionKind resolution_kind,
    MachineApplyKind apply_kind,
    const char *expected_dump) {
    MachineApplyReportOverviewArtifact overview_artifact;
    const MachineApplyFile *apply_file = NULL;
    const MachineCommitReport *commit_report = NULL;
    const MachineApplyHeaderSummary *header_summary = NULL;
    const MachineApplyTargetPolicySummary *target_policy_summary = NULL;
    const MachineApplySummary *apply_summary = NULL;
    MachineApplyError apply_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&apply_error, 0, sizeof(apply_error));

    if (!machine_apply_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_apply_report_get_file(report, &apply_file) || !apply_file ||
        !machine_apply_report_get_commit_report(report, &commit_report) || !commit_report ||
        !machine_apply_report_get_header_summary_artifact(report, &header_summary) || !header_summary ||
        header_summary->target_profile != profile ||
        !machine_apply_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary || !target_policy_summary->surfaces_call_application ||
        !machine_apply_report_get_apply_summary_artifact(report, &apply_summary) ||
        !apply_summary ||
        apply_summary->resolution_kind != resolution_kind ||
        apply_summary->apply_kind != apply_kind ||
        !machine_apply_report_overview_artifact_get_apply_summary_artifact(
            &overview_artifact, &apply_summary) ||
        !apply_summary ||
        !machine_apply_dump_report(report, &dump_text, &apply_error)) {
        fprintf(stderr, "[machine-apply] FAIL: %s report mismatch: %s\n", context, apply_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_apply_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineApplyFile apply_file;
    MachineApplyFile cloned_apply_file;
    MachineApplyReport apply_report;
    MachineApplyError apply_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_apply_file_init(&apply_file);
    machine_apply_file_init(&cloned_apply_file);
    machine_apply_report_init(&apply_report);
    memset(&apply_error, 0, sizeof(apply_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_apply_build_from_machine_ir_report(&ir_report, &apply_file, &apply_error) ||
        !machine_apply_build_report_from_machine_ir_report(&ir_report, &apply_report, &apply_error)) {
        fprintf(stderr,
            "[machine-apply] FAIL: mainline setup failed: ir=%s apply=%s\n",
            ir_error.message,
            apply_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_apply_file(
        &apply_file,
        "apply-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_APPLY_RESOLUTION_PENDING_REGISTER_APPLICATION,
        MACHINE_APPLY_KIND_REGISTER,
        0,
        1,
        0,
        0u,
        "machine_apply profile=generic-elf32 commit=deferred-register-commit origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "apply: resolution=pending-register-application kind=register commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- has-applied-state=no applied-status=- applied-pc=- applied-segment=- applied-byte=- has-preview-state=yes preview-status=ready preview-pc=0x1001 preview-segment=0 preview-byte=0x8a targets=[] return-imm=-\n");

    if (!machine_apply_clone_file(&apply_file, &cloned_apply_file, &apply_error) ||
        cloned_apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers ==
            apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers) {
        fprintf(stderr, "[machine-apply] FAIL: clone failed: %s\n", apply_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_apply_report(
        &apply_report,
        "apply-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_APPLY_RESOLUTION_PENDING_REGISTER_APPLICATION,
        MACHINE_APPLY_KIND_REGISTER,
        "machine_apply profile=generic-elf32 commit=deferred-register-commit origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "apply: resolution=pending-register-application kind=register commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- has-applied-state=no applied-status=- applied-pc=- applied-segment=- applied-byte=- has-preview-state=yes preview-status=ready preview-pc=0x1001 preview-segment=0 preview-byte=0x8a targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: commit=deferred-register-commit status=ready segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000\n"
        "  policy: profile=generic-elf32 state=yes register=yes slot=yes call=yes preview=yes\n"
        "  apply: resolution=pending-register-application kind=register applied=no preview=yes imm=- apc=- ppc=0x1001 targets=[] return-imm=-\n");

    apply_report.header_summary.origin_program_counter = 0u;
    if (!machine_apply_report_refresh(&apply_report, &apply_error)) {
        fprintf(stderr, "[machine-apply] FAIL: report refresh failed: %s\n", apply_error.message);
        ok = 0;
        goto cleanup;
    }

cleanup:
    machine_apply_report_free(&apply_report);
    machine_apply_file_free(&cloned_apply_file);
    machine_apply_file_free(&apply_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_apply_custom_step_cases(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineApplyFile apply_file;
    MachineApplyReport apply_report;
    MachineApplyError apply_error;
    static const unsigned char return_imm_payload[] = {0x17u};
    static const unsigned char jump_payload[] = {0x01u};
    static const unsigned char store_local_imm_payload[] = {0x07u};
    static const unsigned char store_global_imm_payload[] = {0x05u};
    static const unsigned char call_void_imm_payload[] = {0x02u};
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_apply_file_init(&apply_file);
    machine_apply_report_init(&apply_report);
    memset(&apply_error, 0, sizeof(apply_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL)) {
        fprintf(stderr, "[machine-apply] FAIL: custom step setup failed: %s\n", ir_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!overwrite_step_bytes(&step_file, 0x81u, return_imm_payload, 1u, 0, 0u) ||
        !machine_apply_build_from_machine_step_file(&step_file, &apply_file, &apply_error) ||
        !machine_apply_build_report_from_machine_step_file(&step_file, &apply_report, &apply_error)) {
        fprintf(stderr, "[machine-apply] FAIL: halt setup failed: %s\n", apply_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_apply_file(
        &apply_file,
        "apply-halt-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_APPLY_RESOLUTION_APPLIED_STATE,
        MACHINE_APPLY_KIND_STATE,
        1,
        1,
        1,
        7u,
        "machine_apply profile=generic-elf32 commit=committed-state origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "apply: resolution=applied-state kind=state commit=committed-state writeback=committed-no-op mutation=no-mutation effect=control-only transition=halt action=halt raw=0x81 value=0x01 known=yes name=return-imm bytes=2 payload=[0x17] imm=7 has-applied-state=yes applied-status=halted applied-pc=0x1000 applied-segment=- applied-byte=- has-preview-state=yes preview-status=halted preview-pc=0x1000 preview-segment=- preview-byte=- targets=[] return-imm=7\n");

    machine_apply_report_free(&apply_report);
    machine_apply_file_free(&apply_file);

    if (!overwrite_step_bytes(&step_file, 0x84u, jump_payload, 1u, 0, 0u) ||
        !machine_apply_build_from_machine_step_file(&step_file, &apply_file, &apply_error) ||
        !machine_apply_build_report_from_machine_step_file(&step_file, &apply_report, &apply_error)) {
        fprintf(stderr, "[machine-apply] FAIL: jump setup failed: %s\n", apply_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_apply_file(
        &apply_file,
        "apply-jump-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_APPLY_RESOLUTION_BLOCKED_ON_CONTROL,
        MACHINE_APPLY_KIND_NONE,
        0,
        0,
        1,
        1u,
        "machine_apply profile=generic-elf32 commit=blocked-on-control origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "apply: resolution=blocked-on-control kind=none commit=blocked-on-control writeback=blocked-on-control mutation=blocked-on-control effect=control-only transition=deferred-control-transfer action=control-transfer raw=0x84 value=0x04 known=yes name=jump bytes=2 payload=[0x01] imm=1 has-applied-state=no applied-status=- applied-pc=- applied-segment=- applied-byte=- has-preview-state=no preview-status=- preview-pc=- preview-segment=- preview-byte=- targets=[1] return-imm=-\n");

    machine_apply_report_free(&apply_report);
    machine_apply_file_free(&apply_file);

    if (!overwrite_step_bytes(&step_file, 0x00u, NULL, 0u, 0, 0u) ||
        !machine_apply_build_from_machine_step_file(&step_file, &apply_file, &apply_error) ||
        !machine_apply_build_report_from_machine_step_file(&step_file, &apply_report, &apply_error)) {
        fprintf(stderr, "[machine-apply] FAIL: unsupported setup failed: %s\n", apply_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_apply_file(
        &apply_file,
        "apply-unsupported-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_APPLY_RESOLUTION_BLOCKED_UNSUPPORTED,
        MACHINE_APPLY_KIND_NONE,
        0,
        0,
        0,
        0u,
        "machine_apply profile=generic-elf32 commit=blocked-unsupported origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "apply: resolution=blocked-unsupported kind=none commit=blocked-unsupported writeback=blocked-unsupported mutation=blocked-unsupported effect=none transition=unsupported action=unsupported raw=0x00 value=0x00 known=no name=- bytes=1 payload=[] imm=- has-applied-state=no applied-status=- applied-pc=- applied-segment=- applied-byte=- has-preview-state=no preview-status=- preview-pc=- preview-segment=- preview-byte=- targets=[] return-imm=-\n");

    machine_apply_report_free(&apply_report);
    machine_apply_file_free(&apply_file);

    if (!overwrite_step_bytes(&step_file, 0x1eu, store_local_imm_payload, 1u, 1, 0xaau) ||
        !machine_apply_build_from_machine_step_file(&step_file, &apply_file, &apply_error)) {
        fprintf(stderr, "[machine-apply] FAIL: store-local-imm setup failed: %s\n", apply_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_apply_file(
        &apply_file,
        "apply-store-local-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_APPLY_RESOLUTION_PENDING_LOCAL_APPLICATION,
        MACHINE_APPLY_KIND_LOCAL_SLOT,
        0,
        1,
        1,
        7u,
        "machine_apply profile=generic-elf32 commit=deferred-local-commit origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "apply: resolution=pending-local-application kind=local-slot commit=deferred-local-commit writeback=deferred-local-writeback mutation=deferred-local-slot effect=local-slot transition=next-fetch action=advance raw=0x1e value=0x0e known=yes name=store-local-imm bytes=2 payload=[0x07] imm=7 has-applied-state=no applied-status=- applied-pc=- applied-segment=- applied-byte=- has-preview-state=yes preview-status=ready preview-pc=0x1002 preview-segment=0 preview-byte=0xaa targets=[] return-imm=-\n");

    machine_apply_file_free(&apply_file);

    if (!overwrite_step_bytes(&step_file, 0x21u, store_global_imm_payload, 1u, 1, 0xabu) ||
        !machine_apply_build_from_machine_step_file(&step_file, &apply_file, &apply_error)) {
        fprintf(stderr, "[machine-apply] FAIL: store-global-imm setup failed: %s\n", apply_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_apply_file(
        &apply_file,
        "apply-store-global-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_APPLY_RESOLUTION_PENDING_GLOBAL_APPLICATION,
        MACHINE_APPLY_KIND_GLOBAL_SLOT,
        0,
        1,
        1,
        5u,
        "machine_apply profile=generic-elf32 commit=deferred-global-commit origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "apply: resolution=pending-global-application kind=global-slot commit=deferred-global-commit writeback=deferred-global-writeback mutation=deferred-global-slot effect=global-slot transition=next-fetch action=advance raw=0x21 value=0x11 known=yes name=store-global-imm bytes=2 payload=[0x05] imm=5 has-applied-state=no applied-status=- applied-pc=- applied-segment=- applied-byte=- has-preview-state=yes preview-status=ready preview-pc=0x1002 preview-segment=0 preview-byte=0xab targets=[] return-imm=-\n");

    machine_apply_file_free(&apply_file);

    if (!overwrite_step_bytes(&step_file, 0x1bu, call_void_imm_payload, 1u, 1, 0xacu) ||
        !machine_apply_build_from_machine_step_file(&step_file, &apply_file, &apply_error)) {
        fprintf(stderr, "[machine-apply] FAIL: call-void-imm setup failed: %s\n", apply_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_apply_file(
        &apply_file,
        "apply-call-void-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_APPLY_RESOLUTION_PENDING_CALL_APPLICATION,
        MACHINE_APPLY_KIND_CALL,
        0,
        1,
        1,
        2u,
        "machine_apply profile=generic-elf32 commit=deferred-call-commit origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "apply: resolution=pending-call-application kind=call commit=deferred-call-commit writeback=deferred-call-writeback mutation=deferred-call-effect effect=call transition=next-fetch action=advance raw=0x1b value=0x0b known=yes name=call-void-imm bytes=2 payload=[0x02] imm=2 has-applied-state=no applied-status=- applied-pc=- applied-segment=- applied-byte=- has-preview-state=yes preview-status=ready preview-pc=0x1002 preview-segment=0 preview-byte=0xac targets=[] return-imm=-\n");

cleanup:
    machine_apply_report_free(&apply_report);
    machine_apply_file_free(&apply_file);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_apply_i386_bridge(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineApplyFile apply_file;
    MachineApplyReport apply_report;
    MachineApplyError apply_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_apply_file_init(&apply_file);
    machine_apply_report_init(&apply_report);
    memset(&apply_error, 0, sizeof(apply_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_apply_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &apply_file, &apply_error) ||
        !machine_apply_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &apply_report, &apply_error) ||
        !machine_apply_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &apply_error) ||
        !machine_apply_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &apply_error)) {
        fprintf(stderr, "[machine-apply] FAIL: i386 bridge setup failed: %s / %s\n",
            ir_error.message,
            apply_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text("apply i386 dump wrapper", dump_text,
        "machine_apply profile=i386-preview commit=deferred-register-commit origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192\n"
        "apply: resolution=pending-register-application kind=register commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- has-applied-state=no applied-status=- applied-pc=- applied-segment=- applied-byte=- has-preview-state=yes preview-status=ready preview-pc=0x8048001 preview-segment=0 preview-byte=0x8a targets=[] return-imm=-\n");
    ok &= expect_text("apply i386 report dump wrapper", report_dump_text,
        "machine_apply profile=i386-preview commit=deferred-register-commit origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192\n"
        "apply: resolution=pending-register-application kind=register commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- has-applied-state=no applied-status=- applied-pc=- applied-segment=- applied-byte=- has-preview-state=yes preview-status=ready preview-pc=0x8048001 preview-segment=0 preview-byte=0x8a targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: commit=deferred-register-commit status=ready segment=0 mapped-bytes=8192 pc=0x8048000 sp=0x804b000\n"
        "  policy: profile=i386-preview state=yes register=yes slot=yes call=yes preview=yes\n"
        "  apply: resolution=pending-register-application kind=register applied=no preview=yes imm=- apc=- ppc=0x8048001 targets=[] return-imm=-\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_apply_report_free(&apply_report);
    machine_apply_file_free(&apply_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_apply_mainline();
    ok &= test_machine_apply_custom_step_cases();
    ok &= test_machine_apply_i386_bridge();
    return ok ? 0 : 1;
}
