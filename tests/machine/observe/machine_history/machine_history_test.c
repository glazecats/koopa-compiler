#include "machine/history.h"

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
            "[machine-history] FAIL: %s mismatch\nactual:\n%s\nexpected:\n%s\n",
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

static int verify_history_file(const MachineHistoryFile *history_file,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    MachineHistoryResolutionKind resolution_kind,
    MachineHistoryKind history_kind,
    const char *expected_dump) {
    MachineHistoryHeaderSummary header_summary;
    MachineHistoryTargetPolicySummary target_policy_summary;
    MachineHistorySummary history_summary;
    MachineElfArtifactSummary source_artifact_summary;
    MachineHistoryError history_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&history_summary, 0, sizeof(history_summary));
    memset(&source_artifact_summary, 0, sizeof(source_artifact_summary));
    memset(&history_error, 0, sizeof(history_error));

    if (!machine_history_verify_file(history_file, &history_error) ||
        !machine_history_file_get_source_elf_artifact_summary(history_file, &source_artifact_summary) ||
        source_artifact_summary.target_profile != profile ||
        source_artifact_summary.origin_profile != origin_profile ||
        source_artifact_summary.relocation_semantics != semantics ||
        !machine_history_file_get_header_summary(history_file, &header_summary) ||
        header_summary.target_profile != profile ||
        header_summary.history_entry_count != 1u ||
        !machine_history_file_get_target_policy_summary(history_file, &target_policy_summary) ||
        !target_policy_summary.surfaces_single_entry_history ||
        !machine_history_file_get_history_summary(history_file, &history_summary) ||
        history_summary.resolution_kind != resolution_kind ||
        history_summary.history_kind != history_kind ||
        !machine_history_dump_file(history_file, &dump_text, &history_error)) {
        fprintf(stderr, "[machine-history] FAIL: %s validation mismatch: %s\n", context, history_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_history_report(const MachineHistoryReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    MachineHistoryResolutionKind resolution_kind,
    const char *expected_dump) {
    MachineHistoryReportOverviewArtifact overview_artifact;
    const MachineHistoryFile *history_file = NULL;
    const MachineOutcomeReport *outcome_report = NULL;
    const MachineHistoryHeaderSummary *header_summary = NULL;
    const MachineHistoryTargetPolicySummary *target_policy_summary = NULL;
    const MachineHistorySummary *history_summary = NULL;
    const MachineElfArtifactSummary *source_artifact_summary = NULL;
    MachineHistoryError history_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&history_error, 0, sizeof(history_error));

    if (!machine_history_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_history_report_get_file(report, &history_file) || !history_file ||
        !machine_history_report_get_outcome_report(report, &outcome_report) || !outcome_report ||
        !machine_history_report_get_source_elf_artifact_summary_artifact(report, &source_artifact_summary) ||
        !source_artifact_summary ||
        !machine_history_report_get_header_summary_artifact(report, &header_summary) || !header_summary ||
        header_summary->target_profile != profile ||
        source_artifact_summary->target_profile != profile ||
        source_artifact_summary->origin_profile != origin_profile ||
        source_artifact_summary->relocation_semantics != semantics ||
        header_summary->history_entry_count != 1u ||
        !machine_history_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary || !target_policy_summary->surfaces_preview_history ||
        !machine_history_report_get_history_summary_artifact(report, &history_summary) ||
        !history_summary ||
        history_summary->resolution_kind != resolution_kind ||
        !machine_history_report_overview_artifact_get_history_summary_artifact(
            &overview_artifact, &history_summary) ||
        !history_summary ||
        !machine_history_dump_report(report, &dump_text, &history_error)) {
        fprintf(stderr, "[machine-history] FAIL: %s report mismatch: %s\n", context, history_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_history_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineHistoryFile history_file;
    MachineHistoryFile cloned_history_file;
    MachineHistoryReport history_report;
    MachineHistoryError history_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_history_file_init(&history_file);
    machine_history_file_init(&cloned_history_file);
    machine_history_report_init(&history_report);
    memset(&history_error, 0, sizeof(history_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_history_build_from_machine_ir_report(&ir_report, &history_file, &history_error) ||
        !machine_history_build_report_from_machine_ir_report(&ir_report, &history_report, &history_error)) {
        fprintf(stderr,
            "[machine-history] FAIL: mainline setup failed: ir=%s history=%s\n",
            ir_error.message,
            history_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_history_file(
        &history_file,
        "history-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_HISTORY_RESOLUTION_PREVIEW_HISTORY,
        MACHINE_HISTORY_KIND_VALUE_HISTORY,
        "machine_history profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans outcome=preview-outcome origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 entries=1\n"
        "history: resolution=preview-history kind=value-history outcome=value-available event=register-result trace=preview-trace change-class=program-counter-and-fetch apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- exact=no single-entry=yes entries=1 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n");

    if (!machine_history_clone_file(&history_file, &cloned_history_file, &history_error) ||
        cloned_history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers ==
            history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers) {
        fprintf(stderr, "[machine-history] FAIL: clone failed: %s\n", history_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_history_report(
        &history_report,
        "history-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_HISTORY_RESOLUTION_PREVIEW_HISTORY,
        "machine_history profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans outcome=preview-outcome origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 entries=1\n"
        "history: resolution=preview-history kind=value-history outcome=value-available event=register-result trace=preview-trace change-class=program-counter-and-fetch apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- exact=no single-entry=yes entries=1 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: outcome=preview-outcome status=ready segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000 entries=1\n"
        "  elf_source: target=generic-elf32 origin=generic-elf32 semantics=direct-patch-spans\n"
        "  policy: profile=generic-elf32 exact=yes preview=yes single-entry=yes\n"
        "  history: resolution=preview-history kind=value-history outcome=value-available exact=no single-entry=yes entries=1 state=yes status=no pc=yes fetch=yes targets=[] return-imm=-\n");

    history_report.header_summary.origin_program_counter = 0u;
    if (!machine_history_report_refresh(&history_report, &history_error)) {
        fprintf(stderr, "[machine-history] FAIL: report refresh failed: %s\n", history_error.message);
        ok = 0;
        goto cleanup;
    }

cleanup:
    machine_history_report_free(&history_report);
    machine_history_file_free(&cloned_history_file);
    machine_history_file_free(&history_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_history_custom_step_cases(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineHistoryFile history_file;
    MachineHistoryReport history_report;
    MachineHistoryError history_error;
    static const unsigned char return_imm_payload[] = {0x17u};
    static const unsigned char jump_payload[] = {0x01u};
    static const unsigned char store_local_imm_payload[] = {0x07u};
    static const unsigned char store_global_imm_payload[] = {0x05u};
    static const unsigned char call_void_imm_payload[] = {0x02u};
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_history_file_init(&history_file);
    machine_history_report_init(&history_report);
    memset(&history_error, 0, sizeof(history_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL)) {
        fprintf(stderr, "[machine-history] FAIL: custom step setup failed: %s\n", ir_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!overwrite_step_bytes(&step_file, 0x81u, return_imm_payload, 1u, 0, 0u) ||
        !machine_history_build_from_machine_step_file(&step_file, &history_file, &history_error) ||
        !machine_history_build_report_from_machine_step_file(&step_file, &history_report, &history_error)) {
        fprintf(stderr, "[machine-history] FAIL: halt setup failed: %s\n", history_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_history_file(
        &history_file,
        "history-halt-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_HISTORY_RESOLUTION_EXACT_HISTORY,
        MACHINE_HISTORY_KIND_PROGRAM_STOP_HISTORY,
        "machine_history profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans outcome=exact-outcome origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 entries=1\n"
        "history: resolution=exact-history kind=program-stop-history outcome=program-stopped event=control-halt trace=exact-trace change-class=status-and-fetch apply=applied-state commit=committed-state writeback=committed-no-op mutation=no-mutation effect=control-only transition=halt action=halt raw=0x81 value=0x01 known=yes name=return-imm bytes=2 payload=[0x17] imm=7 exact=yes single-entry=yes entries=1 origin-status=ready observed-status=halted status-changed=yes pc-changed=no stack-changed=no fetch-changed=yes targets=[] return-imm=7\n");

    machine_history_report_free(&history_report);
    machine_history_file_free(&history_file);

    if (!overwrite_step_bytes(&step_file, 0x84u, jump_payload, 1u, 0, 0u) ||
        !machine_history_build_from_machine_step_file(&step_file, &history_file, &history_error) ||
        !machine_history_build_report_from_machine_step_file(&step_file, &history_report, &history_error)) {
        fprintf(stderr, "[machine-history] FAIL: jump setup failed: %s\n", history_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_history_file(
        &history_file,
        "history-jump-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_HISTORY_RESOLUTION_BLOCKED_ON_CONTROL,
        MACHINE_HISTORY_KIND_BLOCKED_CONTROL_HISTORY,
        "machine_history profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans outcome=blocked-on-control origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 entries=1\n"
        "history: resolution=blocked-on-control kind=blocked-control-history outcome=blocked-control event=blocked-control trace=blocked-on-control change-class=none apply=blocked-on-control commit=blocked-on-control writeback=blocked-on-control mutation=blocked-on-control effect=control-only transition=deferred-control-transfer action=control-transfer raw=0x84 value=0x04 known=yes name=jump bytes=2 payload=[0x01] imm=1 exact=no single-entry=yes entries=1 origin-status=ready observed-status=- status-changed=- pc-changed=- stack-changed=- fetch-changed=- targets=[1] return-imm=-\n");

    machine_history_report_free(&history_report);
    machine_history_file_free(&history_file);

    if (!overwrite_step_bytes(&step_file, 0x00u, NULL, 0u, 0, 0u) ||
        !machine_history_build_from_machine_step_file(&step_file, &history_file, &history_error) ||
        !machine_history_build_report_from_machine_step_file(&step_file, &history_report, &history_error)) {
        fprintf(stderr, "[machine-history] FAIL: unsupported setup failed: %s\n", history_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_history_file(
        &history_file,
        "history-unsupported-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_HISTORY_RESOLUTION_BLOCKED_UNSUPPORTED,
        MACHINE_HISTORY_KIND_BLOCKED_UNSUPPORTED_HISTORY,
        "machine_history profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans outcome=blocked-unsupported origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 entries=1\n"
        "history: resolution=blocked-unsupported kind=blocked-unsupported-history outcome=blocked-unsupported event=blocked-unsupported trace=blocked-unsupported change-class=none apply=blocked-unsupported commit=blocked-unsupported writeback=blocked-unsupported mutation=blocked-unsupported effect=none transition=unsupported action=unsupported raw=0x00 value=0x00 known=no name=- bytes=1 payload=[] imm=- exact=no single-entry=yes entries=1 origin-status=ready observed-status=- status-changed=- pc-changed=- stack-changed=- fetch-changed=- targets=[] return-imm=-\n");

    machine_history_report_free(&history_report);
    machine_history_file_free(&history_file);

    if (!overwrite_step_bytes(&step_file, 0x1eu, store_local_imm_payload, 1u, 1, 0xaau) ||
        !machine_history_build_from_machine_step_file(&step_file, &history_file, &history_error)) {
        fprintf(stderr, "[machine-history] FAIL: store-local-imm setup failed: %s\n", history_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_history_file(
        &history_file,
        "history-store-local-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_HISTORY_RESOLUTION_PREVIEW_HISTORY,
        MACHINE_HISTORY_KIND_LOCAL_UPDATE_HISTORY,
        "machine_history profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans outcome=preview-outcome origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 entries=1\n"
        "history: resolution=preview-history kind=local-update-history outcome=local-updated event=local-store trace=preview-trace change-class=program-counter-and-fetch apply=pending-local-application commit=deferred-local-commit writeback=deferred-local-writeback mutation=deferred-local-slot effect=local-slot transition=next-fetch action=advance raw=0x1e value=0x0e known=yes name=store-local-imm bytes=2 payload=[0x07] imm=7 exact=no single-entry=yes entries=1 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n");

    machine_history_file_free(&history_file);

    if (!overwrite_step_bytes(&step_file, 0x21u, store_global_imm_payload, 1u, 1, 0xabu) ||
        !machine_history_build_from_machine_step_file(&step_file, &history_file, &history_error)) {
        fprintf(stderr, "[machine-history] FAIL: store-global-imm setup failed: %s\n", history_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_history_file(
        &history_file,
        "history-store-global-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_HISTORY_RESOLUTION_PREVIEW_HISTORY,
        MACHINE_HISTORY_KIND_GLOBAL_UPDATE_HISTORY,
        "machine_history profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans outcome=preview-outcome origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 entries=1\n"
        "history: resolution=preview-history kind=global-update-history outcome=global-updated event=global-store trace=preview-trace change-class=program-counter-and-fetch apply=pending-global-application commit=deferred-global-commit writeback=deferred-global-writeback mutation=deferred-global-slot effect=global-slot transition=next-fetch action=advance raw=0x21 value=0x11 known=yes name=store-global-imm bytes=2 payload=[0x05] imm=5 exact=no single-entry=yes entries=1 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n");

    machine_history_file_free(&history_file);

    if (!overwrite_step_bytes(&step_file, 0x1bu, call_void_imm_payload, 1u, 1, 0xacu) ||
        !machine_history_build_from_machine_step_file(&step_file, &history_file, &history_error)) {
        fprintf(stderr, "[machine-history] FAIL: call-void-imm setup failed: %s\n", history_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_history_file(
        &history_file,
        "history-call-void-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_HISTORY_RESOLUTION_PREVIEW_HISTORY,
        MACHINE_HISTORY_KIND_CALL_HISTORY,
        "machine_history profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans outcome=preview-outcome origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192 entries=1\n"
        "history: resolution=preview-history kind=call-history outcome=call-issued event=call-effect trace=preview-trace change-class=program-counter-and-fetch apply=pending-call-application commit=deferred-call-commit writeback=deferred-call-writeback mutation=deferred-call-effect effect=call transition=next-fetch action=advance raw=0x1b value=0x0b known=yes name=call-void-imm bytes=2 payload=[0x02] imm=2 exact=no single-entry=yes entries=1 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n");

cleanup:
    machine_history_report_free(&history_report);
    machine_history_file_free(&history_file);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_history_i386_bridge(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineHistoryFile history_file;
    MachineHistoryReport history_report;
    MachineHistoryError history_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_history_file_init(&history_file);
    machine_history_report_init(&history_report);
    memset(&history_error, 0, sizeof(history_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_history_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &history_file, &history_error) ||
        !machine_history_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &history_report, &history_error) ||
        !machine_history_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &history_error) ||
        !machine_history_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &history_error)) {
        fprintf(stderr, "[machine-history] FAIL: i386 bridge setup failed: %s / %s\n",
            ir_error.message,
            history_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text("history i386 dump wrapper", dump_text,
        "machine_history profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans outcome=preview-outcome origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192 entries=1\n"
        "history: resolution=preview-history kind=value-history outcome=value-available event=register-result trace=preview-trace change-class=program-counter-and-fetch apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- exact=no single-entry=yes entries=1 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n");
    ok &= expect_text("history i386 report dump wrapper", report_dump_text,
        "machine_history profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans outcome=preview-outcome origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192 entries=1\n"
        "history: resolution=preview-history kind=value-history outcome=value-available event=register-result trace=preview-trace change-class=program-counter-and-fetch apply=pending-register-application commit=deferred-register-commit writeback=deferred-register-writeback mutation=deferred-register-result effect=value-result transition=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 payload=[] imm=- exact=no single-entry=yes entries=1 origin-status=ready observed-status=ready status-changed=no pc-changed=yes stack-changed=no fetch-changed=yes targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: outcome=preview-outcome status=ready segment=0 mapped-bytes=8192 pc=0x8048000 sp=0x804b000 entries=1\n"
        "  elf_source: target=i386-preview origin=i386-preview semantics=direct-patch-spans\n"
        "  policy: profile=i386-preview exact=yes preview=yes single-entry=yes\n"
        "  history: resolution=preview-history kind=value-history outcome=value-available exact=no single-entry=yes entries=1 state=yes status=no pc=yes fetch=yes targets=[] return-imm=-\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_history_report_free(&history_report);
    machine_history_file_free(&history_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_history_mainline();
    ok &= test_machine_history_custom_step_cases();
    ok &= test_machine_history_i386_bridge();
    return ok ? 0 : 1;
}
