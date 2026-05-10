#include "machine/runtime.h"

#include "machine/ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int build_expected_runtime_dump_text(char *buffer, size_t buffer_size,
    const char *profile_name,
    const char *origin_profile_name,
    const char *semantics_name,
    size_t base_virtual_address) {
    size_t stack_base_virtual_address = base_virtual_address + 0x2000u;
    size_t initial_stack_pointer = base_virtual_address + 0x3000u;

    if (!buffer || buffer_size == 0u || !profile_name || !origin_profile_name || !semantics_name) {
        return 0;
    }
    return snprintf(buffer, buffer_size,
               "machine_runtime profile=%s elf_origin=%s elf_semantics=%s base=0x%zx entry=0x%zx sp=0x%zx segments=2 mapped_bytes=8192\n"
               "entry_segment: rseg.0 .text perms=r-x\n"
               "stack_segment: rseg.1 .stack perms=rw-\n"
               "segments:\n"
               "  rseg.0 .text kind=load lseg=0 vaddr=0x%zx bytes=4096 perms=r-x\n"
               "  rseg.1 .stack kind=stack vaddr=0x%zx bytes=4096 perms=rw-\n",
               profile_name,
               origin_profile_name,
               semantics_name,
               base_virtual_address,
               base_virtual_address,
               initial_stack_pointer,
               base_virtual_address,
               stack_base_virtual_address) > 0;
}

static int build_expected_runtime_report_dump_text(char *buffer, size_t buffer_size,
    const char *profile_name,
    const char *origin_profile_name,
    const char *semantics_name,
    size_t base_virtual_address) {
    size_t stack_base_virtual_address = base_virtual_address + 0x2000u;
    size_t initial_stack_pointer = base_virtual_address + 0x3000u;
    size_t initial_stack_base_virtual_address = initial_stack_pointer - 20u;

    if (!buffer || buffer_size == 0u || !profile_name || !origin_profile_name || !semantics_name) {
        return 0;
    }
    return snprintf(buffer, buffer_size,
               "machine_runtime profile=%s elf_origin=%s elf_semantics=%s base=0x%zx entry=0x%zx sp=0x%zx segments=2 mapped_bytes=8192\n"
               "entry_segment: rseg.0 .text perms=r-x\n"
               "stack_segment: rseg.1 .stack perms=rw-\n"
               "segments:\n"
               "  rseg.0 .text kind=load vaddr=0x%zx bytes=4096 perms=r-x\n"
               "  rseg.1 .stack kind=stack vaddr=0x%zx bytes=4096 perms=rw-\n"
               "report_overview:\n"
               "  segments=2 mapped_bytes=8192 executable_segments=1 non_executable_segments=1 entry=0x%zx sp=0x%zx stack_segment=1\n"
               "  elf_source: target=%s origin=%s semantics=%s\n"
               "  policy: profile=%s base=0x%zx stack-align=4096 stack-size=4096 stack-gap=4096\n"
               "  memory: base=0x%zx end=0x%zx span-bytes=12288 mapped-bytes=8192 entry-offset=0x0 sp-offset=0x3000\n"
               "  stack: segment=1 base=0x%zx end=0x%zx bytes=4096 sp=0x%zx sp-offset=0x1000\n"
               "  gap: base=0x%zx end=0x%zx bytes=4096\n"
               "  launch: pc=0x%zx sp=0x%zx stack-segment=1 stack-base=0x%zx stack-end=0x%zx stack-bytes=4096\n"
               "  initial-stack: base=0x%zx end=0x%zx bytes=20 word=4 argc=0 argv-null=0x%zx env-null=0x%zx auxv-null=0x%zx\n"
               "report_segment_filters:\n"
               "  executable[1] rseg.0(.text)\n"
               "  non-executable[1] rseg.1(.stack)\n"
               "  entry[1] rseg.0(.text)\n"
               "  stack[1] rseg.1(.stack)\n"
               "report_segments:\n"
               "  rseg.0 .text kind=load entry=yes stack=no executable=yes bytes=4096 perms=r-x\n"
               "  rseg.1 .stack kind=stack entry=no stack=yes executable=no bytes=4096 perms=rw-\n",
               profile_name,
               origin_profile_name,
               semantics_name,
               base_virtual_address,
               base_virtual_address,
               initial_stack_pointer,
               base_virtual_address,
               stack_base_virtual_address,
               base_virtual_address,
               initial_stack_pointer,
               profile_name,
               origin_profile_name,
               semantics_name,
               profile_name,
               base_virtual_address,
               base_virtual_address,
               base_virtual_address + 0x3000u,
               stack_base_virtual_address,
               initial_stack_pointer,
               initial_stack_pointer,
               base_virtual_address + 0x1000u,
               stack_base_virtual_address,
               base_virtual_address,
               initial_stack_pointer,
               stack_base_virtual_address,
               initial_stack_pointer,
               initial_stack_base_virtual_address,
               initial_stack_pointer,
               initial_stack_base_virtual_address + 4u,
               initial_stack_base_virtual_address + 8u,
               initial_stack_base_virtual_address + 12u) > 0;
}

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

static int build_manual_runtime_load_file_for_wrap_test(MachineLoadFile *load_file,
    size_t virtual_address,
    size_t memory_byte_count) {
    if (!load_file) {
        return 0;
    }

    machine_load_file_init(load_file);

    load_file->exec_file.image_file.target_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    load_file->exec_file.image_file.source_elf_artifact_summary.target_profile =
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    load_file->exec_file.image_file.source_elf_artifact_summary.origin_profile =
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    load_file->exec_file.image_file.source_elf_artifact_summary.relocation_semantics =
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS;
    load_file->exec_file.image_file.base_virtual_address = virtual_address;
    load_file->exec_file.image_file.has_entry = 1u;
    load_file->exec_file.image_file.entry_symbol_index = 0u;
    load_file->exec_file.image_file.entry_virtual_address = virtual_address;
    load_file->exec_file.image_file.segment_count = 1u;
    load_file->exec_file.image_file.segment_capacity = 1u;
    load_file->exec_file.image_file.segments = (MachineImageSegment *)calloc(1u, sizeof(MachineImageSegment));
    load_file->exec_file.image_file.symbol_count = 1u;
    load_file->exec_file.image_file.symbol_capacity = 1u;
    load_file->exec_file.image_file.symbols = (MachineImageSymbol *)calloc(1u, sizeof(MachineImageSymbol));
    load_file->exec_file.image_file.byte_count = 1u;
    load_file->exec_file.image_file.bytes = (unsigned char *)malloc(1u);
    load_file->exec_file.segment_count = 1u;
    load_file->exec_file.segment_capacity = 1u;
    load_file->exec_file.segments = (MachineExecSegment *)calloc(1u, sizeof(MachineExecSegment));
    load_file->segment_count = 1u;
    load_file->segment_capacity = 1u;
    load_file->segments = (MachineLoadSegment *)calloc(1u, sizeof(MachineLoadSegment));
    if (!load_file->exec_file.image_file.segments ||
        !load_file->exec_file.image_file.symbols ||
        !load_file->exec_file.image_file.bytes ||
        !load_file->exec_file.segments ||
        !load_file->segments) {
        machine_load_file_free(load_file);
        return 0;
    }

    load_file->exec_file.image_file.bytes[0] = 0x7fu;

    load_file->exec_file.image_file.segments[0].name = dup_text(".text");
    load_file->exec_file.image_file.segments[0].image_offset = 0u;
    load_file->exec_file.image_file.segments[0].virtual_address = virtual_address;
    load_file->exec_file.image_file.segments[0].byte_count = 1u;
    load_file->exec_file.image_file.segments[0].align = 4096u;

    load_file->exec_file.image_file.symbols[0].name = dup_text("main");
    load_file->exec_file.image_file.symbols[0].binding = MACHINE_ELF_SYMBOL_GLOBAL;
    load_file->exec_file.image_file.symbols[0].type = MACHINE_ELF_SYMBOL_FUNC;
    load_file->exec_file.image_file.symbols[0].is_defined = 1u;
    load_file->exec_file.image_file.symbols[0].segment_index = 0u;
    load_file->exec_file.image_file.symbols[0].value_offset = 0u;
    load_file->exec_file.image_file.symbols[0].virtual_address = virtual_address;

    load_file->exec_file.entry_virtual_address = virtual_address;
    load_file->exec_file.segments[0].name = dup_text(".text");
    load_file->exec_file.segments[0].image_segment_index = 0u;
    load_file->exec_file.segments[0].virtual_address = virtual_address;
    load_file->exec_file.segments[0].byte_count = 1u;
    load_file->exec_file.segments[0].readable = 1u;
    load_file->exec_file.segments[0].writable = 0u;
    load_file->exec_file.segments[0].executable = 1u;

    load_file->entry_virtual_address = virtual_address;
    load_file->total_memory_byte_count = memory_byte_count;
    load_file->segments[0].name = dup_text(".text");
    load_file->segments[0].exec_segment_index = 0u;
    load_file->segments[0].virtual_address = virtual_address;
    load_file->segments[0].file_byte_count = 1u;
    load_file->segments[0].memory_byte_count = memory_byte_count;
    load_file->segments[0].bytes = memory_byte_count > 0u
        ? (unsigned char *)calloc(memory_byte_count, 1u)
        : NULL;
    load_file->segments[0].readable = 1u;
    load_file->segments[0].writable = 0u;
    load_file->segments[0].executable = 1u;
    if (!load_file->exec_file.image_file.segments[0].name ||
        !load_file->exec_file.image_file.symbols[0].name ||
        !load_file->exec_file.segments[0].name ||
        !load_file->segments[0].name ||
        (memory_byte_count > 0u && !load_file->segments[0].bytes)) {
        machine_load_file_free(load_file);
        return 0;
    }
    if (memory_byte_count > 0u) {
        load_file->segments[0].bytes[0] = 0x7fu;
    }

    return 1;
}

