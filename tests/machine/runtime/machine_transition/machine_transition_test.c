#include "machine/transition.h"

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
            "[machine-transition] FAIL: %s mismatch\nactual:\n%s\nexpected:\n%s\n",
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

static int verify_transition_file(const MachineTransitionFile *transition_file,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    size_t base_virtual_address,
    MachineTransitionResolutionKind resolution_kind,
    MachineStepStatus next_status,
    int has_next_fetch,
    size_t next_program_counter,
    size_t next_segment_index,
    unsigned char next_byte,
    const char *expected_dump) {
    MachineTransitionHeaderSummary header_summary;
    MachineTransitionTargetPolicySummary target_policy_summary;
    MachineTransitionSummary transition_summary;
    MachineTransitionNextFetchSummary next_fetch_summary;
    MachineElfArtifactSummary source_artifact_summary;
    MachineTransitionError transition_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&transition_summary, 0, sizeof(transition_summary));
    memset(&next_fetch_summary, 0, sizeof(next_fetch_summary));
    memset(&source_artifact_summary, 0, sizeof(source_artifact_summary));
    memset(&transition_error, 0, sizeof(transition_error));

    if (!machine_transition_verify_file(transition_file, &transition_error) ||
        !machine_transition_file_get_source_elf_artifact_summary(transition_file, &source_artifact_summary) ||
        source_artifact_summary.target_profile != profile ||
        source_artifact_summary.origin_profile != origin_profile ||
        source_artifact_summary.relocation_semantics != semantics ||
        !machine_transition_file_get_header_summary(transition_file, &header_summary) ||
        header_summary.target_profile != profile ||
        header_summary.program_counter != base_virtual_address ||
        !machine_transition_file_get_target_policy_summary(transition_file, &target_policy_summary) ||
        !target_policy_summary.resolves_linear_next_fetch ||
        !machine_transition_file_get_transition_summary(transition_file, &transition_summary) ||
        transition_summary.resolution_kind != resolution_kind ||
        transition_summary.next_status != next_status ||
        transition_summary.has_next_fetch != has_next_fetch ||
        (has_next_fetch && transition_summary.next_program_counter != next_program_counter) ||
        (has_next_fetch && transition_summary.next_current_segment_index != next_segment_index) ||
        (has_next_fetch && transition_summary.next_current_byte != next_byte) ||
        (has_next_fetch && !machine_transition_file_get_next_fetch_summary(transition_file, &next_fetch_summary)) ||
        (has_next_fetch && next_fetch_summary.byte_virtual_address != next_program_counter) ||
        (has_next_fetch && next_fetch_summary.segment_index != next_segment_index) ||
        (has_next_fetch && next_fetch_summary.byte_value != next_byte) ||
        !machine_transition_dump_file(transition_file, &dump_text, &transition_error)) {
        fprintf(stderr, "[machine-transition] FAIL: %s validation mismatch: %s\n", context, transition_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_transition_report(const MachineTransitionReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    MachineTransitionResolutionKind resolution_kind,
    int has_next_fetch,
    size_t next_program_counter,
    const char *expected_dump) {
    MachineTransitionReportOverviewArtifact overview_artifact;
    const MachineTransitionFile *transition_file = NULL;
    const MachineInterpFile *interp_file = NULL;
    const MachineTransitionHeaderSummary *header_summary = NULL;
    const MachineTransitionTargetPolicySummary *target_policy_summary = NULL;
    const MachineTransitionSummary *transition_summary = NULL;
    const MachineTransitionNextFetchSummary *next_fetch_summary = NULL;
    const MachineElfArtifactSummary *source_artifact_summary = NULL;
    MachineTransitionError transition_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&transition_error, 0, sizeof(transition_error));

    if (!machine_transition_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_transition_report_get_file(report, &transition_file) || !transition_file ||
        !machine_transition_report_get_interp_file(report, &interp_file) || !interp_file ||
        !machine_transition_report_get_source_elf_artifact_summary_artifact(report, &source_artifact_summary) ||
        !source_artifact_summary ||
        !machine_transition_report_get_header_summary_artifact(report, &header_summary) || !header_summary ||
        header_summary->target_profile != profile ||
        source_artifact_summary->target_profile != profile ||
        source_artifact_summary->origin_profile != origin_profile ||
        source_artifact_summary->relocation_semantics != semantics ||
        !machine_transition_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary || !target_policy_summary->defers_control_transfer_targets ||
        !machine_transition_report_get_transition_summary_artifact(report, &transition_summary) ||
        !transition_summary || transition_summary->resolution_kind != resolution_kind ||
        transition_summary->has_next_fetch != has_next_fetch ||
        (has_next_fetch && transition_summary->next_program_counter != next_program_counter) ||
        (has_next_fetch &&
            !machine_transition_report_get_next_fetch_summary_artifact(report, &next_fetch_summary)) ||
        (has_next_fetch && !next_fetch_summary) ||
        !machine_transition_report_overview_artifact_get_transition_summary_artifact(
            &overview_artifact, &transition_summary) ||
        !transition_summary ||
        !machine_transition_dump_report(report, &dump_text, &transition_error)) {
        fprintf(stderr, "[machine-transition] FAIL: %s report mismatch: %s\n", context, transition_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_transition_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineTransitionFile transition_file;
    MachineTransitionFile cloned_transition_file;
    MachineTransitionReport transition_report;
    MachineTransitionError transition_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_transition_file_init(&transition_file);
    machine_transition_file_init(&cloned_transition_file);
    machine_transition_report_init(&transition_report);
    memset(&transition_error, 0, sizeof(transition_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_transition_build_from_machine_ir_report(&ir_report, &transition_file, &transition_error) ||
        !machine_transition_build_report_from_machine_ir_report(&ir_report, &transition_report, &transition_error)) {
        fprintf(stderr,
            "[machine-transition] FAIL: mainline setup failed: ir=%s transition=%s\n",
            ir_error.message,
            transition_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_transition_file(
        &transition_file,
        "transition-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u,
        MACHINE_TRANSITION_RESOLUTION_NEXT_FETCH,
        MACHINE_STEP_STATUS_READY,
        1,
        0x1001u,
        0u,
        0x8au,
        "machine_transition profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "transition: resolution=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 next-status=ready next-pc=0x1001 next-segment=0 next-byte=0x8a targets=[] return-imm=-\n");

    if (!machine_transition_clone_file(&transition_file, &cloned_transition_file, &transition_error) ||
        cloned_transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers ==
            transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers) {
        fprintf(stderr, "[machine-transition] FAIL: clone failed: %s\n", transition_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_transition_report(
        &transition_report,
        "transition-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_TRANSITION_RESOLUTION_NEXT_FETCH,
        1,
        0x1001u,
        "machine_transition profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "transition: resolution=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 next-status=ready next-pc=0x1001 next-segment=0 next-byte=0x8a targets=[] return-imm=-\n"
        "report_overview:\n"
        "  status=ready current-segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000\n"
        "  elf_source: target=generic-elf32 origin=generic-elf32 semantics=direct-patch-spans\n"
        "  policy: profile=generic-elf32 linear-next-fetch=yes halt=yes deferred-control=yes\n"
        "  transition: resolution=next-fetch next-status=ready next-pc=0x1001 targets=[] return-imm=-\n");

    transition_report.header_summary.program_counter = 0u;
    transition_report.transition_summary.next_program_counter = 0u;
    if (!machine_transition_report_refresh(&transition_report, &transition_error)) {
        fprintf(stderr, "[machine-transition] FAIL: report refresh failed: %s\n", transition_error.message);
        ok = 0;
        goto cleanup;
    }

cleanup:
    machine_transition_report_free(&transition_report);
    machine_transition_file_free(&cloned_transition_file);
    machine_transition_file_free(&transition_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_transition_custom_step_cases(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineStepReport step_report;
    MachineTransitionFile transition_file;
    MachineTransitionReport transition_report;
    MachineTransitionError transition_error;
    static const unsigned char return_imm_payload[] = {0x17u};
    static const unsigned char jump_payload[] = {0x01u};
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_step_report_init(&step_report);
    machine_transition_file_init(&transition_file);
    machine_transition_report_init(&transition_report);
    memset(&transition_error, 0, sizeof(transition_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL)) {
        fprintf(stderr, "[machine-transition] FAIL: custom step setup failed: %s\n", ir_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!overwrite_step_bytes(&step_file, 0x81u, return_imm_payload, 1u) ||
        !machine_step_build_report_from_file(&step_file, &step_report, NULL) ||
        !machine_transition_build_from_machine_step_file(&step_file, &transition_file, &transition_error) ||
        !machine_transition_build_report_from_machine_step_file(&step_file, &transition_report, &transition_error)) {
        fprintf(stderr, "[machine-transition] FAIL: halt setup failed: %s\n", transition_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_transition_file(
        &transition_file,
        "transition-halt-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u,
        MACHINE_TRANSITION_RESOLUTION_HALT,
        MACHINE_STEP_STATUS_HALTED,
        0,
        0u,
        0u,
        0u,
        "machine_transition profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "transition: resolution=halt action=halt raw=0x81 value=0x01 known=yes name=return-imm bytes=2 next-status=halted next-pc=- next-segment=- next-byte=- targets=[] return-imm=7\n");

    machine_transition_report_free(&transition_report);
    machine_transition_file_free(&transition_file);
    machine_step_report_free(&step_report);
    machine_step_report_init(&step_report);

    if (!overwrite_step_bytes(&step_file, 0x84u, jump_payload, 1u) ||
        !machine_step_build_report_from_file(&step_file, &step_report, NULL) ||
        !machine_transition_build_from_machine_step_report(&step_report, &transition_file, &transition_error) ||
        !machine_transition_build_report_from_machine_step_report(&step_report, &transition_report, &transition_error)) {
        fprintf(stderr, "[machine-transition] FAIL: control-transfer setup failed: %s\n", transition_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_transition_file(
        &transition_file,
        "transition-deferred-control-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u,
        MACHINE_TRANSITION_RESOLUTION_DEFERRED_CONTROL_TRANSFER,
        MACHINE_STEP_STATUS_READY,
        0,
        0u,
        0u,
        0u,
        "machine_transition profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "transition: resolution=deferred-control-transfer action=control-transfer raw=0x84 value=0x04 known=yes name=jump bytes=2 next-status=ready next-pc=- next-segment=- next-byte=- targets=[1] return-imm=-\n");

    machine_transition_report_free(&transition_report);
    machine_transition_file_free(&transition_file);
    machine_step_report_free(&step_report);
    machine_step_report_init(&step_report);

    if (!overwrite_step_bytes(&step_file, 0x00u, NULL, 0u) ||
        !machine_step_build_report_from_file(&step_file, &step_report, NULL) ||
        !machine_transition_build_from_machine_step_file(&step_file, &transition_file, &transition_error) ||
        !machine_transition_build_report_from_machine_step_file(&step_file, &transition_report, &transition_error)) {
        fprintf(stderr, "[machine-transition] FAIL: unsupported setup failed: %s\n", transition_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_transition_file(
        &transition_file,
        "transition-unsupported-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u,
        MACHINE_TRANSITION_RESOLUTION_UNSUPPORTED,
        MACHINE_STEP_STATUS_READY,
        0,
        0u,
        0u,
        0u,
        "machine_transition profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans status=ready pc=0x1000 sp=0x4000 current_segment=0 mapped_bytes=8192\n"
        "transition: resolution=unsupported action=unsupported raw=0x00 value=0x00 known=no name=- bytes=1 next-status=ready next-pc=- next-segment=- next-byte=- targets=[] return-imm=-\n");

cleanup:
    machine_transition_report_free(&transition_report);
    machine_transition_file_free(&transition_file);
    machine_step_report_free(&step_report);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_transition_i386_bridge(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineTransitionFile transition_file;
    MachineTransitionReport transition_report;
    MachineTransitionError transition_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_transition_file_init(&transition_file);
    machine_transition_report_init(&transition_report);
    memset(&transition_error, 0, sizeof(transition_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_transition_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &transition_file, &transition_error) ||
        !machine_transition_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &transition_report, &transition_error) ||
        !machine_transition_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &transition_error) ||
        !machine_transition_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &transition_error)) {
        fprintf(stderr, "[machine-transition] FAIL: i386 bridge setup failed: %s / %s\n",
            ir_error.message,
            transition_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text("transition i386 dump wrapper", dump_text,
        "machine_transition profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans status=ready pc=0x8048000 sp=0x804b000 current_segment=0 mapped_bytes=8192\n"
        "transition: resolution=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 next-status=ready next-pc=0x8048001 next-segment=0 next-byte=0x8a targets=[] return-imm=-\n");
    ok &= expect_text("transition i386 report dump wrapper", report_dump_text,
        "machine_transition profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans status=ready pc=0x8048000 sp=0x804b000 current_segment=0 mapped_bytes=8192\n"
        "transition: resolution=next-fetch action=advance raw=0x1c value=0x0c known=yes name=load-local bytes=1 next-status=ready next-pc=0x8048001 next-segment=0 next-byte=0x8a targets=[] return-imm=-\n"
        "report_overview:\n"
        "  status=ready current-segment=0 mapped-bytes=8192 pc=0x8048000 sp=0x804b000\n"
        "  elf_source: target=i386-preview origin=i386-preview semantics=direct-patch-spans\n"
        "  policy: profile=i386-preview linear-next-fetch=yes halt=yes deferred-control=yes\n"
        "  transition: resolution=next-fetch next-status=ready next-pc=0x8048001 targets=[] return-imm=-\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_transition_report_free(&transition_report);
    machine_transition_file_free(&transition_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_transition_mainline();
    ok &= test_machine_transition_custom_step_cases();
    ok &= test_machine_transition_i386_bridge();
    return ok ? 0 : 1;
}
