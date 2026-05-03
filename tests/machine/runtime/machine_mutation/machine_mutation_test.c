#include "machine/mutation.h"

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
            "[machine-mutation] FAIL: %s mismatch\nactual:\n%s\nexpected:\n%s\n",
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

static int verify_mutation_file(const MachineMutationFile *mutation_file,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    MachineMutationResolutionKind resolution_kind,
    MachineMutationEffectKind effect_kind,
    int has_state,
    size_t program_counter,
    const char *expected_dump) {
    MachineMutationHeaderSummary header_summary;
    MachineMutationTargetPolicySummary target_policy_summary;
    MachineMutationSummary mutation_summary;
    MachineElfArtifactSummary source_artifact_summary;
    MachineMutationError mutation_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    memset(&mutation_summary, 0, sizeof(mutation_summary));
    memset(&source_artifact_summary, 0, sizeof(source_artifact_summary));
    memset(&mutation_error, 0, sizeof(mutation_error));

    if (!machine_mutation_verify_file(mutation_file, &mutation_error) ||
        !machine_mutation_file_get_source_elf_artifact_summary(mutation_file, &source_artifact_summary) ||
        source_artifact_summary.target_profile != profile ||
        source_artifact_summary.origin_profile != origin_profile ||
        source_artifact_summary.relocation_semantics != semantics ||
        !machine_mutation_file_get_header_summary(mutation_file, &header_summary) ||
        header_summary.target_profile != profile ||
        !machine_mutation_file_get_target_policy_summary(mutation_file, &target_policy_summary) ||
        !target_policy_summary.classifies_control_only ||
        !machine_mutation_file_get_mutation_summary(mutation_file, &mutation_summary) ||
        mutation_summary.resolution_kind != resolution_kind ||
        mutation_summary.effect_kind != effect_kind ||
        mutation_summary.has_state != has_state ||
        (has_state && mutation_summary.program_counter != program_counter) ||
        !machine_mutation_dump_file(mutation_file, &dump_text, &mutation_error)) {
        fprintf(stderr, "[machine-mutation] FAIL: %s validation mismatch: %s\n", context, mutation_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int verify_mutation_report(const MachineMutationReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    MachineMutationResolutionKind resolution_kind,
    MachineMutationEffectKind effect_kind,
    const char *expected_dump) {
    MachineMutationReportOverviewArtifact overview_artifact;
    const MachineMutationFile *mutation_file = NULL;
    const MachineStateReport *state_report = NULL;
    const MachineMutationHeaderSummary *header_summary = NULL;
    const MachineMutationTargetPolicySummary *target_policy_summary = NULL;
    const MachineMutationSummary *mutation_summary = NULL;
    const MachineElfArtifactSummary *source_artifact_summary = NULL;
    MachineMutationError mutation_error;
    char *dump_text = NULL;
    int ok = 1;

    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&mutation_error, 0, sizeof(mutation_error));

    if (!machine_mutation_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_mutation_report_get_file(report, &mutation_file) || !mutation_file ||
        !machine_mutation_report_get_state_report(report, &state_report) || !state_report ||
        !machine_mutation_report_get_source_elf_artifact_summary_artifact(report, &source_artifact_summary) ||
        !source_artifact_summary ||
        !machine_mutation_report_get_header_summary_artifact(report, &header_summary) || !header_summary ||
        header_summary->target_profile != profile ||
        source_artifact_summary->target_profile != profile ||
        source_artifact_summary->origin_profile != origin_profile ||
        source_artifact_summary->relocation_semantics != semantics ||
        !machine_mutation_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary || !target_policy_summary->defers_call_effect ||
        !machine_mutation_report_get_mutation_summary_artifact(report, &mutation_summary) ||
        !mutation_summary ||
        mutation_summary->resolution_kind != resolution_kind ||
        mutation_summary->effect_kind != effect_kind ||
        !machine_mutation_report_overview_artifact_get_mutation_summary_artifact(
            &overview_artifact, &mutation_summary) ||
        !mutation_summary ||
        !machine_mutation_dump_report(report, &dump_text, &mutation_error)) {
        fprintf(stderr, "[machine-mutation] FAIL: %s report mismatch: %s\n", context, mutation_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_mutation_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineMutationFile mutation_file;
    MachineMutationFile cloned_mutation_file;
    MachineMutationReport mutation_report;
    MachineMutationError mutation_error;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_mutation_file_init(&mutation_file);
    machine_mutation_file_init(&cloned_mutation_file);
    machine_mutation_report_init(&mutation_report);
    memset(&mutation_error, 0, sizeof(mutation_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_mutation_build_from_machine_ir_report(&ir_report, &mutation_file, &mutation_error) ||
        !machine_mutation_build_report_from_machine_ir_report(&ir_report, &mutation_report, &mutation_error)) {
        fprintf(stderr,
            "[machine-mutation] FAIL: mainline setup failed: ir=%s mutation=%s\n",
            ir_error.message,
            mutation_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_mutation_file(
        &mutation_file,
        "mutation-generic-ir-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_MUTATION_RESOLUTION_DEFERRED_REGISTER_RESULT,
        MACHINE_MUTATION_EFFECT_VALUE_RESULT,
        1,
        0x1001u,
        "machine_mutation profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans state=ready origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "mutation: resolution=deferred-register-result effect=value-result transition=next-fetch action=advance payload-kind=op tag-class=op raw=0x1c value=0x0c known=yes name=load-local bytes=1 has-state=yes status=ready pc=0x1001 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n");

    if (!machine_mutation_clone_file(&mutation_file, &cloned_mutation_file, &mutation_error) ||
        cloned_mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers ==
            mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.registers) {
        fprintf(stderr, "[machine-mutation] FAIL: clone failed: %s\n", mutation_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_mutation_report(
        &mutation_report,
        "mutation-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_MUTATION_RESOLUTION_DEFERRED_REGISTER_RESULT,
        MACHINE_MUTATION_EFFECT_VALUE_RESULT,
        "machine_mutation profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans state=ready origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "mutation: resolution=deferred-register-result effect=value-result transition=next-fetch action=advance payload-kind=op tag-class=op raw=0x1c value=0x0c known=yes name=load-local bytes=1 has-state=yes status=ready pc=0x1001 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: state=ready status=ready segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000\n"
        "  elf_source: target=generic-elf32 origin=generic-elf32 semantics=direct-patch-spans\n"
        "  policy: profile=generic-elf32 control-only=yes register=yes slot=yes call=yes\n"
        "  mutation: resolution=deferred-register-result effect=value-result has-state=yes status=ready pc=0x1001 targets=[] return-imm=-\n");

    mutation_report.header_summary.origin_program_counter = 0u;
    mutation_report.mutation_summary.program_counter = 0u;
    if (!machine_mutation_report_refresh(&mutation_report, &mutation_error)) {
        fprintf(stderr, "[machine-mutation] FAIL: report refresh failed: %s\n", mutation_error.message);
        ok = 0;
        goto cleanup;
    }

cleanup:
    machine_mutation_report_free(&mutation_report);
    machine_mutation_file_free(&cloned_mutation_file);
    machine_mutation_file_free(&mutation_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_mutation_custom_step_cases(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineStepFile step_file;
    MachineMutationFile mutation_file;
    MachineMutationReport mutation_report;
    MachineMutationError mutation_error;
    static const unsigned char return_imm_payload[] = {0x17u};
    static const unsigned char jump_payload[] = {0x01u};
    static const unsigned char store_local_imm_payload[] = {0x07u};
    static const unsigned char store_global_imm_payload[] = {0x05u};
    static const unsigned char call_void_imm_payload[] = {0x02u};
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_step_file_init(&step_file);
    machine_mutation_file_init(&mutation_file);
    machine_mutation_report_init(&mutation_report);
    memset(&mutation_error, 0, sizeof(mutation_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_step_build_from_machine_ir_report(&ir_report, &step_file, NULL)) {
        fprintf(stderr, "[machine-mutation] FAIL: custom step setup failed: %s\n", ir_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!overwrite_step_bytes(&step_file, 0x81u, return_imm_payload, 1u, 0, 0u) ||
        !machine_mutation_build_from_machine_step_file(&step_file, &mutation_file, &mutation_error) ||
        !machine_mutation_build_report_from_machine_step_file(&step_file, &mutation_report, &mutation_error)) {
        fprintf(stderr, "[machine-mutation] FAIL: halt setup failed: %s\n", mutation_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_mutation_file(
        &mutation_file,
        "mutation-halt-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_MUTATION_RESOLUTION_NO_MUTATION,
        MACHINE_MUTATION_EFFECT_CONTROL_ONLY,
        1,
        0x1000u,
        "machine_mutation profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans state=halted origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "mutation: resolution=no-mutation effect=control-only transition=halt action=halt payload-kind=terminator tag-class=terminator raw=0x81 value=0x01 known=yes name=return-imm bytes=2 has-state=yes status=halted pc=0x1000 current-segment=- current-byte=- targets=[] return-imm=7\n");

    machine_mutation_report_free(&mutation_report);
    machine_mutation_file_free(&mutation_file);

    if (!overwrite_step_bytes(&step_file, 0x84u, jump_payload, 1u, 0, 0u) ||
        !machine_mutation_build_from_machine_step_file(&step_file, &mutation_file, &mutation_error) ||
        !machine_mutation_build_report_from_machine_step_file(&step_file, &mutation_report, &mutation_error)) {
        fprintf(stderr, "[machine-mutation] FAIL: jump setup failed: %s\n", mutation_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_mutation_file(
        &mutation_file,
        "mutation-jump-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_MUTATION_RESOLUTION_BLOCKED_ON_CONTROL,
        MACHINE_MUTATION_EFFECT_CONTROL_ONLY,
        0,
        0u,
        "machine_mutation profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans state=deferred-control-transfer origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "mutation: resolution=blocked-on-control effect=control-only transition=deferred-control-transfer action=control-transfer payload-kind=terminator tag-class=terminator raw=0x84 value=0x04 known=yes name=jump bytes=2 has-state=no status=- pc=- current-segment=- current-byte=- targets=[1] return-imm=-\n");

    machine_mutation_report_free(&mutation_report);
    machine_mutation_file_free(&mutation_file);

    if (!overwrite_step_bytes(&step_file, 0x00u, NULL, 0u, 0, 0u) ||
        !machine_mutation_build_from_machine_step_file(&step_file, &mutation_file, &mutation_error) ||
        !machine_mutation_build_report_from_machine_step_file(&step_file, &mutation_report, &mutation_error)) {
        fprintf(stderr, "[machine-mutation] FAIL: unsupported setup failed: %s\n", mutation_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_mutation_file(
        &mutation_file,
        "mutation-unsupported-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_MUTATION_RESOLUTION_BLOCKED_UNSUPPORTED,
        MACHINE_MUTATION_EFFECT_NONE,
        0,
        0u,
        "machine_mutation profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans state=unsupported origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "mutation: resolution=blocked-unsupported effect=none transition=unsupported action=unsupported payload-kind=none tag-class=none raw=0x00 value=0x00 known=no name=- bytes=1 has-state=no status=- pc=- current-segment=- current-byte=- targets=[] return-imm=-\n");

    machine_mutation_report_free(&mutation_report);
    machine_mutation_file_free(&mutation_file);

    if (!overwrite_step_bytes(&step_file, 0x1eu, store_local_imm_payload, 1u, 1, 0xaau) ||
        !machine_mutation_build_from_machine_step_file(&step_file, &mutation_file, &mutation_error)) {
        fprintf(stderr, "[machine-mutation] FAIL: store-local-imm setup failed: %s\n", mutation_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_mutation_file(
        &mutation_file,
        "mutation-store-local-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_MUTATION_RESOLUTION_DEFERRED_LOCAL_SLOT,
        MACHINE_MUTATION_EFFECT_LOCAL_SLOT,
        1,
        0x1002u,
        "machine_mutation profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans state=ready origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "mutation: resolution=deferred-local-slot effect=local-slot transition=next-fetch action=advance payload-kind=op tag-class=op raw=0x1e value=0x0e known=yes name=store-local-imm bytes=2 has-state=yes status=ready pc=0x1002 current-segment=0 current-byte=0xaa targets=[] return-imm=-\n");

    machine_mutation_file_free(&mutation_file);

    if (!overwrite_step_bytes(&step_file, 0x21u, store_global_imm_payload, 1u, 1, 0xabu) ||
        !machine_mutation_build_from_machine_step_file(&step_file, &mutation_file, &mutation_error)) {
        fprintf(stderr, "[machine-mutation] FAIL: store-global-imm setup failed: %s\n", mutation_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_mutation_file(
        &mutation_file,
        "mutation-store-global-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_MUTATION_RESOLUTION_DEFERRED_GLOBAL_SLOT,
        MACHINE_MUTATION_EFFECT_GLOBAL_SLOT,
        1,
        0x1002u,
        "machine_mutation profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans state=ready origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "mutation: resolution=deferred-global-slot effect=global-slot transition=next-fetch action=advance payload-kind=op tag-class=op raw=0x21 value=0x11 known=yes name=store-global-imm bytes=2 has-state=yes status=ready pc=0x1002 current-segment=0 current-byte=0xab targets=[] return-imm=-\n");

    machine_mutation_file_free(&mutation_file);

    if (!overwrite_step_bytes(&step_file, 0x1bu, call_void_imm_payload, 1u, 1, 0xacu) ||
        !machine_mutation_build_from_machine_step_file(&step_file, &mutation_file, &mutation_error)) {
        fprintf(stderr, "[machine-mutation] FAIL: call-void-imm setup failed: %s\n", mutation_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= verify_mutation_file(
        &mutation_file,
        "mutation-call-void-imm-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        MACHINE_MUTATION_RESOLUTION_DEFERRED_CALL_EFFECT,
        MACHINE_MUTATION_EFFECT_CALL,
        1,
        0x1002u,
        "machine_mutation profile=generic-elf32 elf_origin=generic-elf32 elf_semantics=direct-patch-spans state=ready origin-status=ready origin-pc=0x1000 origin-sp=0x4000 origin-segment=0 mapped_bytes=8192\n"
        "mutation: resolution=deferred-call-effect effect=call transition=next-fetch action=advance payload-kind=op tag-class=op raw=0x1b value=0x0b known=yes name=call-void-imm bytes=2 has-state=yes status=ready pc=0x1002 current-segment=0 current-byte=0xac targets=[] return-imm=-\n");

cleanup:
    machine_mutation_report_free(&mutation_report);
    machine_mutation_file_free(&mutation_file);
    machine_step_file_free(&step_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_mutation_i386_bridge(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineMutationFile mutation_file;
    MachineMutationReport mutation_report;
    MachineMutationError mutation_error;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_mutation_file_init(&mutation_file);
    machine_mutation_report_init(&mutation_report);
    memset(&mutation_error, 0, sizeof(mutation_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_mutation_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &mutation_file, &mutation_error) ||
        !machine_mutation_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &mutation_report, &mutation_error) ||
        !machine_mutation_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &mutation_error) ||
        !machine_mutation_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &mutation_error)) {
        fprintf(stderr, "[machine-mutation] FAIL: i386 bridge setup failed: %s / %s\n",
            ir_error.message,
            mutation_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text("mutation i386 dump wrapper", dump_text,
        "machine_mutation profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans state=ready origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192\n"
        "mutation: resolution=deferred-register-result effect=value-result transition=next-fetch action=advance payload-kind=op tag-class=op raw=0x1c value=0x0c known=yes name=load-local bytes=1 has-state=yes status=ready pc=0x8048001 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n");
    ok &= expect_text("mutation i386 report dump wrapper", report_dump_text,
        "machine_mutation profile=i386-preview elf_origin=i386-preview elf_semantics=direct-patch-spans state=ready origin-status=ready origin-pc=0x8048000 origin-sp=0x804b000 origin-segment=0 mapped_bytes=8192\n"
        "mutation: resolution=deferred-register-result effect=value-result transition=next-fetch action=advance payload-kind=op tag-class=op raw=0x1c value=0x0c known=yes name=load-local bytes=1 has-state=yes status=ready pc=0x8048001 current-segment=0 current-byte=0x8a targets=[] return-imm=-\n"
        "report_overview:\n"
        "  origin: state=ready status=ready segment=0 mapped-bytes=8192 pc=0x8048000 sp=0x804b000\n"
        "  elf_source: target=i386-preview origin=i386-preview semantics=direct-patch-spans\n"
        "  policy: profile=i386-preview control-only=yes register=yes slot=yes call=yes\n"
        "  mutation: resolution=deferred-register-result effect=value-result has-state=yes status=ready pc=0x8048001 targets=[] return-imm=-\n");

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_mutation_report_free(&mutation_report);
    machine_mutation_file_free(&mutation_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_mutation_mainline();
    ok &= test_machine_mutation_custom_step_cases();
    ok &= test_machine_mutation_i386_bridge();
    return ok ? 0 : 1;
}