static int expect_text(const char *label, const char *actual_text, const char *expected_text) {
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[machine-runtime] FAIL: %s mismatch\nactual:\n%s\n",
            label,
            actual_text ? actual_text : "<null>");
        return 0;
    }
    return 1;
}

static int expect_runtime_dump_text_for_profile(const char *label,
    const char *actual_text,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    size_t base_virtual_address) {
    char expected_dump[1024];

    memset(expected_dump, 0, sizeof(expected_dump));
    if (!build_expected_runtime_dump_text(
            expected_dump,
            sizeof(expected_dump),
            machine_elf_target_profile_name(profile),
            machine_elf_target_profile_name(origin_profile),
            machine_elf_relocation_semantics_name(semantics),
            base_virtual_address)) {
        return 0;
    }
    return expect_text(label, actual_text, expected_dump);
}

static int expect_runtime_report_dump_text_for_profile(const char *label,
    const char *actual_text,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    size_t base_virtual_address) {
    char expected_dump[4096];

    memset(expected_dump, 0, sizeof(expected_dump));
    if (!build_expected_runtime_report_dump_text(
            expected_dump,
            sizeof(expected_dump),
            machine_elf_target_profile_name(profile),
            machine_elf_target_profile_name(origin_profile),
            machine_elf_relocation_semantics_name(semantics),
            base_virtual_address)) {
        return 0;
    }
    return expect_text(label, actual_text, expected_dump);
}

