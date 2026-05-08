#include "machine/state.h"

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
            "[machine-state] FAIL: %s mismatch\nactual:\n%s\nexpected:\n%s\n",
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

static int verify_state_file(const MachineStateFile *state_file,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    MachineStateResolutionKind resolution_kind,
    int has_state,
    MachineStepStatus state_status,
    size_t program_counter,
    size_t stack_pointer,
    int has_current_fetch,
    size_t current_segment_index,
    unsigned char current_byte,
    const char *expected_dump) {
    MachineStateHeaderSummary header_summary;
    MachineStateTargetPolicySummary target_policy_summary;
    MachineStateSummary state_summary;
    MachineStateCurrentFetchSummary current_fetch_summary;
    MachineElfArtifactSummary source_artifact_summary;
    MachineStateError state_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&state_summary, 0, sizeof(state_summary));
    memset(&current_fetch_summary, 0, sizeof(current_fetch_summary));
    memset(&source_artifact_summary, 0, sizeof(source_artifact_summary));
    memset(&state_error, 0, sizeof(state_error));

    if (!machine_state_verify_file(state_file, &state_error) ||
        !machine_state_file_get_source_elf_artifact_summary(state_file, &source_artifact_summary) ||
        source_artifact_summary.target_profile != profile ||
        source_artifact_summary.origin_profile != origin_profile ||
        source_artifact_summary.relocation_semantics != semantics ||
        !machine_state_file_get_header_summary(state_file, &header_summary) ||
        header_summary.target_profile != profile ||
        !machine_state_file_get_target_policy_summary(state_file, &target_policy_summary) ||
        !target_policy_summary.resolves_ready_state ||
        !machine_state_file_get_state_summary(state_file, &state_summary) ||
        state_summary.resolution_kind != resolution_kind ||
        state_summary.has_state != has_state ||
        state_summary.state_status != state_status ||
        (has_state && state_summary.program_counter != program_counter) ||
        (has_state && state_summary.stack_pointer != stack_pointer) ||
        state_summary.has_current_fetch != has_current_fetch ||
        (has_current_fetch && state_summary.current_segment_index != current_segment_index) ||
        (has_current_fetch && state_summary.current_byte != current_byte) ||
        (has_current_fetch && !machine_state_file_get_current_fetch_summary(state_file, &current_fetch_summary)) ||
        (has_current_fetch && current_fetch_summary.byte_virtual_address != program_counter) ||
        (has_current_fetch && current_fetch_summary.segment_index != current_segment_index) ||
        (has_current_fetch && current_fetch_summary.byte_value != current_byte) ||
        !machine_state_dump_file(state_file, &dump_text, &state_error)) {
        fprintf(stderr, "[machine-state] FAIL: %s validation mismatch: %s\n", context, state_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_state_report(const MachineStateReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    MachineStateResolutionKind resolution_kind,
    int has_state,
    size_t program_counter,
    const char *expected_dump) {
    MachineStateReportOverviewArtifact overview_artifact;
    const MachineStateFile *state_file = NULL;
    const MachineTransitionReport *transition_report = NULL;
    const MachineStateHeaderSummary *header_summary = NULL;
    const MachineStateTargetPolicySummary *target_policy_summary = NULL;
    const MachineStateSummary *state_summary = NULL;
    const MachineStateCurrentFetchSummary *current_fetch_summary = NULL;
    const MachineElfArtifactSummary *source_artifact_summary = NULL;
    MachineStateError state_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&state_error, 0, sizeof(state_error));

    if (!machine_state_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_state_report_get_file(report, &state_file) || !state_file ||
        !machine_state_report_get_transition_report(report, &transition_report) || !transition_report ||
        !machine_state_report_get_source_elf_artifact_summary_artifact(report, &source_artifact_summary) ||
        !source_artifact_summary ||
        !machine_state_report_get_header_summary_artifact(report, &header_summary) || !header_summary ||
        header_summary->target_profile != profile ||
        source_artifact_summary->target_profile != profile ||
        source_artifact_summary->origin_profile != origin_profile ||
        source_artifact_summary->relocation_semantics != semantics ||
        !machine_state_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary || !target_policy_summary->defers_control_transfer_state ||
        !machine_state_report_get_state_summary_artifact(report, &state_summary) ||
        !state_summary || state_summary->resolution_kind != resolution_kind ||
        state_summary->has_state != has_state ||
        (has_state && state_summary->program_counter != program_counter) ||
        (state_summary->has_current_fetch &&
            !machine_state_report_get_current_fetch_summary_artifact(report, &current_fetch_summary)) ||
        !machine_state_report_overview_artifact_get_state_summary_artifact(&overview_artifact, &state_summary) ||
        !state_summary ||
        !machine_state_dump_report(report, &dump_text, &state_error)) {
        fprintf(stderr, "[machine-state] FAIL: %s report mismatch: %s\n", context, state_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_state_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStateFile state_file;
    MachineStateFile cloned_state_file;
    MachineStateReport state_report;
    MachineStateError state_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_state_file_init(&state_file);
    machine_state_file_init(&cloned_state_file);
    machine_state_report_init(&state_report);
    memset(&state_error, 0, sizeof(state_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_state_build_from_machine_ir_report(&ir_report, &state_file, &state_error) ||
        !machine_state_build_report_from_machine_ir_report(&ir_report, &state_report, &state_error)) {
        fprintf(stderr,
            "[machine-state] FAIL: mainline setup failed: ir=%s state=%s\n",
            ir_error.message,
            state_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_state_file(
        &state_file,
        "state-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_STATE_RESOLUTION_READY,
        1,
        MACHINE_STEP_STATUS_READY,
        0x1001u,
        0x4000u,
        1,
        0u,
        0x8au,
        "machine_state profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "state: resolution=ready transition=next-fetch action=advance raw=0x1e value=0x0e known=yes name=load-local bytes=1 has-state=yes status=ready pc=0x1001 sp=0x4000 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n");

    if (!machine_state_clone_file(&state_file, &cloned_state_file, &state_error) ||
        cloned_state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers ==
            state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers) {
        fprintf(stderr, "[machine-state] FAIL: clone failed: %s\n", state_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_state_report(
        &state_report,
        "state-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_STATE_RESOLUTION_READY,
        1,
        0x1001u,
        "machine_state profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "state: resolution=ready transition=next-fetch action=advance raw=0x1e value=0x0e known=yes name=load-local bytes=1 has-state=yes status=ready pc=0x1001 sp=0x4000 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: status=ready segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000\n"
        "  elf_source: target=generic-elf32 origin=generic-elf32 semantics=direct-patch-spans\n"
        "  policy: profile=generic-elf32 ready=yes halt=yes deferred-control=yes\n"
        "  state: resolution=ready has-state=yes status=ready pc=0x1001 targets=[] return-imm=-\n");

    state_report.header_summary.origin_program_counter = 0u;
    state_report.state_summary.program_counter = 0u;
    if (!machine_state_report_refresh(&state_report, &state_error)) {
        fprintf(stderr, "[machine-state] FAIL: report refresh failed: %s\n", state_error.message);
        ok = 0;
        goto cleanup;
    }

cleanup:
    machine_state_report_free(&state_report);
    machine_state_file_free(&cloned_state_file);
    machine_state_file_free(&state_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_state_custom_step_cases(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineStepReport step_report;
    MachineStateFile state_file;
    MachineStateReport state_report;
    MachineStateError state_error;
    static const unsigned char return_imm_payload[] = {0x17u};
    static const unsigned char jump_payload[] = {0x01u};
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_step_report_init(&step_report);
    machine_state_file_init(&state_file);
    machine_state_report_init(&state_report);
    memset(&state_error, 0, sizeof(state_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL)) {
        fprintf(stderr, "[machine-state] FAIL: custom step setup failed: %s\n", ir_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!overwrite_step_bytes(&step_file, 0x81u, return_imm_payload, 1u) ||
        !machine_step_build_report_from_file(&step_file, &step_report, NULL) ||
        !machine_state_build_from_machine_step_file(&step_file, &state_file, &state_error) ||
        !machine_state_build_report_from_machine_step_file(&step_file, &state_report, &state_error)) {
        fprintf(stderr, "[machine-state] FAIL: halt setup failed: %s\n", state_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_state_file(
        &state_file,
        "state-halt-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_STATE_RESOLUTION_HALTED,
        1,
        MACHINE_STEP_STATUS_HALTED,
        0x1000u,
        0x4000u,
        0,
        0u,
        0u,
        "machine_state profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "state: resolution=halted transition=halt action=halt raw=0x81 value=0x01 known=yes name=return-imm bytes=2 has-state=yes status=halted pc=0x1000 sp=0x4000 current-segment=- current-byte=- targets=[] return-imm=7\n");

    machine_state_report_free(&state_report);
    machine_state_file_free(&state_file);
    machine_step_report_free(&step_report);
    machine_step_report_init(&step_report);

    if (!overwrite_step_bytes(&step_file, 0x84u, jump_payload, 1u) ||
        !machine_step_build_report_from_file(&step_file, &step_report, NULL) ||
        !machine_state_build_from_machine_step_report(&step_report, &state_file, &state_error) ||
        !machine_state_build_report_from_machine_step_report(&step_report, &state_report, &state_error)) {
        fprintf(stderr, "[machine-state] FAIL: deferred control setup failed: %s\n", state_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_state_file(
        &state_file,
        "state-deferred-control-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_STATE_RESOLUTION_DEFERRED_CONTROL_TRANSFER,
        0,
        MACHINE_STEP_STATUS_READY,
        0u,
        0u,
        0,
        0u,
        0u,
        "machine_state profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "state: resolution=deferred-control-transfer transition=deferred-control-transfer action=control-transfer raw=0x84 value=0x04 known=yes name=jump bytes=2 has-state=no status=- pc=- sp=- current-segment=- current-byte=- targets=[1] return-imm=-\n");

    machine_state_report_free(&state_report);
    machine_state_file_free(&state_file);
    machine_step_report_free(&step_report);
    machine_step_report_init(&step_report);

    if (!overwrite_step_bytes(&step_file, 0x00u, NULL, 0u) ||
        !machine_step_build_report_from_file(&step_file, &step_report, NULL) ||
        !machine_state_build_from_machine_step_file(&step_file, &state_file, &state_error) ||
        !machine_state_build_report_from_machine_step_file(&step_file, &state_report, &state_error)) {
        fprintf(stderr, "[machine-state] FAIL: unsupported setup failed: %s\n", state_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_state_file(
        &state_file,
        "state-unsupported-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_STATE_RESOLUTION_UNSUPPORTED,
        0,
        MACHINE_STEP_STATUS_READY,
        0u,
        0u,
        0,
        0u,
        0u,
        "machine_state profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "state: resolution=unsupported transition=unsupported action=unsupported raw=0x00 value=0x00 known=no name=- bytes=1 has-state=no status=- pc=- sp=- current-segment=- current-byte=- targets=[] return-imm=-\n");

cleanup:
    machine_state_report_free(&state_report);
    machine_state_file_free(&state_file);
    machine_step_report_free(&step_report);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_state_i386_bridge(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStateFile state_file;
    MachineStateReport state_report;
    MachineStateError state_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_state_file_init(&state_file);
    machine_state_report_init(&state_report);
    memset(&state_error, 0, sizeof(state_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_state_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &state_file, &state_error) ||
        !machine_state_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &state_report, &state_error) ||
        !machine_state_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &state_error) ||
        !machine_state_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &state_error)) {
        fprintf(stderr, "[machine-state] FAIL: i386 bridge setup failed: %s / %s\n",
            ir_error.message,
            state_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text("state i386 dump wrapper", dump_text,
        "machine_state profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192\n"
        "state: resolution=ready transition=next-fetch action=advance raw=0x1e value=0x0e known=yes name=load-local bytes=1 has-state=yes status=ready pc=0x8048001 sp=0x804b000 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n");
    ok &= expect_text("state i386 report dump wrapper", report_dump_text,
        "machine_state profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192\n"
        "state: resolution=ready transition=next-fetch action=advance raw=0x1e value=0x0e known=yes name=load-local bytes=1 has-state=yes status=ready pc=0x8048001 sp=0x804b000 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: status=ready segment=0 mapped-bytes=8192 pc=0x8048000 sp=0x804b000\n"
        "  elf_source: target=i386-preview origin=i386-preview semantics=direct-patch-spans\n"
        "  policy: profile=i386-preview ready=yes halt=yes deferred-control=yes\n"
        "  state: resolution=ready has-state=yes status=ready pc=0x8048001 targets=[] return-imm=-\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_state_report_free(&state_report);
    machine_state_file_free(&state_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_state_mainline();
    ok &= test_machine_state_custom_step_cases();
    ok &= test_machine_state_i386_bridge();
    return ok ? 0 : 1;
}
