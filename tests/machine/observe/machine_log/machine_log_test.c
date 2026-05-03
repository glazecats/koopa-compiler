#include "machine/log.h"

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
            "[machine-log] FAIL: %s mismatch\nactual:\n%s\nexpected:\n%s\n",
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

static int verify_log_file(const MachineLogFile *log_file,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    MachineLogResolutionKind resolution_kind,
    MachineLogKind log_kind,
    const char *expected_dump) {
    MachineLogHeaderSummary header_summary;
    MachineLogTargetPolicySummary target_policy_summary;
    MachineLogSummary log_summary;
    MachineElfArtifactSummary source_artifact_summary;
    MachineLogError log_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&log_summary, 0, sizeof(log_summary));
    memset(&source_artifact_summary, 0, sizeof(source_artifact_summary));
    memset(&log_error, 0, sizeof(log_error));

    if (!machine_log_verify_file(log_file, &log_error) ||
        !machine_log_file_get_source_elf_artifact_summary(log_file, &source_artifact_summary) ||
        source_artifact_summary.target_profile != profile ||
        source_artifact_summary.origin_profile != origin_profile ||
        source_artifact_summary.relocation_semantics != semantics ||
        !machine_log_file_get_header_summary(log_file, &header_summary) ||
        header_summary.target_profile != profile ||
        header_summary.log_line_count != 1u ||
        header_summary.log_line_index != 0u ||
        !machine_log_file_get_target_policy_summary(log_file, &target_policy_summary) ||
        !target_policy_summary.surfaces_single_line_log ||
        !machine_log_file_get_log_summary(log_file, &log_summary) ||
        log_summary.resolution_kind != resolution_kind ||
        log_summary.log_kind != log_kind ||
        !machine_log_dump_file(log_file, &dump_text, &log_error)) {
        fprintf(stderr, "[machine-log] FAIL: %s validation mismatch: %s\n", context, log_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_log_report(const MachineLogReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    MachineLogResolutionKind resolution_kind,
    const char *expected_dump) {
    MachineLogReportOverviewArtifact overview_artifact;
    const MachineLogFile *log_file = NULL;
    const MachineTimelineReport *timeline_report = NULL;
    const MachineLogHeaderSummary *header_summary = NULL;
    const MachineLogTargetPolicySummary *target_policy_summary = NULL;
    const MachineLogSummary *log_summary = NULL;
    const MachineElfArtifactSummary *source_artifact_summary = NULL;
    MachineLogError log_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&log_error, 0, sizeof(log_error));

    if (!machine_log_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_log_report_get_file(report, &log_file) || !log_file ||
        !machine_log_report_get_timeline_report(report, &timeline_report) || !timeline_report ||
        !machine_log_report_get_source_elf_artifact_summary_artifact(report, &source_artifact_summary) ||
        !source_artifact_summary ||
        !machine_log_report_get_header_summary_artifact(report, &header_summary) || !header_summary ||
        header_summary->target_profile != profile ||
        source_artifact_summary->target_profile != profile ||
        source_artifact_summary->origin_profile != origin_profile ||
        source_artifact_summary->relocation_semantics != semantics ||
        header_summary->log_line_count != 1u ||
        header_summary->log_line_index != 0u ||
        !machine_log_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary || !target_policy_summary->surfaces_preview_log ||
        !machine_log_report_get_log_summary_artifact(report, &log_summary) ||
        !log_summary ||
        log_summary->resolution_kind != resolution_kind ||
        !machine_log_report_overview_artifact_get_log_summary_artifact(
            &overview_artifact, &log_summary) ||
        !log_summary ||
        !machine_log_dump_report(report, &dump_text, &log_error)) {
        fprintf(stderr, "[machine-log] FAIL: %s report mismatch: %s\n", context, log_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_log_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineLogFile log_file;
    MachineLogFile cloned_log_file;
    MachineLogReport log_report;
    MachineLogError log_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_log_file_init(&log_file);
    machine_log_file_init(&cloned_log_file);
    machine_log_report_init(&log_report);
    memset(&log_error, 0, sizeof(log_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_log_build_from_machine_ir_report(&ir_report, &log_file, &log_error) ||
        !machine_log_build_report_from_machine_ir_report(&ir_report, &log_report, &log_error)) {
        fprintf(stderr,
            "[machine-log] FAIL: mainline setup failed: ir=%s log=%s\n",
            ir_error.message,
            log_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_log_file(
        &log_file,
        "log-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_LOG_RESOLUTION_PREVIEW_LOG,
        MACHINE_LOG_KIND_VALUE_LOG,
        "machine_log profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans timeline=preview-timeline origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 lines=1 line-index=0\n"
        "log: resolution=preview-log kind=value-log timeline=value-timeline history=value-history outcome=value-available event=register-result trace=preview-trace change-class=program-counter-and-fetch apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- exact=no single-line=yes lines=1 line-index=0 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n");

    if (!machine_log_clone_file(&log_file, &cloned_log_file, &log_error) ||
        cloned_log_file.timeline_file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers ==
            log_file.timeline_file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers) {
        fprintf(stderr, "[machine-log] FAIL: clone failed: %s\n", log_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_log_report(
        &log_report,
        "log-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_LOG_RESOLUTION_PREVIEW_LOG,
        "machine_log profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans timeline=preview-timeline origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 lines=1 line-index=0\n"
        "log: resolution=preview-log kind=value-log timeline=value-timeline history=value-history outcome=value-available event=register-result trace=preview-trace change-class=program-counter-and-fetch apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- exact=no single-line=yes lines=1 line-index=0 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: timeline=preview-timeline status=ready segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000 lines=1 line-index=0\n"
        "  elf_source: target=generic-elf32 origin=generic-elf32 semantics=direct-patch-spans\n"
        "  policy: profile=generic-elf32 exact=yes preview=yes single-line=yes\n"
        "  log: resolution=preview-log kind=value-log timeline=value-timeline exact=no single-line=yes lines=1 line-index=0 state=yes status=no pc=yes fetch=yes targets=[] return-imm=-\n");

    log_report.header_summary.origin_program_counter = 0u;
    if (!machine_log_report_refresh(&log_report, &log_error)) {
        fprintf(stderr, "[machine-log] FAIL: report refresh failed: %s\n", log_error.message);
        ok = 0;
        goto cleanup;
    }

cleanup:
    machine_log_report_free(&log_report);
    machine_log_file_free(&cloned_log_file);
    machine_log_file_free(&log_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_log_custom_step_cases(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineLogFile log_file;
    MachineLogReport log_report;
    MachineLogError log_error;
    static const unsigned char return_imm_payload[] = {0x17u};
    static const unsigned char jump_payload[] = {0x01u};
    static const unsigned char store_local_imm_payload[] = {0x07u};
    static const unsigned char store_global_imm_payload[] = {0x05u};
    static const unsigned char call_void_imm_payload[] = {0x02u};
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_log_file_init(&log_file);
    machine_log_report_init(&log_report);
    memset(&log_error, 0, sizeof(log_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL)) {
        fprintf(stderr, "[machine-log] FAIL: custom step setup failed: %s\n", ir_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!overwrite_step_bytes(&step_file, 0x81u, return_imm_payload, 1u, 0, 0u) ||
        !machine_log_build_from_machine_step_file(&step_file, &log_file, &log_error) ||
        !machine_log_build_report_from_machine_step_file(&step_file, &log_report, &log_error)) {
        fprintf(stderr, "[machine-log] FAIL: halt setup failed: %s\n", log_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_log_file(
        &log_file,
        "log-halt-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_LOG_RESOLUTION_EXACT_LOG,
        MACHINE_LOG_KIND_PROGRAM_STOP_LOG,
        "machine_log profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans timeline=exact-timeline origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 lines=1 line-index=0\n"
        "log: resolution=exact-log kind=program-stop-log timeline=program-stop-timeline history=program-stop-history outcome=program-stopped event=control-halt trace=exact-trace change-class=status-and-fetch apply=applied-state commit=committed-state writeback=committed-no-op mutation=no-mutation effect=control-only transition=halt action=halt raw=0x81 value=0x01 known=yes name=return-imm bytes=2 payload=[0x17] imm=7 exact=yes single-line=yes lines=1 line-index=0 origin-status=ready observed-status=halted status-changed=yes pc-changed=no stack-changed=no fetch-changed=yes targets=[] return-imm=7\n");

    machine_log_report_free(&log_report);
    machine_log_file_free(&log_file);

    if (!overwrite_step_bytes(&step_file, 0x84u, jump_payload, 1u, 0, 0u) ||
        !machine_log_build_from_machine_step_file(&step_file, &log_file, &log_error) ||
        !machine_log_build_report_from_machine_step_file(&step_file, &log_report, &log_error)) {
        fprintf(stderr, "[machine-log] FAIL: jump setup failed: %s\n", log_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_log_file(
        &log_file,
        "log-jump-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_LOG_RESOLUTION_BLOCKED_ON_CONTROL,
        MACHINE_LOG_KIND_BLOCKED_CONTROL_LOG,
        "machine_log profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans timeline=blocked-on-control origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 lines=1 line-index=0\n"
        "log: resolution=blocked-on-control kind=blocked-control-log timeline=blocked-control-timeline history=blocked-control-history outcome=blocked-control event=blocked-control trace=blocked-on-control change-class=none apply=blocked-on-control commit=blocked-on-control writeback=blocked-on-control mutation=blocked-on-control effect=control-only transition=deferred-control-transfer action=control-transfer raw=0x84 value=0x04 known=yes name=jump bytes=2 payload=[0x01] imm=1 exact=no single-line=yes lines=1 line-index=0 origin-status=ready observed-status=- status-changed=- pc-changed=- stack-changed=- fetch-changed=- targets=[1] return-imm=-\n");

    machine_log_report_free(&log_report);
    machine_log_file_free(&log_file);

    if (!overwrite_step_bytes(&step_file, 0x00u, NULL, 0u, 0, 0u) ||
        !machine_log_build_from_machine_step_file(&step_file, &log_file, &log_error) ||
        !machine_log_build_report_from_machine_step_file(&step_file, &log_report, &log_error)) {
        fprintf(stderr, "[machine-log] FAIL: unsupported setup failed: %s\n", log_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_log_file(
        &log_file,
        "log-unsupported-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_LOG_RESOLUTION_BLOCKED_UNSUPPORTED,
        MACHINE_LOG_KIND_BLOCKED_UNSUPPORTED_LOG,
        "machine_log profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans timeline=blocked-unsupported origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 lines=1 line-index=0\n"
        "log: resolution=blocked-unsupported kind=blocked-unsupported-log timeline=blocked-unsupported-timeline history=blocked-unsupported-history outcome=blocked-unsupported event=blocked-unsupported trace=blocked-unsupported change-class=none apply=blocked-unsupported commit=blocked-unsupported writeback=blocked-unsupported mutation=blocked-unsupported effect=none transition=unsupported action=unsupported raw=0x00 value=0x00 known=no name=- bytes=1 payload=[] imm=- exact=no single-line=yes lines=1 line-index=0 origin-status=ready observed-status=- status-changed=- pc-changed=- stack-changed=- fetch-changed=- targets=[] return-imm=-\n");

    machine_log_report_free(&log_report);
    machine_log_file_free(&log_file);

    if (!overwrite_step_bytes(&step_file, 0x1eu, store_local_imm_payload, 1u, 1, 0xaau) ||
        !machine_log_build_from_machine_step_file(&step_file, &log_file, &log_error)) {
        fprintf(stderr, "[machine-log] FAIL: store-local-imm setup failed: %s\n", log_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_log_file(
        &log_file,
        "log-store-local-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_LOG_RESOLUTION_PREVIEW_LOG,
        MACHINE_LOG_KIND_LOCAL_UPDATE_LOG,
        "machine_log profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans timeline=preview-timeline origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 lines=1 line-index=0\n"
        "log: resolution=preview-log kind=local-update-log timeline=local-update-timeline history=local-update-history outcome=local-updated event=local-store trace=preview-trace change-class=program-counter-and-fetch apply=pending-local-application commit=deferred-local-commit writeback=deferred-local-writeback mutation=deferred-local-slot effect=local-slot transition=next-fetch action=advance raw=0x1e value=0x0e known=yes name=store-local-imm bytes=2 payload=[0x07] imm=7 exact=no single-line=yes lines=1 line-index=0 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n");

    machine_log_file_free(&log_file);

    if (!overwrite_step_bytes(&step_file, 0x21u, store_global_imm_payload, 1u, 1, 0xabu) ||
        !machine_log_build_from_machine_step_file(&step_file, &log_file, &log_error)) {
        fprintf(stderr, "[machine-log] FAIL: store-global-imm setup failed: %s\n", log_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_log_file(
        &log_file,
        "log-store-global-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_LOG_RESOLUTION_PREVIEW_LOG,
        MACHINE_LOG_KIND_GLOBAL_UPDATE_LOG,
        "machine_log profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans timeline=preview-timeline origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 lines=1 line-index=0\n"
        "log: resolution=preview-log kind=global-update-log timeline=global-update-timeline history=global-update-history outcome=global-updated event=global-store trace=preview-trace change-class=program-counter-and-fetch apply=pending-global-application commit=deferred-global-commit writeback=deferred-global-writeback mutation=deferred-global-slot effect=global-slot transition=next-fetch action=advance raw=0x21 value=0x11 known=yes name=store-global-imm bytes=2 payload=[0x05] imm=5 exact=no single-line=yes lines=1 line-index=0 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n");

    machine_log_file_free(&log_file);

    if (!overwrite_step_bytes(&step_file, 0x1bu, call_void_imm_payload, 1u, 1, 0xacu) ||
        !machine_log_build_from_machine_step_file(&step_file, &log_file, &log_error)) {
        fprintf(stderr, "[machine-log] FAIL: call-void-imm setup failed: %s\n", log_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_log_file(
        &log_file,
        "log-call-void-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_LOG_RESOLUTION_PREVIEW_LOG,
        MACHINE_LOG_KIND_CALL_LOG,
        "machine_log profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans timeline=preview-timeline origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 lines=1 line-index=0\n"
        "log: resolution=preview-log kind=call-log timeline=call-timeline history=call-history outcome=call-issued event=call-effect trace=preview-trace change-class=program-counter-and-fetch apply=pending-call-application commit=deferred-call-commit writeback=deferred-call-writeback mutation=deferred-call-effect effect=call transition=next-fetch action=advance raw=0x1b value=0x0b known=yes name=call-void-imm bytes=2 payload=[0x02] imm=2 exact=no single-line=yes lines=1 line-index=0 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n");

cleanup:
    machine_log_report_free(&log_report);
    machine_log_file_free(&log_file);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_log_i386_bridge(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineLogFile log_file;
    MachineLogReport log_report;
    MachineLogError log_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_log_file_init(&log_file);
    machine_log_report_init(&log_report);
    memset(&log_error, 0, sizeof(log_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_log_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &log_file, &log_error) ||
        !machine_log_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &log_report, &log_error) ||
        !machine_log_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &log_error) ||
        !machine_log_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &log_error)) {
        fprintf(stderr, "[machine-log] FAIL: i386 bridge setup failed: %s / %s\n",
            ir_error.message,
            log_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text("log i386 dump wrapper", dump_text,
        "machine_log profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans timeline=preview-timeline origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192 lines=1 line-index=0\n"
        "log: resolution=preview-log kind=value-log timeline=value-timeline history=value-history outcome=value-available event=register-result trace=preview-trace change-class=program-counter-and-fetch apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- exact=no single-line=yes lines=1 line-index=0 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n");
    ok &= expect_text("log i386 report dump wrapper", report_dump_text,
        "machine_log profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans timeline=preview-timeline origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192 lines=1 line-index=0\n"
        "log: resolution=preview-log kind=value-log timeline=value-timeline history=value-history outcome=value-available event=register-result trace=preview-trace change-class=program-counter-and-fetch apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- exact=no single-line=yes lines=1 line-index=0 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: timeline=preview-timeline status=ready segment=0 mapped-bytes=8192 pc=0x8048000 sp=0x804b000 lines=1 line-index=0\n"
        "  elf_source: target=i386-preview origin=i386-preview semantics=direct-patch-spans\n"
        "  policy: profile=i386-preview exact=yes preview=yes single-line=yes\n"
        "  log: resolution=preview-log kind=value-log timeline=value-timeline exact=no single-line=yes lines=1 line-index=0 state=yes status=no pc=yes fetch=yes targets=[] return-imm=-\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_log_report_free(&log_report);
    machine_log_file_free(&log_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_log_mainline();
    ok &= test_machine_log_custom_step_cases();
    ok &= test_machine_log_i386_bridge();
    return ok ? 0 : 1;
}