static int build_resolved_machine_elf_artifacts(const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineElfFile *out_elf_file,
    MachineElfReport *out_elf_report,
    unsigned char **out_elf_bytes,
    size_t *out_elf_byte_count,
    MachineElfError *error) {
    if (!report || !out_elf_file || !out_elf_report || !out_elf_bytes || !out_elf_byte_count || !error) {
        return 0;
    }
    *out_elf_bytes = NULL;
    *out_elf_byte_count = 0u;
    machine_elf_file_init(out_elf_file);
    machine_elf_report_init(out_elf_report);
    memset(error, 0, sizeof(*error));
    return machine_elf_build_from_machine_ir_report_with_profile(report, profile, out_elf_file, error) &&
        machine_elf_build_report_from_machine_ir_report_with_profile(report, profile, out_elf_report, error) &&
        machine_elf_build_bytes_from_machine_ir_report_with_profile(
            report, profile, out_elf_bytes, out_elf_byte_count, error);
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

static int build_global_object_machine_ir_report(
    MachineIrAllocateRewriteReport *report,
    MachineIrError *error) {
    MachineIrGlobal *global = NULL;

    if (!report || !error) {
        return 0;
    }
    if (!build_resolved_machine_ir_report(report, error)) {
        return 0;
    }
    if (!machine_ir_program_append_global(&report->program, "g", &global, error) ||
        !global ||
        !machine_ir_program_append_global(&report->program, "h", &global, error)) {
        machine_ir_allocate_rewrite_report_free(report);
        return 0;
    }
    report->program.globals[1].has_initializer = 1;
    report->program.globals[1].initializer_value = 7;
    return 1;
}

static int verify_runtime_file_with_profile(const MachineRuntimeFile *runtime_file,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    size_t base_virtual_address) {
    MachineRuntimeHeaderSummary header_summary;
    MachineRuntimeMemorySummary memory_summary;
    MachineRuntimeStackSummary stack_summary_info;
    MachineRuntimeGapSummary gap_summary;
    MachineRuntimeLaunchSummary launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeSegmentSummary text_summary;
    MachineRuntimeSegmentSummary stack_summary;
    MachineRuntimeError runtime_error;
    const MachineRuntimeSegment *segment = NULL;
    const MachineRuntimeSegment *entry_segment = NULL;
    const MachineRuntimeSegment *stack_segment = NULL;
    unsigned char byte = 0xffu;
    unsigned char *flat_bytes = NULL;
    unsigned char *segment_bytes = NULL;
    unsigned char *window_bytes = NULL;
    char *dump_text = NULL;
    char expected_dump[1024];
    size_t segment_count = 0u;
    size_t mapped_byte_count = 0u;
    size_t executable_segment_count = 0u;
    size_t segment_index = 0u;
    size_t entry_segment_index = 0u;
    size_t stack_segment_index = 0u;
    size_t flat_byte_count = 0u;
    size_t flat_base_virtual_address = 0u;
    size_t segment_byte_count = 0u;
    size_t window_byte_count = 0u;
    size_t window_base_virtual_address = 0u;
    MachineElfArtifactSummary source_artifact_summary;
    int ok = 1;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&memory_summary, 0, sizeof(memory_summary));
    memset(&stack_summary_info, 0, sizeof(stack_summary_info));
    memset(&gap_summary, 0, sizeof(gap_summary));
    memset(&launch_summary, 0, sizeof(launch_summary));
    memset(&initial_stack_summary, 0, sizeof(initial_stack_summary));
    memset(&text_summary, 0, sizeof(text_summary));
    memset(&stack_summary, 0, sizeof(stack_summary));
    memset(&source_artifact_summary, 0, sizeof(source_artifact_summary));
    memset(&runtime_error, 0, sizeof(runtime_error));
    memset(expected_dump, 0, sizeof(expected_dump));
    if (!build_expected_runtime_dump_text(
            expected_dump,
            sizeof(expected_dump),
            machine_elf_target_profile_name(profile),
            machine_elf_target_profile_name(origin_profile),
            machine_elf_relocation_semantics_name(semantics),
            base_virtual_address)) {
        return 0;
    }

    if (!machine_runtime_verify_file(runtime_file, &runtime_error) ||
        !machine_runtime_file_get_summary(
            runtime_file, &segment_count, &mapped_byte_count, &executable_segment_count) ||
        segment_count != 2u || mapped_byte_count != 8192u || executable_segment_count != 1u ||
        !machine_runtime_file_get_source_elf_artifact_summary(runtime_file, &source_artifact_summary) ||
        source_artifact_summary.target_profile != profile ||
        source_artifact_summary.origin_profile != origin_profile ||
        source_artifact_summary.relocation_semantics != semantics ||
        !machine_runtime_file_get_header_summary(runtime_file, &header_summary) ||
        header_summary.target_profile != profile ||
        header_summary.base_virtual_address != base_virtual_address ||
        header_summary.entry_virtual_address != base_virtual_address ||
        header_summary.initial_stack_pointer != base_virtual_address + 0x3000u ||
        header_summary.segment_count != 2u ||
        header_summary.mapped_byte_count != 8192u ||
        !machine_runtime_file_get_memory_summary(runtime_file, &memory_summary) ||
        memory_summary.base_virtual_address != base_virtual_address ||
        memory_summary.end_virtual_address != base_virtual_address + 0x3000u ||
        memory_summary.span_byte_count != 12288u ||
        memory_summary.mapped_byte_count != 8192u ||
        memory_summary.entry_offset != 0u ||
        memory_summary.initial_stack_pointer_offset != 0x3000u ||
        !machine_runtime_file_get_stack_summary(runtime_file, &stack_summary_info) ||
        stack_summary_info.stack_segment_index != 1u ||
        stack_summary_info.base_virtual_address != base_virtual_address + 0x2000u ||
        stack_summary_info.end_virtual_address != base_virtual_address + 0x3000u ||
        stack_summary_info.byte_count != 4096u ||
        stack_summary_info.initial_stack_pointer != base_virtual_address + 0x3000u ||
        stack_summary_info.initial_stack_pointer_offset != 0x1000u ||
        !machine_runtime_file_get_gap_summary(runtime_file, &gap_summary) ||
        gap_summary.base_virtual_address != base_virtual_address + 0x1000u ||
        gap_summary.end_virtual_address != base_virtual_address + 0x2000u ||
        gap_summary.byte_count != 4096u ||
        !machine_runtime_file_get_launch_summary(runtime_file, &launch_summary) ||
        launch_summary.entry_virtual_address != base_virtual_address ||
        launch_summary.initial_stack_pointer != base_virtual_address + 0x3000u ||
        launch_summary.stack_segment_index != 1u ||
        launch_summary.stack_base_virtual_address != base_virtual_address + 0x2000u ||
        launch_summary.stack_end_virtual_address != base_virtual_address + 0x3000u ||
        launch_summary.stack_byte_count != 4096u ||
        !machine_runtime_file_get_initial_stack_summary(runtime_file, &initial_stack_summary) ||
        initial_stack_summary.word_byte_count != 4u ||
        initial_stack_summary.image_base_virtual_address != base_virtual_address + 0x3000u - 20u ||
        initial_stack_summary.image_end_virtual_address != base_virtual_address + 0x3000u ||
        initial_stack_summary.image_byte_count != 20u ||
        initial_stack_summary.argc != 0u ||
        initial_stack_summary.argc_virtual_address != base_virtual_address + 0x3000u - 20u ||
        initial_stack_summary.argv_terminator_virtual_address != base_virtual_address + 0x3000u - 16u ||
        initial_stack_summary.envp_terminator_virtual_address != base_virtual_address + 0x3000u - 12u ||
        initial_stack_summary.auxv_terminator_virtual_address != base_virtual_address + 0x3000u - 8u ||
        !machine_runtime_file_find_segment_by_name(runtime_file, ".text", &segment_index, &segment) ||
        !segment || segment_index != 0u ||
        !machine_runtime_file_find_segment_by_name(runtime_file, ".stack", &segment_index, &segment) ||
        !segment || segment_index != 1u ||
        !machine_runtime_file_find_segment_covering_virtual_address(
            runtime_file, base_virtual_address + 4u, &segment_index, &segment) ||
        !segment || segment_index != 0u ||
        machine_runtime_file_find_segment_covering_virtual_address(
            runtime_file, base_virtual_address + 0x1000u, &segment_index, &segment) ||
        !machine_runtime_file_find_segment_covering_virtual_address(
            runtime_file, base_virtual_address + 0x2000u + 7u, &segment_index, &segment) ||
        !segment || segment_index != 1u ||
        !machine_runtime_file_find_segment_covering_memory_offset(runtime_file, 128u, &segment_index, &segment) ||
        !segment || segment_index != 0u ||
        machine_runtime_file_find_segment_covering_memory_offset(runtime_file, 0x1000u, &segment_index, &segment) ||
        !machine_runtime_file_find_segment_covering_memory_offset(runtime_file, 0x2000u + 1u, &segment_index, &segment) ||
        !segment || segment_index != 1u ||
        !machine_runtime_file_get_entry_segment(runtime_file, &entry_segment_index, &entry_segment) ||
        !entry_segment || entry_segment_index != 0u ||
        !machine_runtime_file_get_stack_segment(runtime_file, &stack_segment_index, &stack_segment) ||
        !stack_segment || stack_segment_index != 1u ||
        !machine_runtime_file_get_executable_segment_count(runtime_file, &executable_segment_count) ||
        executable_segment_count != 1u ||
        !machine_runtime_file_get_executable_segment_by_index(runtime_file, 0u, &segment_index, &segment) ||
        !segment || segment_index != 0u ||
        !machine_runtime_segment_get_summary(entry_segment, &text_summary) ||
        !machine_runtime_segment_get_summary(stack_segment, &stack_summary) ||
        !text_summary.name || strcmp(text_summary.name, ".text") != 0 ||
        text_summary.kind != MACHINE_RUNTIME_SEGMENT_KIND_LOAD ||
        text_summary.load_segment_index != 0u ||
        text_summary.virtual_address != base_virtual_address ||
        text_summary.byte_count != 4096u ||
        !text_summary.readable || text_summary.writable || !text_summary.executable ||
        !stack_summary.name || strcmp(stack_summary.name, ".stack") != 0 ||
        stack_summary.kind != MACHINE_RUNTIME_SEGMENT_KIND_STACK ||
        stack_summary.load_segment_index != (size_t)-1 ||
        stack_summary.virtual_address != base_virtual_address + 0x2000u ||
        stack_summary.byte_count != 4096u ||
        !stack_summary.readable || !stack_summary.writable || stack_summary.executable ||
        !machine_runtime_file_get_memory_byte_at_virtual_address(runtime_file, base_virtual_address + 128u, &byte) ||
        byte != 0u ||
        !machine_runtime_file_get_memory_byte_at_virtual_address(runtime_file, base_virtual_address + 0x2000u + 16u, &byte) ||
        byte != 0u ||
        !machine_runtime_file_get_memory_byte_at_offset(runtime_file, 128u, &byte) ||
        byte != 0u ||
        !machine_runtime_file_get_memory_byte_at_offset(runtime_file, 0x2000u + 16u, &byte) ||
        byte != 0u ||
        machine_runtime_file_get_memory_byte_at_offset(runtime_file, 0x1000u, &byte) ||
        !machine_runtime_file_get_stack_byte_at_offset(runtime_file, 0u, &byte) ||
        byte != 0u ||
        !machine_runtime_file_get_stack_byte_at_offset(runtime_file, 16u, &byte) ||
        byte != 0u ||
        machine_runtime_file_get_stack_byte_at_offset(runtime_file, 4096u, &byte) ||
        !machine_runtime_file_copy_segment_bytes(runtime_file, 0u, &segment_bytes, &segment_byte_count, &runtime_error) ||
        segment_byte_count != 4096u ||
        segment_bytes[9] != 0u ||
        (free(segment_bytes), segment_bytes = NULL, segment_byte_count = 0u, 0) ||
        !machine_runtime_file_copy_stack_bytes(runtime_file, &segment_bytes, &segment_byte_count, &runtime_error) ||
        segment_byte_count != 4096u ||
        segment_bytes[0] != 0u ||
        (free(segment_bytes), segment_bytes = NULL, segment_byte_count = 0u, 0) ||
        !machine_runtime_file_copy_memory_window(
            runtime_file, 128u, 32u, &window_bytes, &window_byte_count, &window_base_virtual_address, &runtime_error) ||
        window_byte_count != 32u ||
        window_base_virtual_address != base_virtual_address + 128u ||
        window_bytes[0] != 0u ||
        window_bytes[31] != 0u ||
        (free(window_bytes), window_bytes = NULL, window_byte_count = 0u, 0) ||
        !machine_runtime_file_copy_stack_window(
            runtime_file, 32u, 48u, &window_bytes, &window_byte_count, &window_base_virtual_address, &runtime_error) ||
        window_byte_count != 48u ||
        window_base_virtual_address != base_virtual_address + 0x2000u + 32u ||
        window_bytes[0] != 0u ||
        window_bytes[47] != 0u ||
        (free(window_bytes), window_bytes = NULL, window_byte_count = 0u, 0) ||
        !machine_runtime_file_copy_initial_stack_window(
            runtime_file, 64u, &window_bytes, &window_byte_count, &window_base_virtual_address, &runtime_error) ||
        window_byte_count != 64u ||
        window_base_virtual_address != base_virtual_address + 0x3000u - 64u ||
        window_bytes[0] != 0u ||
        window_bytes[63] != 0u ||
        (free(window_bytes), window_bytes = NULL, window_byte_count = 0u, 0) ||
        !machine_runtime_file_copy_initial_stack_image(
            runtime_file, &window_bytes, &window_byte_count, &window_base_virtual_address, &runtime_error) ||
        window_byte_count != 20u ||
        window_base_virtual_address != base_virtual_address + 0x3000u - 20u ||
        window_bytes[0] != 0u ||
        window_bytes[19] != 0u ||
        (free(window_bytes), window_bytes = NULL, window_byte_count = 0u, 0) ||
        !machine_runtime_file_copy_gap_window(
            runtime_file, 32u, 48u, &window_bytes, &window_byte_count, &window_base_virtual_address, &runtime_error) ||
        window_byte_count != 48u ||
        window_base_virtual_address != base_virtual_address + 0x1000u + 32u ||
        window_bytes[0] != 0u ||
        window_bytes[47] != 0u ||
        !machine_runtime_file_copy_flat_memory_image(
            runtime_file, &flat_bytes, &flat_byte_count, &flat_base_virtual_address, &runtime_error) ||
        flat_byte_count != 12288u ||
        flat_base_virtual_address != base_virtual_address ||
        flat_bytes[9] != 0u ||
        flat_bytes[4096] != 0u ||
        flat_bytes[8192] != 0u ||
        !machine_runtime_dump_file(runtime_file, &dump_text, &runtime_error)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: %s validation mismatch: %s\n",
            context,
            runtime_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(window_bytes);
    free(segment_bytes);
    free(flat_bytes);
    free(dump_text);
    return ok;
}

static int verify_runtime_report_with_profile(const MachineRuntimeReport *report,
    const char *context,
    MachineElfTargetProfile profile,
    MachineElfTargetProfile origin_profile,
    MachineElfRelocationSemantics semantics,
    size_t base_virtual_address) {
    MachineRuntimeError runtime_error;
    MachineRuntimeReportOverviewArtifact overview_artifact;
    MachineRuntimeReportStackArtifact stack_artifact;
    MachineRuntimeReportGapArtifact gap_artifact;
    MachineRuntimeReportLaunchArtifact launch_artifact;
    MachineRuntimeReportInitialStackArtifact initial_stack_artifact;
    MachineRuntimeReportRegionArtifact region_artifact;
    MachineRuntimeReportSegmentArtifact segment_artifact;
    MachineRuntimeRegionSummary region_summary;
    const MachineRuntimeSegmentSummary *segment_summary = NULL;
    const MachineRuntimeHeaderSummary *header_summary = NULL;
    const MachineRuntimeTargetPolicySummary *target_policy_summary = NULL;
    const MachineRuntimeMemorySummary *memory_summary = NULL;
    const MachineRuntimeStackSummary *stack_summary_info = NULL;
    const MachineRuntimeGapSummary *gap_summary = NULL;
    const MachineRuntimeLaunchSummary *launch_summary = NULL;
    const MachineRuntimeInitialStackSummary *initial_stack_summary = NULL;
    const MachineElfArtifactSummary *source_artifact_summary = NULL;
    const MachineRuntimeFile *runtime_file = NULL;
    char *dump_text = NULL;
    char expected_dump[4096];
    size_t segment_count = 0u;
    size_t mapped_byte_count = 0u;
    size_t executable_segment_count = 0u;
    size_t filter_count = 0u;
    size_t segment_index = 0u;
    int ok = 1;

    memset(&runtime_error, 0, sizeof(runtime_error));
    memset(&overview_artifact, 0, sizeof(overview_artifact));
    memset(&stack_artifact, 0, sizeof(stack_artifact));
    memset(&gap_artifact, 0, sizeof(gap_artifact));
    memset(&launch_artifact, 0, sizeof(launch_artifact));
    memset(&initial_stack_artifact, 0, sizeof(initial_stack_artifact));
    memset(&region_artifact, 0, sizeof(region_artifact));
    memset(&segment_artifact, 0, sizeof(segment_artifact));
    memset(&region_summary, 0, sizeof(region_summary));
    memset(expected_dump, 0, sizeof(expected_dump));
    if (!build_expected_runtime_report_dump_text(
            expected_dump,
            sizeof(expected_dump),
            machine_elf_target_profile_name(profile),
            machine_elf_target_profile_name(origin_profile),
            machine_elf_relocation_semantics_name(semantics),
            base_virtual_address)) {
        return 0;
    }

    if (!machine_runtime_report_get_summary(
            report, &segment_count, &mapped_byte_count, &executable_segment_count) ||
        segment_count != 2u || mapped_byte_count != 8192u || executable_segment_count != 1u ||
        !machine_runtime_report_get_overview_artifact(report, &overview_artifact) ||
        !overview_artifact.header_summary || !overview_artifact.target_policy_summary ||
        !overview_artifact.memory_summary || !overview_artifact.entry_segment_summary ||
        !overview_artifact.stack_segment_summary ||
        overview_artifact.entry_segment_index != 0u ||
        overview_artifact.stack_segment_index != 1u ||
        !machine_runtime_report_get_file(report, &runtime_file) ||
        !runtime_file ||
        !machine_runtime_report_get_source_elf_artifact_summary_artifact(report, &source_artifact_summary) ||
        !source_artifact_summary ||
        !machine_runtime_report_get_header_summary_artifact(report, &header_summary) ||
        !header_summary ||
        header_summary->target_profile != profile ||
        header_summary->base_virtual_address != base_virtual_address ||
        header_summary->initial_stack_pointer != base_virtual_address + 0x3000u ||
        source_artifact_summary->target_profile != profile ||
        source_artifact_summary->origin_profile != origin_profile ||
        source_artifact_summary->relocation_semantics != semantics ||
        !machine_runtime_report_get_target_policy_summary_artifact(report, &target_policy_summary) ||
        !target_policy_summary ||
        target_policy_summary->stack_alignment != 4096u ||
        target_policy_summary->stack_byte_count != 4096u ||
        target_policy_summary->stack_gap_byte_count != 4096u ||
        !machine_runtime_report_get_memory_summary_artifact(report, &memory_summary) ||
        !memory_summary ||
        memory_summary->span_byte_count != 12288u ||
        memory_summary->mapped_byte_count != 8192u ||
        !machine_runtime_report_get_stack_summary_artifact(report, &stack_summary_info) ||
        !stack_summary_info ||
        stack_summary_info->stack_segment_index != 1u ||
        stack_summary_info->base_virtual_address != base_virtual_address + 0x2000u ||
        stack_summary_info->end_virtual_address != base_virtual_address + 0x3000u ||
        stack_summary_info->byte_count != 4096u ||
        stack_summary_info->initial_stack_pointer != base_virtual_address + 0x3000u ||
        stack_summary_info->initial_stack_pointer_offset != 0x1000u ||
        !machine_runtime_report_get_gap_summary_artifact(report, &gap_summary) ||
        !gap_summary ||
        gap_summary->base_virtual_address != base_virtual_address + 0x1000u ||
        gap_summary->end_virtual_address != base_virtual_address + 0x2000u ||
        gap_summary->byte_count != 4096u ||
        !machine_runtime_report_get_launch_summary_artifact(report, &launch_summary) ||
        !launch_summary ||
        launch_summary->entry_virtual_address != base_virtual_address ||
        launch_summary->initial_stack_pointer != base_virtual_address + 0x3000u ||
        launch_summary->stack_segment_index != 1u ||
        launch_summary->stack_base_virtual_address != base_virtual_address + 0x2000u ||
        launch_summary->stack_end_virtual_address != base_virtual_address + 0x3000u ||
        launch_summary->stack_byte_count != 4096u ||
        !machine_runtime_report_get_initial_stack_summary_artifact(report, &initial_stack_summary) ||
        !initial_stack_summary ||
        initial_stack_summary->word_byte_count != 4u ||
        initial_stack_summary->image_base_virtual_address != base_virtual_address + 0x3000u - 20u ||
        initial_stack_summary->image_end_virtual_address != base_virtual_address + 0x3000u ||
        initial_stack_summary->image_byte_count != 20u ||
        initial_stack_summary->argc != 0u ||
        initial_stack_summary->argc_virtual_address != base_virtual_address + 0x3000u - 20u ||
        initial_stack_summary->argv_terminator_virtual_address != base_virtual_address + 0x3000u - 16u ||
        initial_stack_summary->envp_terminator_virtual_address != base_virtual_address + 0x3000u - 12u ||
        initial_stack_summary->auxv_terminator_virtual_address != base_virtual_address + 0x3000u - 8u ||
        !machine_runtime_report_get_stack_artifact(report, &stack_artifact) ||
        !stack_artifact.stack_summary ||
        stack_artifact.stack_summary->stack_segment_index != 1u ||
        !stack_artifact.stack_segment_artifact.is_stack_segment ||
        !machine_runtime_report_get_gap_artifact(report, &gap_artifact) ||
        !gap_artifact.gap_summary ||
        gap_artifact.gap_summary->byte_count != 4096u ||
        !machine_runtime_report_get_launch_artifact(report, &launch_artifact) ||
        !launch_artifact.launch_summary ||
        launch_artifact.launch_summary->initial_stack_pointer != base_virtual_address + 0x3000u ||
        !launch_artifact.entry_segment_artifact.is_entry_segment ||
        !launch_artifact.stack_artifact.stack_segment_artifact.is_stack_segment ||
        !machine_runtime_report_get_initial_stack_artifact(report, &initial_stack_artifact) ||
        !initial_stack_artifact.initial_stack_summary ||
        initial_stack_artifact.initial_stack_summary->image_byte_count != 20u ||
        !initial_stack_artifact.stack_artifact.stack_segment_artifact.is_stack_segment ||
        !initial_stack_artifact.launch_artifact.entry_segment_artifact.is_entry_segment ||
        !machine_runtime_report_get_region_count(report, &segment_count) ||
        segment_count != 3u ||
        !machine_runtime_report_get_region_summary(report, 0u, &region_summary) ||
        region_summary.kind != MACHINE_RUNTIME_REGION_LOAD ||
        region_summary.segment_index != 0u ||
        strcmp(region_summary.name, ".text") != 0 ||
        !machine_runtime_report_get_region_summary(report, 1u, &region_summary) ||
        region_summary.kind != MACHINE_RUNTIME_REGION_STACK ||
        region_summary.segment_index != 1u ||
        strcmp(region_summary.name, ".stack") != 0 ||
        !machine_runtime_report_get_region_summary(report, 2u, &region_summary) ||
        region_summary.kind != MACHINE_RUNTIME_REGION_GAP ||
        region_summary.segment_index != (size_t)-1 ||
        strcmp(region_summary.name, ".gap") != 0 ||
        !machine_runtime_report_get_region_artifact(report, 2u, &region_artifact) ||
        region_artifact.summary.kind != MACHINE_RUNTIME_REGION_GAP ||
        !region_artifact.has_gap_artifact ||
        !machine_runtime_report_find_region_summary_covering_virtual_address(
            report, base_virtual_address + 0x1000u + 8u, &segment_index, &region_summary) ||
        segment_index != 2u || region_summary.kind != MACHINE_RUNTIME_REGION_GAP ||
        !machine_runtime_report_find_region_artifact_covering_memory_offset(
            report, 0x1000u + 8u, &segment_index, &region_artifact) ||
        segment_index != 2u || region_artifact.summary.kind != MACHINE_RUNTIME_REGION_GAP ||
        !machine_runtime_report_get_region_filter_count(report, MACHINE_RUNTIME_REGION_FILTER_LOAD, &filter_count) ||
        filter_count != 1u ||
        !machine_runtime_report_get_region_filter_count(report, MACHINE_RUNTIME_REGION_FILTER_GAP, &filter_count) ||
        filter_count != 1u ||
        !machine_runtime_report_get_region_filter_count(report, MACHINE_RUNTIME_REGION_FILTER_STACK, &filter_count) ||
        filter_count != 1u ||
        !machine_runtime_report_get_region_summary_by_filter_index(
            report, MACHINE_RUNTIME_REGION_FILTER_GAP, 0u, &segment_index, &region_summary) ||
        segment_index != 2u || region_summary.kind != MACHINE_RUNTIME_REGION_GAP ||
        !machine_runtime_report_get_region_artifact_by_filter_index(
            report, MACHINE_RUNTIME_REGION_FILTER_STACK, 0u, &segment_index, &region_artifact) ||
        segment_index != 1u || region_artifact.summary.kind != MACHINE_RUNTIME_REGION_STACK ||
        !overview_artifact.stack_summary || !overview_artifact.gap_summary ||
        !overview_artifact.launch_summary || !overview_artifact.initial_stack_summary ||
        !machine_runtime_report_get_segment_filter_count(
            report, MACHINE_RUNTIME_SEGMENT_FILTER_EXECUTABLE, &filter_count) ||
        filter_count != 1u ||
        !machine_runtime_report_get_segment_summary_by_filter_index(
            report, MACHINE_RUNTIME_SEGMENT_FILTER_EXECUTABLE, 0u, &segment_index, &segment_summary) ||
        !segment_summary || segment_index != 0u ||
        strcmp(segment_summary->name, ".text") != 0 ||
        !machine_runtime_report_get_segment_filter_count(
            report, MACHINE_RUNTIME_SEGMENT_FILTER_STACK, &filter_count) ||
        filter_count != 1u ||
        !machine_runtime_report_get_segment_artifact_by_filter_index(
            report, MACHINE_RUNTIME_SEGMENT_FILTER_STACK, 0u, &segment_index, &segment_artifact) ||
        segment_index != 1u || !segment_artifact.is_stack_segment ||
        !machine_runtime_report_find_segment_summary_by_name(report, ".stack", &segment_index, &segment_summary) ||
        !segment_summary || segment_index != 1u ||
        !machine_runtime_report_find_segment_artifact_covering_virtual_address(
            report, base_virtual_address + 0x2000u + 1u, &segment_index, &segment_artifact) ||
        segment_index != 1u || !segment_artifact.is_stack_segment ||
        !machine_runtime_report_find_segment_summary_covering_memory_offset(
            report, 128u, &segment_index, &segment_summary) ||
        segment_index != 0u || !segment_summary ||
        strcmp(segment_summary->name, ".text") != 0 ||
        !machine_runtime_report_find_segment_artifact_covering_memory_offset(
            report, 0x2000u + 8u, &segment_index, &segment_artifact) ||
        segment_index != 1u || !segment_artifact.is_stack_segment ||
        !machine_runtime_report_find_segment_summary_covering_memory_offset(
            report, 0x3000u - 1u, &segment_index, &segment_summary) ||
        segment_index != 1u || !segment_summary ||
        strcmp(segment_summary->name, ".stack") != 0 ||
        !machine_runtime_report_overview_artifact_get_segment_summary(
            &overview_artifact, MACHINE_RUNTIME_SEGMENT_FILTER_ENTRY, 0u, &segment_index, &segment_summary) ||
        segment_index != 0u || !segment_summary ||
        !machine_runtime_report_overview_artifact_get_stack_summary_artifact(
            &overview_artifact, &stack_summary_info) ||
        !stack_summary_info ||
        stack_summary_info->stack_segment_index != 1u ||
        !machine_runtime_report_overview_artifact_get_gap_summary_artifact(
            &overview_artifact, &gap_summary) ||
        !gap_summary ||
        gap_summary->byte_count != 4096u ||
        !machine_runtime_report_overview_artifact_get_launch_summary_artifact(
            &overview_artifact, &launch_summary) ||
        !launch_summary ||
        launch_summary->initial_stack_pointer != base_virtual_address + 0x3000u ||
        !machine_runtime_report_overview_artifact_get_initial_stack_summary_artifact(
            &overview_artifact, &initial_stack_summary) ||
        !initial_stack_summary ||
        initial_stack_summary->image_base_virtual_address != base_virtual_address + 0x3000u - 20u ||
        !machine_runtime_report_overview_artifact_get_stack_artifact(
            &overview_artifact, &stack_artifact) ||
        !stack_artifact.stack_summary ||
        stack_artifact.stack_summary->stack_segment_index != 1u ||
        !machine_runtime_report_overview_artifact_get_region_summary(
            &overview_artifact, MACHINE_RUNTIME_REGION_FILTER_GAP, 0u, &segment_index, &region_summary) ||
        segment_index != 2u || region_summary.kind != MACHINE_RUNTIME_REGION_GAP ||
        !machine_runtime_report_overview_artifact_get_region_artifact(
            &overview_artifact, MACHINE_RUNTIME_REGION_FILTER_STACK, 0u, &segment_index, &region_artifact) ||
        segment_index != 1u || region_artifact.summary.kind != MACHINE_RUNTIME_REGION_STACK ||
        !machine_runtime_report_overview_artifact_get_launch_artifact(
            &overview_artifact, &launch_artifact) ||
        !launch_artifact.launch_summary ||
        launch_artifact.launch_summary->entry_virtual_address != base_virtual_address ||
        !machine_runtime_report_overview_artifact_get_initial_stack_artifact(
            &overview_artifact, &initial_stack_artifact) ||
        !initial_stack_artifact.initial_stack_summary ||
        initial_stack_artifact.initial_stack_summary->image_end_virtual_address != base_virtual_address + 0x3000u ||
        strcmp(segment_summary->name, ".text") != 0 ||
        !machine_runtime_dump_report(report, &dump_text, &runtime_error)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: %s report mismatch: %s\n",
            context,
            runtime_error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_text(context, dump_text, expected_dump);

cleanup:
    free(dump_text);
    return ok;
}

static int test_machine_runtime_mainline(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineLoadFile load_file;
    MachineLoadReport load_report;
    MachineRuntimeFile runtime_file;
    MachineRuntimeReport runtime_report;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_load_file_init(&load_file);
    machine_load_report_init(&load_report);
    machine_runtime_file_init(&runtime_file);
    machine_runtime_report_init(&runtime_report);

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: runtime setup failed: %s\n",
            ir_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_load_build_from_machine_ir_report(&ir_report, &load_file, NULL) ||
        !machine_load_build_report_from_machine_ir_report(&ir_report, &load_report, NULL) ||
        !machine_runtime_build_from_machine_load_file(&load_file, &runtime_file, NULL)) {
        fprintf(stderr, "[machine-runtime] FAIL: generic runtime build failed\n");
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-generic-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_file_free(&runtime_file);
    if (!machine_runtime_build_from_machine_load_report(&load_report, &runtime_file, NULL)) {
        fprintf(stderr, "[machine-runtime] FAIL: generic runtime report bridge failed\n");
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-generic-load-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    if (!machine_runtime_build_report_from_machine_load_file(&load_file, &runtime_report, NULL)) {
        fprintf(stderr, "[machine-runtime] FAIL: generic runtime report build failed\n");
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_ir_report(&ir_report, &runtime_report, NULL)) {
        fprintf(stderr, "[machine-runtime] FAIL: generic runtime ir-report bridge failed\n");
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-ir-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

cleanup:
    machine_runtime_report_free(&runtime_report);
    machine_runtime_file_free(&runtime_file);
    machine_load_report_free(&load_report);
    machine_load_file_free(&load_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_runtime_bridge_matrix(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineElfFile elf_file;
    MachineElfReport elf_report;
    MachineImageFile image_file;
    MachineImageReport image_report;
    MachineExecFile exec_file;
    MachineExecReport exec_report;
    MachineLoadFile load_file;
    MachineLoadReport load_report;
    MachineRuntimeFile runtime_file;
    MachineRuntimeFile cloned_runtime;
    MachineRuntimeReport runtime_report;
    MachineElfError elf_error;
    MachineImageError image_error;
    MachineExecError exec_error;
    MachineLoadError load_error;
    MachineRuntimeError runtime_error;
    unsigned char *elf_bytes = NULL;
    size_t elf_byte_count = 0u;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_elf_file_init(&elf_file);
    machine_elf_report_init(&elf_report);
    memset(&elf_error, 0, sizeof(elf_error));
    machine_image_file_init(&image_file);
    machine_image_report_init(&image_report);
    memset(&image_error, 0, sizeof(image_error));
    machine_exec_file_init(&exec_file);
    machine_exec_report_init(&exec_report);
    memset(&exec_error, 0, sizeof(exec_error));
    machine_load_file_init(&load_file);
    machine_load_report_init(&load_report);
    memset(&load_error, 0, sizeof(load_error));
    machine_runtime_file_init(&runtime_file);
    machine_runtime_file_init(&cloned_runtime);
    machine_runtime_report_init(&runtime_report);
    memset(&runtime_error, 0, sizeof(runtime_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !build_resolved_machine_elf_artifacts(
            &ir_report,
            MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
            &elf_file,
            &elf_report,
            &elf_bytes,
            &elf_byte_count,
            &elf_error) ||
        !machine_image_build_from_machine_ir_report(&ir_report, &image_file, &image_error) ||
        !machine_image_build_report_from_machine_ir_report(&ir_report, &image_report, &image_error) ||
        !machine_exec_build_from_machine_ir_report(&ir_report, &exec_file, &exec_error) ||
        !machine_exec_build_report_from_machine_ir_report(&ir_report, &exec_report, &exec_error) ||
        !machine_load_build_from_machine_ir_report(&ir_report, &load_file, &load_error) ||
        !machine_load_build_report_from_machine_ir_report(&ir_report, &load_report, &load_error)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: bridge-matrix setup failed: ir=%s elf=%s image=%s exec=%s load=%s\n",
            ir_error.message,
            elf_error.message,
            image_error.message,
            exec_error.message,
            load_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_runtime_build_from_machine_exec_file(&exec_file, &runtime_file, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime from exec file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-generic-exec-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_file_free(&runtime_file);
    if (!machine_runtime_build_from_machine_exec_report(&exec_report, &runtime_file, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime from exec report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-generic-exec-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_file_free(&runtime_file);
    if (!machine_runtime_build_from_machine_image_file(&image_file, &runtime_file, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime from image file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-generic-image-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_file_free(&runtime_file);
    if (!machine_runtime_build_from_machine_image_report(&image_report, &runtime_file, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime from image report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-generic-image-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_file_free(&runtime_file);
    if (!machine_runtime_build_from_machine_elf_file(&elf_file, &runtime_file, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime from elf file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-generic-elf-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    if (!machine_runtime_clone_file(&runtime_file, &cloned_runtime, &runtime_error) ||
        cloned_runtime.segments == runtime_file.segments ||
        cloned_runtime.segments[0].name == runtime_file.segments[0].name ||
        cloned_runtime.segments[0].bytes == runtime_file.segments[0].bytes) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime clone failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &cloned_runtime,
        "runtime-generic-cloned-elf-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_file_free(&runtime_file);
    if (!machine_runtime_build_from_machine_elf_report(&elf_report, &runtime_file, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime from elf report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-generic-elf-report-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_file_free(&runtime_file);
    if (!machine_runtime_build_from_machine_elf_bytes(
            elf_bytes, elf_byte_count, &runtime_file, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime from elf bytes failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-generic-elf-bytes-file",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE,
        0x1000u);

    if (!machine_runtime_build_report_from_file(&runtime_file, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime report from file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-file-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE,
        0x1000u);

    runtime_report.header_summary.initial_stack_pointer = 0u;
    runtime_report.launch_summary.initial_stack_pointer = 0u;
    runtime_report.initial_stack_summary.image_byte_count = 0u;
    if (!machine_runtime_report_refresh(&runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime report refresh failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-refreshed-file-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE,
        0x1000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_load_report(&load_report, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime report from load report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-load-report-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_exec_file(&exec_file, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime report from exec file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-exec-file-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_exec_report(&exec_report, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime report from exec report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-exec-report-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_image_file(&image_file, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime report from image file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-image-file-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_image_report(&image_report, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime report from image report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-image-report-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_elf_file(&elf_file, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime report from elf file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-elf-file-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_elf_report(&elf_report, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime report from elf report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-elf-report-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_elf_bytes(
            elf_bytes, elf_byte_count, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: runtime report from elf bytes failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-generic-elf-bytes-report",
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE,
        0x1000u);

cleanup:
    machine_runtime_report_free(&runtime_report);
    machine_runtime_file_free(&cloned_runtime);
    machine_runtime_file_free(&runtime_file);
    machine_load_report_free(&load_report);
    machine_load_file_free(&load_file);
    machine_exec_report_free(&exec_report);
    machine_exec_file_free(&exec_file);
    machine_image_report_free(&image_report);
    machine_image_file_free(&image_file);
    free(elf_bytes);
    machine_elf_report_free(&elf_report);
    machine_elf_file_free(&elf_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_runtime_dump_wrappers(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineElfFile elf_file;
    MachineElfReport elf_report;
    MachineImageFile image_file;
    MachineImageReport image_report;
    MachineExecFile exec_file;
    MachineExecReport exec_report;
    MachineLoadFile load_file;
    MachineLoadReport load_report;
    MachineRuntimeFile runtime_file;
    MachineElfError elf_error;
    MachineImageError image_error;
    MachineExecError exec_error;
    MachineLoadError load_error;
    MachineRuntimeError runtime_error;
    unsigned char *elf_bytes = NULL;
    size_t elf_byte_count = 0u;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_elf_file_init(&elf_file);
    machine_elf_report_init(&elf_report);
    memset(&elf_error, 0, sizeof(elf_error));
    machine_image_file_init(&image_file);
    machine_image_report_init(&image_report);
    memset(&image_error, 0, sizeof(image_error));
    machine_exec_file_init(&exec_file);
    machine_exec_report_init(&exec_report);
    memset(&exec_error, 0, sizeof(exec_error));
    machine_load_file_init(&load_file);
    machine_load_report_init(&load_report);
    memset(&load_error, 0, sizeof(load_error));
    machine_runtime_file_init(&runtime_file);
    memset(&runtime_error, 0, sizeof(runtime_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !build_resolved_machine_elf_artifacts(
            &ir_report,
            MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
            &elf_file,
            &elf_report,
            &elf_bytes,
            &elf_byte_count,
            &elf_error) ||
        !machine_image_build_from_machine_ir_report(&ir_report, &image_file, &image_error) ||
        !machine_image_build_report_from_machine_ir_report(&ir_report, &image_report, &image_error) ||
        !machine_exec_build_from_machine_ir_report(&ir_report, &exec_file, &exec_error) ||
        !machine_exec_build_report_from_machine_ir_report(&ir_report, &exec_report, &exec_error) ||
        !machine_load_build_from_machine_ir_report(&ir_report, &load_file, &load_error) ||
        !machine_load_build_report_from_machine_ir_report(&ir_report, &load_report, &load_error) ||
        !machine_runtime_build_from_machine_ir_report(&ir_report, &runtime_file, &runtime_error)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: dump-wrapper setup failed: ir=%s elf=%s image=%s exec=%s load=%s runtime=%s\n",
            ir_error.message,
            elf_error.message,
            image_error.message,
            exec_error.message,
            load_error.message,
            runtime_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_runtime_build_dump_from_file(&runtime_file, &dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: dump from file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from file",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    free(dump_text);
    dump_text = NULL;

    if (!machine_runtime_build_dump_from_machine_load_file(&load_file, &dump_text, &runtime_error) ||
        !machine_runtime_build_dump_from_machine_load_report(&load_report, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: dump from load artifacts failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from load file",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from load report",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    if (!machine_runtime_build_dump_from_machine_exec_file(&exec_file, &dump_text, &runtime_error) ||
        !machine_runtime_build_dump_from_machine_exec_report(&exec_report, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: dump from exec artifacts failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from exec file",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from exec report",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    if (!machine_runtime_build_dump_from_machine_image_file(&image_file, &dump_text, &runtime_error) ||
        !machine_runtime_build_dump_from_machine_image_report(&image_report, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: dump from image artifacts failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from image file",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from image report",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    if (!machine_runtime_build_dump_from_machine_elf_file(&elf_file, &dump_text, &runtime_error) ||
        !machine_runtime_build_dump_from_machine_elf_report(&elf_report, &report_dump_text, &runtime_error) ||
        !machine_runtime_build_dump_from_machine_elf_bytes(
            elf_bytes, elf_byte_count, &dump_text, &runtime_error) ||
        !machine_runtime_build_dump_from_machine_ir_report(&ir_report, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: dump from elf/ir artifacts failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from elf bytes",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE,
        0x1000u);
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from ir report",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    if (!machine_runtime_build_report_dump_from_file(&runtime_file, &dump_text, &runtime_error) ||
        !machine_runtime_build_report_dump_from_machine_load_file(&load_file, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: report dump from file/load file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from file",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from load file",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    if (!machine_runtime_build_report_dump_from_machine_load_report(&load_report, &dump_text, &runtime_error) ||
        !machine_runtime_build_report_dump_from_machine_exec_file(&exec_file, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: report dump from load report/exec file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from load report",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from exec file",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    if (!machine_runtime_build_report_dump_from_machine_exec_report(&exec_report, &dump_text, &runtime_error) ||
        !machine_runtime_build_report_dump_from_machine_image_file(&image_file, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: report dump from exec report/image file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from exec report",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from image file",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    if (!machine_runtime_build_report_dump_from_machine_image_report(&image_report, &dump_text, &runtime_error) ||
        !machine_runtime_build_report_dump_from_machine_elf_file(&elf_file, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: report dump from image report/elf file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from image report",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from elf file",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    if (!machine_runtime_build_report_dump_from_machine_elf_report(&elf_report, &dump_text, &runtime_error) ||
        !machine_runtime_build_report_dump_from_machine_elf_bytes(
            elf_bytes, elf_byte_count, &report_dump_text, &runtime_error) ||
        !machine_runtime_build_report_dump_from_machine_ir_report(&ir_report, &dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: report dump from elf/ir artifacts failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from ir report",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x1000u);
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from elf bytes",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE,
        0x1000u);

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_runtime_file_free(&runtime_file);
    machine_load_report_free(&load_report);
    machine_load_file_free(&load_file);
    machine_exec_report_free(&exec_report);
    machine_exec_file_free(&exec_file);
    machine_image_report_free(&image_report);
    machine_image_file_free(&image_file);
    free(elf_bytes);
    machine_elf_report_free(&elf_report);
    machine_elf_file_free(&elf_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_runtime_profiled_wrappers(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineElfFile elf_file;
    MachineElfReport elf_report;
    MachineRuntimeFile runtime_file;
    MachineRuntimeReport runtime_report;
    MachineElfError elf_error;
    MachineRuntimeError runtime_error;
    unsigned char *elf_bytes = NULL;
    size_t elf_byte_count = 0u;
    char *dump_text = NULL;
    char *report_dump_text = NULL;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_elf_file_init(&elf_file);
    machine_elf_report_init(&elf_report);
    memset(&elf_error, 0, sizeof(elf_error));
    machine_runtime_file_init(&runtime_file);
    machine_runtime_report_init(&runtime_report);
    memset(&runtime_error, 0, sizeof(runtime_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !build_resolved_machine_elf_artifacts(
            &ir_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &elf_file,
            &elf_report,
            &elf_bytes,
            &elf_byte_count,
            &elf_error)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: profiled-wrapper setup failed: ir=%s elf=%s\n",
            ir_error.message,
            elf_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_runtime_build_from_machine_elf_file_with_profile(
            &elf_file, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &runtime_file, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: profiled runtime from elf file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-i386-elf-file",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);

    machine_runtime_file_free(&runtime_file);
    if (!machine_runtime_build_from_machine_elf_report_with_profile(
            &elf_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &runtime_file, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: profiled runtime from elf report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-i386-elf-report-file",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);

    machine_runtime_file_free(&runtime_file);
    if (!machine_runtime_build_from_machine_elf_bytes_with_profile(
            elf_bytes, elf_byte_count, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &runtime_file, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: profiled runtime from elf bytes failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-i386-elf-bytes-file",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE,
        0x08048000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_elf_file_with_profile(
            &elf_file, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: profiled runtime report from elf file failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-i386-elf-file-report",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_elf_report_with_profile(
            &elf_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: profiled runtime report from elf report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-i386-elf-report-report",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);

    machine_runtime_report_free(&runtime_report);
    if (!machine_runtime_build_report_from_machine_elf_bytes_with_profile(
            elf_bytes, elf_byte_count, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &runtime_report, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: profiled runtime report from elf bytes failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-i386-elf-bytes-report",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE,
        0x08048000u);

    if (!machine_runtime_build_dump_from_machine_elf_file_with_profile(
            &elf_file, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &runtime_error) ||
        !machine_runtime_build_dump_from_machine_elf_report_with_profile(
            &elf_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: profiled dump from elf file/report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from profiled elf file",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from profiled elf report",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);
    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    if (!machine_runtime_build_dump_from_machine_elf_bytes_with_profile(
            elf_bytes, elf_byte_count, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &runtime_error) ||
        !machine_runtime_build_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: profiled dump from elf bytes/ir report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from profiled elf bytes",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE,
        0x08048000u);
    ok &= expect_runtime_dump_text_for_profile(
        "runtime dump from profiled ir report",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);
    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    if (!machine_runtime_build_report_dump_from_machine_elf_file_with_profile(
            &elf_file, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &runtime_error) ||
        !machine_runtime_build_report_dump_from_machine_elf_report_with_profile(
            &elf_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: profiled report dump from elf file/report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from profiled elf file",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from profiled elf report",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);
    free(dump_text);
    dump_text = NULL;
    free(report_dump_text);
    report_dump_text = NULL;

    if (!machine_runtime_build_report_dump_from_machine_elf_bytes_with_profile(
            elf_bytes, elf_byte_count, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &dump_text, &runtime_error) ||
        !machine_runtime_build_report_dump_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &report_dump_text, &runtime_error)) {
        fprintf(stderr, "[machine-runtime] FAIL: profiled report dump from elf bytes/ir report failed: %s\n", runtime_error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from profiled elf bytes",
        dump_text,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE,
        0x08048000u);
    ok &= expect_runtime_report_dump_text_for_profile(
        "runtime report dump from profiled ir report",
        report_dump_text,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);

cleanup:
    free(report_dump_text);
    free(dump_text);
    machine_runtime_report_free(&runtime_report);
    machine_runtime_file_free(&runtime_file);
    free(elf_bytes);
    machine_elf_report_free(&elf_report);
    machine_elf_file_free(&elf_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_runtime_profile_bridge(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineRuntimeFile runtime_file;
    MachineRuntimeReport runtime_report;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_runtime_file_init(&runtime_file);
    machine_runtime_report_init(&runtime_report);

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: i386 setup failed: %s\n",
            ir_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_runtime_build_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &runtime_file, NULL)) {
        fprintf(stderr, "[machine-runtime] FAIL: i386 runtime file bridge failed\n");
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_file_with_profile(
        &runtime_file,
        "runtime-i386-file",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);

    if (!machine_runtime_build_report_from_machine_ir_report_with_profile(
            &ir_report, MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW, &runtime_report, NULL)) {
        fprintf(stderr, "[machine-runtime] FAIL: i386 runtime report bridge failed\n");
        ok = 0;
        goto cleanup;
    }
    ok &= verify_runtime_report_with_profile(
        &runtime_report,
        "runtime-i386-report",
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
        MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
        0x08048000u);

cleanup:
    machine_runtime_report_free(&runtime_report);
    machine_runtime_file_free(&runtime_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_runtime_zero_gap_region_and_report_surface(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineRuntimeFile runtime_file;
    MachineRuntimeReport runtime_report;
    MachineRuntimeError runtime_error;
    MachineRuntimeGapSummary gap_summary;
    MachineRuntimeRegionSummary region_summary;
    size_t region_count = 0u;
    size_t filter_count = 0u;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    memset(&ir_error, 0, sizeof(ir_error));
    machine_runtime_file_init(&runtime_file);
    machine_runtime_report_init(&runtime_report);
    memset(&runtime_error, 0, sizeof(runtime_error));
    memset(&gap_summary, 0, sizeof(gap_summary));
    memset(&region_summary, 0, sizeof(region_summary));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_runtime_build_from_machine_ir_report(&ir_report, &runtime_file, &runtime_error)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: zero-gap setup failed: ir=%s runtime=%s\n",
            ir_error.message,
            runtime_error.message);
        ok = 0;
        goto cleanup;
    }

    runtime_file.segments[runtime_file.stack_segment_index].virtual_address =
        runtime_file.segments[0].virtual_address + runtime_file.segments[0].byte_count;
    runtime_file.initial_stack_pointer =
        runtime_file.segments[runtime_file.stack_segment_index].virtual_address +
        runtime_file.segments[runtime_file.stack_segment_index].byte_count;

    if (!machine_runtime_verify_file(&runtime_file, &runtime_error) ||
        !machine_runtime_file_get_gap_summary(&runtime_file, &gap_summary) ||
        gap_summary.byte_count != 0u ||
        !machine_runtime_file_get_region_count(&runtime_file, &region_count) ||
        region_count != 3u ||
        !machine_runtime_build_report_from_file(&runtime_file, &runtime_report, &runtime_error) ||
        !machine_runtime_report_get_region_filter_count(&runtime_report, MACHINE_RUNTIME_REGION_FILTER_GAP, &filter_count) ||
        filter_count != 1u ||
        !machine_runtime_report_get_region_summary(&runtime_report, 2u, &region_summary) ||
        region_summary.kind != MACHINE_RUNTIME_REGION_GAP ||
        region_summary.byte_count != 0u) {
        fprintf(stderr, "[machine-runtime] FAIL: zero-gap runtime surface mismatch: %s\n", runtime_error.message);
        ok = 0;
    }

cleanup:
    machine_runtime_report_free(&runtime_report);
    machine_runtime_file_free(&runtime_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_runtime_region_load_filter_survives_segment_reorder(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineRuntimeFile runtime_file;
    MachineRuntimeReport runtime_report;
    MachineRuntimeError runtime_error;
    MachineRuntimeSegment swapped_segment;
    MachineRuntimeRegionSummary region_summary;
    size_t filter_count = 0u;
    size_t region_index = 0u;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    machine_runtime_file_init(&runtime_file);
    machine_runtime_report_init(&runtime_report);
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&runtime_error, 0, sizeof(runtime_error));
    memset(&swapped_segment, 0, sizeof(swapped_segment));
    memset(&region_summary, 0, sizeof(region_summary));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_runtime_build_from_machine_ir_report(&ir_report, &runtime_file, &runtime_error)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: region-reorder setup failed: ir=%s runtime=%s\n",
            ir_error.message,
            runtime_error.message);
        ok = 0;
        goto cleanup;
    }

    swapped_segment = runtime_file.segments[0];
    runtime_file.segments[0] = runtime_file.segments[1];
    runtime_file.segments[1] = swapped_segment;
    runtime_file.stack_segment_index = 0u;

    if (!machine_runtime_verify_file(&runtime_file, &runtime_error) ||
        !machine_runtime_build_report_from_file(&runtime_file, &runtime_report, &runtime_error) ||
        !machine_runtime_report_get_region_filter_count(
            &runtime_report, MACHINE_RUNTIME_REGION_FILTER_LOAD, &filter_count) ||
        filter_count != 1u ||
        !machine_runtime_report_get_region_summary_by_filter_index(
            &runtime_report,
            MACHINE_RUNTIME_REGION_FILTER_LOAD,
            0u,
            &region_index,
            &region_summary) ||
        region_index != 1u ||
        region_summary.kind != MACHINE_RUNTIME_REGION_LOAD ||
        !region_summary.name ||
        strcmp(region_summary.name, ".text") != 0) {
        fprintf(stderr,
            "[machine-runtime] FAIL: region-reorder load filter mismatch: %s\n",
            runtime_error.message);
        ok = 0;
    }

cleanup:
    machine_runtime_report_free(&runtime_report);
    machine_runtime_file_free(&runtime_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_runtime_window_copy_clamps_huge_lengths(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineRuntimeFile runtime_file;
    MachineRuntimeError runtime_error;
    unsigned char *window_bytes = NULL;
    size_t window_byte_count = 0u;
    size_t window_base_virtual_address = 0u;
    size_t stack_base_virtual_address = 0u;
    size_t stack_memory_offset = 0u;
    size_t gap_base_virtual_address = 0u;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    machine_runtime_file_init(&runtime_file);
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&runtime_error, 0, sizeof(runtime_error));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_runtime_build_from_machine_ir_report(&ir_report, &runtime_file, &runtime_error)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: overflow clamp setup failed: ir=%s runtime=%s\n",
            ir_error.message,
            runtime_error.message);
        ok = 0;
        goto cleanup;
    }

    gap_base_virtual_address =
        runtime_file.segments[0].virtual_address + runtime_file.segments[0].byte_count;
    stack_base_virtual_address =
        runtime_file.segments[runtime_file.stack_segment_index].virtual_address;
    stack_memory_offset = stack_base_virtual_address -
        runtime_file.segments[0].virtual_address;

    if (!machine_runtime_file_copy_memory_window(
            &runtime_file,
            stack_memory_offset + 32u,
            SIZE_MAX,
            &window_bytes,
            &window_byte_count,
            &window_base_virtual_address,
            &runtime_error) ||
        window_byte_count != 4064u ||
        window_base_virtual_address != stack_base_virtual_address + 32u ||
        (window_byte_count > 0u && window_bytes[window_byte_count - 1u] != 0u) ||
        (free(window_bytes), window_bytes = NULL, 0) ||
        !machine_runtime_file_copy_stack_window(
            &runtime_file,
            32u,
            SIZE_MAX,
            &window_bytes,
            &window_byte_count,
            &window_base_virtual_address,
            &runtime_error) ||
        window_byte_count != 4064u ||
        window_base_virtual_address != stack_base_virtual_address + 32u ||
        (window_byte_count > 0u && window_bytes[window_byte_count - 1u] != 0u) ||
        (free(window_bytes), window_bytes = NULL, 0) ||
        !machine_runtime_file_copy_gap_window(
            &runtime_file,
            32u,
            SIZE_MAX,
            &window_bytes,
            &window_byte_count,
            &window_base_virtual_address,
            &runtime_error) ||
        window_byte_count != 4064u ||
        window_base_virtual_address != gap_base_virtual_address + 32u ||
        (window_byte_count > 0u && window_bytes[window_byte_count - 1u] != 0u)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: overflow clamp validation mismatch: %s\n",
            runtime_error.message);
        ok = 0;
    }

cleanup:
    free(window_bytes);
    machine_runtime_file_free(&runtime_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_runtime_preserves_global_data_segment_permissions(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineRuntimeFile runtime_file;
    MachineRuntimeReport runtime_report;
    MachineIrError ir_error;
    MachineRuntimeError runtime_error;
    const MachineRuntimeSegment *text_segment = NULL;
    const MachineRuntimeSegment *sbss_segment = NULL;
    const MachineRuntimeSegment *sdata_segment = NULL;
    const MachineRuntimeSegment *stack_segment = NULL;
    const MachineRuntimeSegmentSummary *segment_summary = NULL;
    unsigned char *segment_bytes = NULL;
    size_t segment_count = 0u;
    size_t mapped_byte_count = 0u;
    size_t executable_segment_count = 0u;
    size_t non_executable_segment_count = 0u;
    size_t segment_byte_count = 0u;
    size_t segment_index = 0u;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    machine_runtime_file_init(&runtime_file);
    machine_runtime_report_init(&runtime_report);
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&runtime_error, 0, sizeof(runtime_error));

    if (!build_global_object_machine_ir_report(&ir_report, &ir_error) ||
        !machine_runtime_build_from_machine_ir_report(&ir_report, &runtime_file, &runtime_error) ||
        !machine_runtime_build_report_from_machine_ir_report(&ir_report, &runtime_report, &runtime_error) ||
        !machine_runtime_file_get_summary(
            &runtime_file, &segment_count, &mapped_byte_count, &executable_segment_count) ||
        segment_count != 4u || mapped_byte_count != 16384u || executable_segment_count != 1u ||
        !machine_runtime_file_find_segment_by_name(&runtime_file, ".text", &segment_index, &text_segment) ||
        !text_segment || segment_index != 0u ||
        !machine_runtime_file_find_segment_by_name(&runtime_file, ".sbss", &segment_index, &sbss_segment) ||
        !sbss_segment || segment_index != 1u ||
        !machine_runtime_file_find_segment_by_name(&runtime_file, ".sdata", &segment_index, &sdata_segment) ||
        !sdata_segment || segment_index != 2u ||
        !machine_runtime_file_find_segment_by_name(&runtime_file, ".stack", &segment_index, &stack_segment) ||
        !stack_segment || segment_index != runtime_file.stack_segment_index ||
        !text_segment->readable || text_segment->writable || !text_segment->executable ||
        !sbss_segment->readable || !sbss_segment->writable || sbss_segment->executable ||
        !sdata_segment->readable || !sdata_segment->writable || sdata_segment->executable ||
        !stack_segment->readable || !stack_segment->writable || stack_segment->executable ||
        !machine_runtime_file_copy_segment_bytes(
            &runtime_file, 1u, &segment_bytes, &segment_byte_count, &runtime_error) ||
        !segment_bytes || segment_byte_count != 4096u ||
        segment_bytes[0] != 0u || segment_bytes[1] != 0u ||
        segment_bytes[2] != 0u || segment_bytes[3] != 0u ||
        (free(segment_bytes), segment_bytes = NULL, segment_byte_count = 0u, 0) ||
        !machine_runtime_file_copy_segment_bytes(
            &runtime_file, 2u, &segment_bytes, &segment_byte_count, &runtime_error) ||
        !segment_bytes || segment_byte_count != 4096u ||
        segment_bytes[0] != 7u || segment_bytes[1] != 0u ||
        segment_bytes[2] != 0u || segment_bytes[3] != 0u ||
        !machine_runtime_report_get_segment_filter_count(
            &runtime_report, MACHINE_RUNTIME_SEGMENT_FILTER_NON_EXECUTABLE, &non_executable_segment_count) ||
        non_executable_segment_count != 3u ||
        !machine_runtime_report_get_segment_summary_by_filter_index(
            &runtime_report,
            MACHINE_RUNTIME_SEGMENT_FILTER_NON_EXECUTABLE,
            0u,
            &segment_index,
            &segment_summary) ||
        !segment_summary || segment_index != 1u || strcmp(segment_summary->name, ".sbss") != 0 ||
        !segment_summary->writable ||
        !machine_runtime_report_get_segment_summary_by_filter_index(
            &runtime_report,
            MACHINE_RUNTIME_SEGMENT_FILTER_NON_EXECUTABLE,
            1u,
            &segment_index,
            &segment_summary) ||
        !segment_summary || segment_index != 2u || strcmp(segment_summary->name, ".sdata") != 0 ||
        !segment_summary->writable) {
        fprintf(stderr,
            "[machine-runtime] FAIL: global-data runtime permission mismatch: ir=%s runtime=%s\n",
            ir_error.message,
            runtime_error.message);
        ok = 0;
    }

    free(segment_bytes);
    machine_runtime_report_free(&runtime_report);
    machine_runtime_file_free(&runtime_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_runtime_query_helpers_reject_malformed_segment_tables(void) {
    MachineIrAllocateRewriteReport ir_report;
    MachineIrError ir_error;
    MachineRuntimeFile runtime_file;
    MachineRuntimeReport runtime_report;
    MachineRuntimeError runtime_error;
    MachineRuntimeSegment *saved_file_segments = NULL;
    MachineRuntimeSegment *saved_report_file_segments = NULL;
    MachineRuntimeSegmentSummary *saved_report_segment_summaries = NULL;
    const MachineRuntimeSegment *segment = NULL;
    const MachineRuntimeSegmentSummary *segment_summary = NULL;
    MachineRuntimeReportSegmentArtifact segment_artifact;
    MachineRuntimeRegionSummary region_summary;
    size_t segment_index = 0u;
    size_t count = 0u;
    size_t base_virtual_address = 0u;
    int ok = 1;

    machine_ir_allocate_rewrite_report_init(&ir_report);
    machine_runtime_file_init(&runtime_file);
    machine_runtime_report_init(&runtime_report);
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&runtime_error, 0, sizeof(runtime_error));
    memset(&segment_artifact, 0, sizeof(segment_artifact));
    memset(&region_summary, 0, sizeof(region_summary));

    if (!build_resolved_machine_ir_report(&ir_report, &ir_error) ||
        !machine_runtime_build_from_machine_ir_report(&ir_report, &runtime_file, &runtime_error) ||
        !machine_runtime_build_report_from_machine_ir_report(&ir_report, &runtime_report, &runtime_error)) {
        fprintf(stderr,
            "[machine-runtime] FAIL: malformed-runtime setup failed: ir=%s runtime=%s\n",
            ir_error.message,
            runtime_error.message);
        ok = 0;
        goto cleanup;
    }

    base_virtual_address = runtime_file.segments[0].virtual_address;
    saved_file_segments = runtime_file.segments;
    runtime_file.segments = NULL;
    if (machine_runtime_file_get_summary(&runtime_file, &count, NULL, NULL) ||
        machine_runtime_file_get_gap_summary(&runtime_file, &runtime_report.gap_summary) ||
        machine_runtime_file_get_segment(&runtime_file, 0u, &segment) ||
        machine_runtime_file_find_segment_by_name(&runtime_file, ".text", &segment_index, &segment) ||
        machine_runtime_file_find_segment_covering_virtual_address(
            &runtime_file, base_virtual_address, &segment_index, &segment) ||
        machine_runtime_file_get_stack_segment(&runtime_file, &segment_index, &segment) ||
        machine_runtime_file_get_executable_segment_count(&runtime_file, &count) ||
        machine_runtime_file_get_executable_segment_by_index(&runtime_file, 0u, &segment_index, &segment) ||
        machine_runtime_file_get_region_summary(&runtime_file, 0u, &region_summary)) {
        fprintf(stderr, "[machine-runtime] FAIL: malformed runtime-file query unexpectedly succeeded\n");
        ok = 0;
    }
    runtime_file.segments = saved_file_segments;

    saved_report_file_segments = runtime_report.file.segments;
    saved_report_segment_summaries = runtime_report.segment_summaries;
    runtime_report.file.segments = NULL;
    runtime_report.segment_summaries = NULL;
    if (machine_runtime_report_get_entry_segment_summary_artifact(
            &runtime_report, &segment_index, &segment_summary) ||
        machine_runtime_report_get_stack_segment_summary_artifact(
            &runtime_report, &segment_index, &segment_summary) ||
        machine_runtime_report_get_segment_summary(&runtime_report, 0u, &segment_summary) ||
        machine_runtime_report_get_segment_artifact(&runtime_report, 0u, &segment_artifact) ||
        machine_runtime_report_find_segment_summary_by_name(
            &runtime_report, ".text", &segment_index, &segment_summary) ||
        machine_runtime_report_find_segment_summary_covering_virtual_address(
            &runtime_report, base_virtual_address, &segment_index, &segment_summary) ||
        machine_runtime_report_get_segment_filter_count(
            &runtime_report, MACHINE_RUNTIME_SEGMENT_FILTER_EXECUTABLE, &count) ||
        machine_runtime_report_get_segment_summary_by_filter_index(
            &runtime_report,
            MACHINE_RUNTIME_SEGMENT_FILTER_EXECUTABLE,
            0u,
            &segment_index,
            &segment_summary) ||
        machine_runtime_report_get_region_filter_count(
            &runtime_report, MACHINE_RUNTIME_REGION_FILTER_LOAD, &count) ||
        machine_runtime_report_get_region_summary_by_filter_index(
            &runtime_report,
            MACHINE_RUNTIME_REGION_FILTER_LOAD,
            0u,
            &segment_index,
            &region_summary)) {
        fprintf(stderr, "[machine-runtime] FAIL: malformed runtime-report query unexpectedly succeeded\n");
        ok = 0;
    }
    runtime_report.file.segments = saved_report_file_segments;
    runtime_report.segment_summaries = saved_report_segment_summaries;

cleanup:
    runtime_report.file.segments = saved_report_file_segments ? saved_report_file_segments : runtime_report.file.segments;
    runtime_report.segment_summaries =
        saved_report_segment_summaries ? saved_report_segment_summaries : runtime_report.segment_summaries;
    runtime_file.segments = saved_file_segments ? saved_file_segments : runtime_file.segments;
    machine_runtime_report_free(&runtime_report);
    machine_runtime_file_free(&runtime_file);
    machine_ir_allocate_rewrite_report_free(&ir_report);
    return ok;
}

static int test_machine_runtime_rejects_wrapped_address_ranges(void) {
    MachineLoadFile load_file;
    MachineRuntimeFile runtime_file;
    MachineRuntimeError runtime_error;
    int ok = 1;

    machine_load_file_init(&load_file);
    machine_runtime_file_init(&runtime_file);
    memset(&runtime_error, 0, sizeof(runtime_error));

    if (!build_manual_runtime_load_file_for_wrap_test(
            &load_file,
            ((size_t)-1) - 0x17FFu,
            0x800u)) {
        fprintf(stderr, "[machine-runtime] FAIL: wrapped-runtime setup failed\n");
        machine_runtime_file_free(&runtime_file);
        machine_load_file_free(&load_file);
        return 0;
    }

    if (machine_runtime_build_from_machine_load_file(&load_file, &runtime_file, &runtime_error) ||
        strstr(runtime_error.message, "failed deriving runtime policy") == NULL) {
        fprintf(stderr,
            "[machine-runtime] FAIL: wrapped-runtime build rejection mismatch: %s\n",
            runtime_error.message);
        ok = 0;
    }
    machine_runtime_file_free(&runtime_file);
    machine_load_file_free(&load_file);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_runtime_mainline();
    ok &= test_machine_runtime_bridge_matrix();
    ok &= test_machine_runtime_dump_wrappers();
    ok &= test_machine_runtime_profile_bridge();
    ok &= test_machine_runtime_profiled_wrappers();
    ok &= test_machine_runtime_zero_gap_region_and_report_surface();
    ok &= test_machine_runtime_region_load_filter_survives_segment_reorder();
    ok &= test_machine_runtime_window_copy_clamps_huge_lengths();
    ok &= test_machine_runtime_preserves_global_data_segment_permissions();
    ok &= test_machine_runtime_query_helpers_reject_malformed_segment_tables();
    ok &= test_machine_runtime_rejects_wrapped_address_ranges();
    return ok ? 0 : 1;
}
