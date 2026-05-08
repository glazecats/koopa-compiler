#include "machine/observe.h"

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
            "[machine-observe] FAIL: %s mismatch\nactual:\n%s\nexpected:\n%s\n",
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

static int verify_observe_file(const MachineObserveFile *observe_file,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    MachineObserveResolutionKind resolution_kind,
    int is_exact_state,
    int has_observed_state,
    const char *expected_dump) {
    MachineObserveHeaderSummary header_summary;
    MachineObserveTargetPolicySummary target_policy_summary;
    MachineObserveSummary observe_summary;
    MachineElfArtifactSummary source_artifact_summary;
    MachineObserveError observe_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&observe_summary, 0, sizeof(observe_summary));
    memset(&source_artifact_summary, 0, sizeof(source_artifact_summary));
    memset(&observe_error, 0, sizeof(observe_error));

    if (!machine_observe_verify_file(observe_file, &observe_error) ||
        !machine_observe_file_get_source_elf_artifact_summary(observe_file, &source_artifact_summary) ||
        source_artifact_summary.target_profile != profile ||
        source_artifact_summary.origin_profile != origin_profile ||
        source_artifact_summary.relocation_semantics != semantics ||
        !machine_observe_file_get_header_summary(observe_file, &header_summary) ||
        header_summary.target_profile != profile ||
        !machine_observe_file_get_target_policy_summary(observe_file, &target_policy_summary) ||
        !target_policy_summary.surfaces_preview_state ||
        !machine_observe_file_get_observe_summary(observe_file, &observe_summary) ||
        observe_summary.resolution_kind != resolution_kind ||
        observe_summary.is_exact_state != is_exact_state ||
        observe_summary.has_observed_state != has_observed_state ||
        !machine_observe_dump_file(observe_file, &dump_text, &observe_error)) {
        fprintf(stderr, "[machine-observe] FAIL: %s validation mismatch: %s\n", context, observe_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_observe_report(const MachineObserveReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    MachineObserveResolutionKind resolution_kind,
    const char *expected_dump) {
    MachineObserveReportOverviewArtifact overview_artifact;
    const MachineObserveFile *observe_file = NULL;
    const MachineApplyReport *apply_report = NULL;
    const MachineObserveHeaderSummary *header_summary = NULL;
    const MachineObserveTargetPolicySummary *target_policy_summary = NULL;
    const MachineObserveSummary *observe_summary = NULL;
    const MachineElfArtifactSummary *source_artifact_summary = NULL;
    MachineObserveError observe_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&observe_error, 0, sizeof(observe_error));

    if (!machine_observe_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_observe_report_get_file(report, &observe_file) || !observe_file ||
        !machine_observe_report_get_apply_report(report, &apply_report) || !apply_report ||
        !machine_observe_report_get_source_elf_artifact_summary_artifact(report, &source_artifact_summary) ||
        !source_artifact_summary ||
        !machine_observe_report_get_header_summary_artifact(report, &header_summary) || !header_summary ||
        header_summary->target_profile != profile ||
        source_artifact_summary->target_profile != profile ||
        source_artifact_summary->origin_profile != origin_profile ||
        source_artifact_summary->relocation_semantics != semantics ||
        !machine_observe_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary || !target_policy_summary->surfaces_exact_state ||
        !machine_observe_report_get_observe_summary_artifact(report, &observe_summary) ||
        !observe_summary ||
        observe_summary->resolution_kind != resolution_kind ||
        !machine_observe_report_overview_artifact_get_observe_summary_artifact(
            &overview_artifact, &observe_summary) ||
        !observe_summary ||
        !machine_observe_dump_report(report, &dump_text, &observe_error)) {
        fprintf(stderr, "[machine-observe] FAIL: %s report mismatch: %s\n", context, observe_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_observe_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineObserveFile observe_file;
    MachineObserveFile cloned_observe_file;
    MachineObserveReport observe_report;
    MachineObserveError observe_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_observe_file_init(&observe_file);
    machine_observe_file_init(&cloned_observe_file);
    machine_observe_report_init(&observe_report);
    memset(&observe_error, 0, sizeof(observe_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_observe_build_from_machine_ir_report(&ir_report, &observe_file, &observe_error) ||
        !machine_observe_build_report_from_machine_ir_report(&ir_report, &observe_report, &observe_error)) {
        fprintf(stderr,
            "[machine-observe] FAIL: mainline setup failed: ir=%s observe=%s\n",
            ir_error.message,
            observe_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_observe_file(
        &observe_file,
        "observe-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_OBSERVE_RESOLUTION_PREVIEW_STATE,
        0,
        1,
        "machine_observe profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans apply=pending-register-application origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "observe: resolution=preview-state kind=state apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1e value=0x0e known=yes name=load-local bytes=1 payload=[] imm=- exact=no has-state=yes status=ready pc=0x1001 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n");

    if (!machine_observe_clone_file(&observe_file, &cloned_observe_file, &observe_error) ||
        cloned_observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers ==
            observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers) {
        fprintf(stderr, "[machine-observe] FAIL: clone failed: %s\n", observe_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_observe_report(
        &observe_report,
        "observe-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_OBSERVE_RESOLUTION_PREVIEW_STATE,
        "machine_observe profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans apply=pending-register-application origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "observe: resolution=preview-state kind=state apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1e value=0x0e known=yes name=load-local bytes=1 payload=[] imm=- exact=no has-state=yes status=ready pc=0x1001 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: apply=pending-register-application status=ready segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000\n"
        "  elf_source: target=generic-elf32 origin=generic-elf32 semantics=direct-patch-spans\n"
        "  policy: profile=generic-elf32 exact=yes preview=yes\n"
        "  observe: resolution=preview-state kind=state exact=no state=yes imm=- pc=0x1001 targets=[] return-imm=-\n");

    observe_report.header_summary.origin_program_counter = 0u;
    if (!machine_observe_report_refresh(&observe_report, &observe_error)) {
        fprintf(stderr, "[machine-observe] FAIL: report refresh failed: %s\n", observe_error.message);
        ok = 0;
        goto cleanup;
    }

cleanup:
    machine_observe_report_free(&observe_report);
    machine_observe_file_free(&cloned_observe_file);
    machine_observe_file_free(&observe_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_observe_custom_step_cases(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineObserveFile observe_file;
    MachineObserveReport observe_report;
    MachineObserveError observe_error;
    static const unsigned char return_imm_payload[] = {0x17u};
    static const unsigned char jump_payload[] = {0x01u};
    static const unsigned char store_local_imm_payload[] = {0x07u};
    static const unsigned char store_global_imm_payload[] = {0x05u};
    static const unsigned char call_void_imm_payload[] = {0x02u};
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_observe_file_init(&observe_file);
    machine_observe_report_init(&observe_report);
    memset(&observe_error, 0, sizeof(observe_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL)) {
        fprintf(stderr, "[machine-observe] FAIL: custom step setup failed: %s\n", ir_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!overwrite_step_bytes(&step_file, 0x81u, return_imm_payload, 1u, 0, 0u) ||
        !machine_observe_build_from_machine_step_file(&step_file, &observe_file, &observe_error) ||
        !machine_observe_build_report_from_machine_step_file(&step_file, &observe_report, &observe_error)) {
        fprintf(stderr, "[machine-observe] FAIL: halt setup failed: %s\n", observe_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_observe_file(
        &observe_file,
        "observe-halt-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_OBSERVE_RESOLUTION_EXACT_STATE,
        1,
        1,
        "machine_observe profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans apply=applied-state origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "observe: resolution=exact-state kind=state apply=applied-state commit=committed-state writeback=committed-no-op mutation=no-mutation effect=control-only transition=halt action=halt raw=0x81 value=0x01 known=yes name=return-imm bytes=2 payload=[0x17] imm=7 exact=yes has-state=yes status=halted pc=0x1000 current-segment=- current-byte=- targets=[] return-imm=7\n");

    machine_observe_report_free(&observe_report);
    machine_observe_file_free(&observe_file);

    if (!overwrite_step_bytes(&step_file, 0x84u, jump_payload, 1u, 0, 0u) ||
        !machine_observe_build_from_machine_step_file(&step_file, &observe_file, &observe_error) ||
        !machine_observe_build_report_from_machine_step_file(&step_file, &observe_report, &observe_error)) {
        fprintf(stderr, "[machine-observe] FAIL: jump setup failed: %s\n", observe_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_observe_file(
        &observe_file,
        "observe-jump-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_OBSERVE_RESOLUTION_BLOCKED_ON_CONTROL,
        0,
        0,
        "machine_observe profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans apply=blocked-on-control origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "observe: resolution=blocked-on-control kind=none apply=blocked-on-control commit=blocked-on-control writeback=blocked-on-control mutation=blocked-on-control effect=control-only transition=deferred-control-transfer action=control-transfer raw=0x84 value=0x04 known=yes name=jump bytes=2 payload=[0x01] imm=1 exact=no has-state=no status=- pc=- current-segment=- current-byte=- targets=[1] return-imm=-\n");

    machine_observe_report_free(&observe_report);
    machine_observe_file_free(&observe_file);

    if (!overwrite_step_bytes(&step_file, 0x00u, NULL, 0u, 0, 0u) ||
        !machine_observe_build_from_machine_step_file(&step_file, &observe_file, &observe_error) ||
        !machine_observe_build_report_from_machine_step_file(&step_file, &observe_report, &observe_error)) {
        fprintf(stderr, "[machine-observe] FAIL: unsupported setup failed: %s\n", observe_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_observe_file(
        &observe_file,
        "observe-unsupported-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_OBSERVE_RESOLUTION_BLOCKED_UNSUPPORTED,
        0,
        0,
        "machine_observe profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans apply=blocked-unsupported origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "observe: resolution=blocked-unsupported kind=none apply=blocked-unsupported commit=blocked-unsupported writeback=blocked-unsupported mutation=blocked-unsupported effect=none transition=unsupported action=unsupported raw=0x00 value=0x00 known=no name=- bytes=1 payload=[] imm=- exact=no has-state=no status=- pc=- current-segment=- current-byte=- targets=[] return-imm=-\n");

    machine_observe_report_free(&observe_report);
    machine_observe_file_free(&observe_file);

    if (!overwrite_step_bytes(&step_file, 0x20u, store_local_imm_payload, 1u, 1, 0xaau) ||
        !machine_observe_build_from_machine_step_file(&step_file, &observe_file, &observe_error)) {
        fprintf(stderr, "[machine-observe] FAIL: store-local-imm setup failed: %s\n", observe_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_observe_file(
        &observe_file,
        "observe-store-local-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_OBSERVE_RESOLUTION_PREVIEW_STATE,
        0,
        1,
        "machine_observe profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans apply=pending-local-application origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "observe: resolution=preview-state kind=state apply=pending-local-application commit=deferred-local-commit writeback=deferred-local-writeback mutation=deferred-local-slot effect=local-slot transition=next-fetch action=advance raw=0x20 value=0x10 known=yes name=store-local-imm bytes=2 payload=[0x07] imm=7 exact=no has-state=yes status=ready pc=0x1002 current-segment=0 current-byte=0xaa targets=[] return-imm=-\n");

    machine_observe_file_free(&observe_file);

    if (!overwrite_step_bytes(&step_file, 0x23u, store_global_imm_payload, 1u, 1, 0xabu) ||
        !machine_observe_build_from_machine_step_file(&step_file, &observe_file, &observe_error)) {
        fprintf(stderr, "[machine-observe] FAIL: store-global-imm setup failed: %s\n", observe_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_observe_file(
        &observe_file,
        "observe-store-global-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_OBSERVE_RESOLUTION_PREVIEW_STATE,
        0,
        1,
        "machine_observe profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans apply=pending-global-application origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "observe: resolution=preview-state kind=state apply=pending-global-application commit=deferred-global-commit writeback=deferred-global-writeback mutation=deferred-global-slot effect=global-slot transition=next-fetch action=advance raw=0x23 value=0x13 known=yes name=store-global-imm bytes=2 payload=[0x05] imm=5 exact=no has-state=yes status=ready pc=0x1002 current-segment=0 current-byte=0xab targets=[] return-imm=-\n");

    machine_observe_file_free(&observe_file);

    if (!overwrite_step_bytes(&step_file, 0x1bu, call_void_imm_payload, 1u, 1, 0xacu) ||
        !machine_observe_build_from_machine_step_file(&step_file, &observe_file, &observe_error)) {
        fprintf(stderr, "[machine-observe] FAIL: call-void-imm setup failed: %s\n", observe_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_observe_file(
        &observe_file,
        "observe-call-void-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_OBSERVE_RESOLUTION_PREVIEW_STATE,
        0,
        1,
        "machine_observe profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans apply=pending-call-application origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "observe: resolution=preview-state kind=state apply=pending-call-application commit=deferred-call-commit writeback=deferred-call-writeback mutation=deferred-call-effect effect=call transition=next-fetch action=advance raw=0x1b value=0x0b known=yes name=call-void-imm bytes=2 payload=[0x02] imm=2 exact=no has-state=yes status=ready pc=0x1002 current-segment=0 current-byte=0xac targets=[] return-imm=-\n");

cleanup:
    machine_observe_report_free(&observe_report);
    machine_observe_file_free(&observe_file);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_observe_i386_bridge(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineObserveFile observe_file;
    MachineObserveReport observe_report;
    MachineObserveError observe_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_observe_file_init(&observe_file);
    machine_observe_report_init(&observe_report);
    memset(&observe_error, 0, sizeof(observe_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_observe_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &observe_file, &observe_error) ||
        !machine_observe_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &observe_report, &observe_error) ||
        !machine_observe_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &observe_error) ||
        !machine_observe_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &observe_error)) {
        fprintf(stderr, "[machine-observe] FAIL: i386 bridge setup failed: %s / %s\n",
            ir_error.message,
            observe_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text("observe i386 dump wrapper", dump_text,
        "machine_observe profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans apply=pending-register-application origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192\n"
        "observe: resolution=preview-state kind=state apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1e value=0x0e known=yes name=load-local bytes=1 payload=[] imm=- exact=no has-state=yes status=ready pc=0x8048001 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n");
    ok &= expect_text("observe i386 report dump wrapper", report_dump_text,
        "machine_observe profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans apply=pending-register-application origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192\n"
        "observe: resolution=preview-state kind=state apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1e value=0x0e known=yes name=load-local bytes=1 payload=[] imm=- exact=no has-state=yes status=ready pc=0x8048001 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: apply=pending-register-application status=ready segment=0 mapped-bytes=8192 pc=0x8048000 sp=0x804b000\n"
        "  elf_source: target=i386-preview origin=i386-preview semantics=direct-patch-spans\n"
        "  policy: profile=i386-preview exact=yes preview=yes\n"
        "  observe: resolution=preview-state kind=state exact=no state=yes imm=- pc=0x8048001 targets=[] return-imm=-\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_observe_report_free(&observe_report);
    machine_observe_file_free(&observe_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_observe_mainline();
    ok &= test_machine_observe_custom_step_cases();
    ok &= test_machine_observe_i386_bridge();
    return ok ? 0 : 1;
}
