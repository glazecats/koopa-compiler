#include "machine/elf.h"

#include "machine/bytes.h"
#include "machine/container.h"
#include "machine/emit.h"
#include "machine/encode.h"
#include "machine/ir.h"
#include "machine/object.h"
#include "machine/reloc.h"

#include <elf.h>
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
    copy = (char *)malloc(length + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static void write_u16_le(unsigned char *bytes, size_t offset, uint16_t value) {
    bytes[offset] = (unsigned char)(value & 0xFFu);
    bytes[offset + 1u] = (unsigned char)((value >> 8u) & 0xFFu);
}

static void write_u32_le(unsigned char *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (unsigned char)(value & 0xFFu);
    bytes[offset + 1u] = (unsigned char)((value >> 8u) & 0xFFu);
    bytes[offset + 2u] = (unsigned char)((value >> 16u) & 0xFFu);
    bytes[offset + 3u] = (unsigned char)((value >> 24u) & 0xFFu);
}

static int expect_dump(const MachineElfFile *elf_file, const char *expected_text) {
    char *actual_text = NULL;
    MachineElfError error;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    if (!machine_elf_dump_file(elf_file, &actual_text, &error)) {
        fprintf(stderr, "[machine-elf] FAIL: dump failed: %s\n", error.message);
        return 0;
    }
    if (!actual_text || strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr, "[machine-elf] FAIL: dump mismatch\nactual:\n%s\n", actual_text ? actual_text : "<null>");
        ok = 0;
    }
    free(actual_text);
    return ok;
}

static int expect_round_trip_dump(const MachineElfFile *elf_file) {
    unsigned char *bytes = NULL;
    unsigned char *normalized_bytes = NULL;
    size_t byte_count = 0u;
    size_t normalized_byte_count = 0u;
    MachineElfFile parsed_file;
    MachineElfError error;
    char *original_dump = NULL;
    char *parsed_dump = NULL;
    char *parsed_dump_via_helper = NULL;
    int ok = 1;

    memset(&error, 0, sizeof(error));
    machine_elf_file_init(&parsed_file);

    if (!machine_elf_file_copy_bytes(elf_file, &bytes, &byte_count, &error) ||
        !bytes ||
        !machine_elf_parse_file_from_bytes(bytes, byte_count, &parsed_file, &error) ||
        !machine_elf_normalize_bytes_from_bytes(
            bytes,
            byte_count,
            &normalized_bytes,
            &normalized_byte_count,
            &error) ||
        !machine_elf_dump_file(elf_file, &original_dump, &error) ||
        !machine_elf_dump_file(&parsed_file, &parsed_dump, &error) ||
        !machine_elf_build_dump_from_bytes(bytes, byte_count, &parsed_dump_via_helper, &error)) {
        fprintf(stderr, "[machine-elf] FAIL: round-trip failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    if (!original_dump || !parsed_dump || !parsed_dump_via_helper ||
        strcmp(original_dump, parsed_dump) != 0 ||
        strcmp(original_dump, parsed_dump_via_helper) != 0) {
        fprintf(stderr,
            "[machine-elf] FAIL: round-trip dump mismatch\noriginal:\n%s\nparsed:\n%s\nhelper:\n%s\n",
            original_dump ? original_dump : "<null>",
            parsed_dump ? parsed_dump : "<null>",
            parsed_dump_via_helper ? parsed_dump_via_helper : "<null>");
        ok = 0;
    }
    if (!normalized_bytes || normalized_byte_count != byte_count ||
        memcmp(normalized_bytes, bytes, byte_count) != 0) {
        fprintf(stderr, "[machine-elf] FAIL: normalized byte image mismatch\n");
        ok = 0;
    }

cleanup:
    free(bytes);
    free(normalized_bytes);
    free(original_dump);
    free(parsed_dump);
    free(parsed_dump_via_helper);
    machine_elf_file_free(&parsed_file);
    return ok;
}

static int test_machine_elf_builds_from_machine_ir_report(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfError elf_error;
    MachineElfHeaderSummary header_summary;
    const MachineElfSection *section = NULL;
    const MachineElfSection *symtab_section = NULL;
    const MachineElfSection *rel_text_section = NULL;
    const MachineElfSection *strtab_section = NULL;
    const MachineElfSection *shstrtab_section = NULL;
    const MachineElfSymbol *symbol = NULL;
    const MachineElfSymbol *target_symbol = NULL;
    const MachineElfRelocation *relocation = NULL;
    unsigned char *bytes = NULL;
    size_t section_count = 0;
    size_t symbol_count = 0;
    size_t relocation_count = 0;
    size_t byte_count = 0;
    size_t first_global_symbol_index = 0u;
    MachineElfTargetProfile target_profile;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_get_summary(&elf_file, &section_count, &symbol_count, &relocation_count, &byte_count) ||
        !machine_elf_file_get_target_profile(&elf_file, &target_profile) ||
        target_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        section_count != 6u || symbol_count != 5u || relocation_count != 2u || byte_count != 468u ||
        !machine_elf_file_get_header_summary(&elf_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        header_summary.elf_class != 1u || header_summary.data_encoding != 1u ||
        header_summary.object_type != 1u || header_summary.machine != 0u ||
        header_summary.elf_version != 1u || header_summary.flags != 0u ||
        header_summary.section_header_offset != 228u ||
        header_summary.section_header_entry_size != 40u ||
        header_summary.section_header_count != 6u ||
        header_summary.section_name_string_table_index != 5u ||
        !machine_elf_file_get_text_section(&elf_file, NULL, &section) || !section ||
        section->file_offset != 52u || section->byte_count != 9u ||
        !machine_elf_file_get_symtab_section(&elf_file, NULL, &symtab_section) || !symtab_section ||
        symtab_section->info != 4u || symtab_section->link != 2u ||
        !machine_elf_file_get_rel_text_section(&elf_file, NULL, &rel_text_section) || !rel_text_section ||
        rel_text_section->info != 1u || rel_text_section->link != 3u ||
        !machine_elf_file_get_strtab_section(&elf_file, NULL, &strtab_section) || !strtab_section ||
        strtab_section->file_offset != 61u || strtab_section->byte_count != 24u ||
        !machine_elf_file_get_shstrtab_section(&elf_file, NULL, &shstrtab_section) || !shstrtab_section ||
        shstrtab_section->file_offset != 184u || shstrtab_section->byte_count != 43u ||
        !machine_elf_file_get_first_global_symbol_index(&elf_file, &first_global_symbol_index) ||
        first_global_symbol_index != 4u ||
        !machine_elf_file_find_symbol_by_name(&elf_file, "main", NULL, &symbol) || !symbol ||
        symbol->binding != MACHINE_ELF_SYMBOL_GLOBAL ||
        !symbol->is_defined || symbol->section_index != 1u || symbol->value != 0u ||
        !machine_elf_file_find_symbol_by_name(&elf_file, "F0.L2", NULL, &target_symbol) || !target_symbol ||
        target_symbol->binding != MACHINE_ELF_SYMBOL_LOCAL ||
        !machine_elf_file_get_relocation(&elf_file, 0u, &relocation) || !relocation ||
        relocation->source_kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        relocation->section_index != 4u || relocation->offset != 4u || relocation->symbol_index != 3u ||
        relocation->type != 2u ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error) ||
        !bytes || byte_count != 468u ||
        bytes[0] != 0x7Fu || bytes[1] != 'E' || bytes[2] != 'L' || bytes[3] != 'F') {
        fprintf(stderr, "[machine-elf] FAIL: elf query mismatch: %s\n", elf_error.message);
        ok = 0;
    }

    ok &= expect_dump(
        &elf_file,
        "machine_elf profile=generic-elf32 bytes=468 sections=6 symbols=5 relocations=2 shoff=228\n"
        "sections:\n"
        "  esec.0  type=0 file@0 bytes=0 link=0 info=0 align=0 entsize=0\n"
        "  esec.1 .text type=1 file@52 bytes=9 link=0 info=0 align=1 entsize=0\n"
        "  esec.2 .strtab type=3 file@61 bytes=24 link=0 info=0 align=1 entsize=0\n"
        "  esec.3 .symtab type=2 file@88 bytes=80 link=2 info=4 align=4 entsize=16\n"
        "  esec.4 .rel.text type=9 file@168 bytes=16 link=3 info=1 align=4 entsize=8\n"
        "  esec.5 .shstrtab type=3 file@184 bytes=43 link=0 info=0 align=1 entsize=0\n"
        "symbols:\n"
        "  esym.0  bind=local type=0 defined=0 sec=0 value=0 size=0\n"
        "  esym.1 F0.L0 bind=local type=0 defined=1 sec=1 value=0 size=5\n"
        "  esym.2 F0.L1 bind=local type=0 defined=1 sec=1 value=5 size=2\n"
        "  esym.3 F0.L2 bind=local type=0 defined=1 sec=1 value=7 size=2\n"
        "  esym.4 main bind=global type=2 defined=1 sec=1 value=0 size=9\n"
        "relocations:\n"
        "  erel.0 kind=ctrl-primary sec=4 off=4 sym=3 type=2 name=F0.L2\n"
        "  erel.1 kind=ctrl-secondary sec=4 off=4 sym=2 type=3 name=F0.L1\n");
    ok &= expect_round_trip_dump(&elf_file);
    if (ok) {
        unsigned char *invalid_bytes = (unsigned char *)malloc(byte_count);
        MachineElfFile invalid_file;

        machine_elf_file_init(&invalid_file);
        if (!invalid_bytes) {
            ok = 0;
        } else {
            memcpy(invalid_bytes, bytes, byte_count);
            invalid_bytes[18] = 1u;
            invalid_bytes[19] = 0u;
            if (machine_elf_parse_file_from_bytes(invalid_bytes, byte_count, &invalid_file, &elf_error)) {
                fprintf(stderr, "[machine-elf] FAIL: parse should reject unknown target profile header\n");
                ok = 0;
            }
        }
        free(invalid_bytes);
        machine_elf_file_free(&invalid_file);
    }
    if (ok) {
        unsigned char *invalid_bytes = (unsigned char *)malloc(byte_count);
        MachineElfFile invalid_file;

        machine_elf_file_init(&invalid_file);
        if (!invalid_bytes) {
            ok = 0;
        } else {
            memcpy(invalid_bytes, bytes, byte_count);
            invalid_bytes[228u + 3u * 40u + 24u] = 5u;
            invalid_bytes[228u + 3u * 40u + 25u] = 0u;
            invalid_bytes[228u + 3u * 40u + 26u] = 0u;
            invalid_bytes[228u + 3u * 40u + 27u] = 0u;
            if (machine_elf_parse_file_from_bytes(invalid_bytes, byte_count, &invalid_file, &elf_error)) {
                fprintf(stderr, "[machine-elf] FAIL: parse should reject invalid symtab link semantics\n");
                ok = 0;
            }
        }
        free(invalid_bytes);
        machine_elf_file_free(&invalid_file);
    }
    if (ok && rel_text_section) {
        unsigned char *invalid_bytes = (unsigned char *)malloc(byte_count);
        MachineElfFile invalid_file;

        machine_elf_file_init(&invalid_file);
        if (!invalid_bytes) {
            ok = 0;
        } else {
            memcpy(invalid_bytes, bytes, byte_count);
            invalid_bytes[rel_text_section->file_offset + 4u] = 0u;
            if (machine_elf_parse_file_from_bytes(invalid_bytes, byte_count, &invalid_file, &elf_error)) {
                fprintf(stderr, "[machine-elf] FAIL: parse should reject invalid relocation opcode mapping\n");
                ok = 0;
            }
        }
        free(invalid_bytes);
        machine_elf_file_free(&invalid_file);
    }
    if (ok) {
        unsigned char *reordered_bytes = (unsigned char *)malloc(byte_count);
        unsigned char *normalized_bytes = NULL;
        size_t normalized_byte_count = 0u;
        MachineElfFile reordered_file;
        char *reordered_dump = NULL;
        size_t section_order[6] = {0u, 5u, 4u, 3u, 2u, 1u};
        size_t entry_index;

        machine_elf_file_init(&reordered_file);
        if (!reordered_bytes) {
            ok = 0;
        } else {
            memcpy(reordered_bytes, bytes, byte_count);
            for (entry_index = 0u; entry_index < 6u; ++entry_index) {
                memcpy(
                    reordered_bytes + 228u + entry_index * 40u,
                    bytes + 228u + section_order[entry_index] * 40u,
                    40u);
            }
            write_u16_le(reordered_bytes, 50u, 1u);
            write_u32_le(reordered_bytes, 228u + 2u * 40u + 24u, 3u);
            write_u32_le(reordered_bytes, 228u + 2u * 40u + 28u, 5u);
            write_u32_le(reordered_bytes, 228u + 3u * 40u + 24u, 4u);
            for (entry_index = 1u; entry_index < 5u; ++entry_index) {
                write_u16_le(reordered_bytes, 88u + entry_index * 16u + 14u, 5u);
            }

            if (!machine_elf_parse_file_from_bytes(reordered_bytes, byte_count, &reordered_file, &elf_error) ||
                !machine_elf_normalize_bytes_from_bytes(
                    reordered_bytes,
                    byte_count,
                    &normalized_bytes,
                    &normalized_byte_count,
                    &elf_error) ||
                !machine_elf_dump_file(&reordered_file, &reordered_dump, &elf_error) ||
                !reordered_dump ||
                !normalized_bytes ||
                normalized_byte_count != byte_count ||
                memcmp(normalized_bytes, bytes, byte_count) != 0 ||
                strcmp(
                    reordered_dump,
                    "machine_elf profile=generic-elf32 bytes=468 sections=6 symbols=5 relocations=2 shoff=228\n"
                    "sections:\n"
                    "  esec.0  type=0 file@0 bytes=0 link=0 info=0 align=0 entsize=0\n"
                    "  esec.1 .text type=1 file@52 bytes=9 link=0 info=0 align=1 entsize=0\n"
                    "  esec.2 .strtab type=3 file@61 bytes=24 link=0 info=0 align=1 entsize=0\n"
                    "  esec.3 .symtab type=2 file@88 bytes=80 link=2 info=4 align=4 entsize=16\n"
                    "  esec.4 .rel.text type=9 file@168 bytes=16 link=3 info=1 align=4 entsize=8\n"
                    "  esec.5 .shstrtab type=3 file@184 bytes=43 link=0 info=0 align=1 entsize=0\n"
                    "symbols:\n"
                    "  esym.0  bind=local type=0 defined=0 sec=0 value=0 size=0\n"
                    "  esym.1 F0.L0 bind=local type=0 defined=1 sec=1 value=0 size=5\n"
                    "  esym.2 F0.L1 bind=local type=0 defined=1 sec=1 value=5 size=2\n"
                    "  esym.3 F0.L2 bind=local type=0 defined=1 sec=1 value=7 size=2\n"
                    "  esym.4 main bind=global type=2 defined=1 sec=1 value=0 size=9\n"
                    "relocations:\n"
                    "  erel.0 kind=ctrl-primary sec=4 off=4 sym=3 type=2 name=F0.L2\n"
                    "  erel.1 kind=ctrl-secondary sec=4 off=4 sym=2 type=3 name=F0.L1\n") != 0 ||
                !expect_round_trip_dump(&reordered_file)) {
                fprintf(stderr, "[machine-elf] FAIL: parse should normalize reordered section headers: %s\n", elf_error.message);
                ok = 0;
            }
        }
        free(reordered_bytes);
        free(normalized_bytes);
        free(reordered_dump);
        machine_elf_file_free(&reordered_file);
    }
    if (ok) {
        unsigned char *unsorted_symbol_bytes = (unsigned char *)malloc(byte_count);
        MachineElfFile unsorted_symbol_file;
        char *unsorted_symbol_dump = NULL;

        machine_elf_file_init(&unsorted_symbol_file);
        if (!unsorted_symbol_bytes) {
            ok = 0;
        } else {
            memcpy(unsorted_symbol_bytes, bytes, byte_count);
            memcpy(unsorted_symbol_bytes + 120u, bytes + 152u, 16u);
            memcpy(unsorted_symbol_bytes + 136u, bytes + 120u, 16u);
            memcpy(unsorted_symbol_bytes + 152u, bytes + 136u, 16u);
            write_u32_le(unsorted_symbol_bytes, 168u + 4u, (4u << 8u) | 2u);
            write_u32_le(unsorted_symbol_bytes, 176u + 4u, (3u << 8u) | 3u);
            write_u32_le(unsorted_symbol_bytes, 228u + 3u * 40u + 28u, 3u);

            if (!machine_elf_parse_file_from_bytes(unsorted_symbol_bytes, byte_count, &unsorted_symbol_file, &elf_error) ||
                !machine_elf_dump_file(&unsorted_symbol_file, &unsorted_symbol_dump, &elf_error) ||
                !unsorted_symbol_dump ||
                strcmp(
                    unsorted_symbol_dump,
                    "machine_elf profile=generic-elf32 bytes=468 sections=6 symbols=5 relocations=2 shoff=228\n"
                    "sections:\n"
                    "  esec.0  type=0 file@0 bytes=0 link=0 info=0 align=0 entsize=0\n"
                    "  esec.1 .text type=1 file@52 bytes=9 link=0 info=0 align=1 entsize=0\n"
                    "  esec.2 .strtab type=3 file@61 bytes=24 link=0 info=0 align=1 entsize=0\n"
                    "  esec.3 .symtab type=2 file@88 bytes=80 link=2 info=4 align=4 entsize=16\n"
                    "  esec.4 .rel.text type=9 file@168 bytes=16 link=3 info=1 align=4 entsize=8\n"
                    "  esec.5 .shstrtab type=3 file@184 bytes=43 link=0 info=0 align=1 entsize=0\n"
                    "symbols:\n"
                    "  esym.0  bind=local type=0 defined=0 sec=0 value=0 size=0\n"
                    "  esym.1 F0.L0 bind=local type=0 defined=1 sec=1 value=0 size=5\n"
                    "  esym.2 F0.L1 bind=local type=0 defined=1 sec=1 value=5 size=2\n"
                    "  esym.3 F0.L2 bind=local type=0 defined=1 sec=1 value=7 size=2\n"
                    "  esym.4 main bind=global type=2 defined=1 sec=1 value=0 size=9\n"
                    "relocations:\n"
                    "  erel.0 kind=ctrl-primary sec=4 off=4 sym=3 type=2 name=F0.L2\n"
                    "  erel.1 kind=ctrl-secondary sec=4 off=4 sym=2 type=3 name=F0.L1\n") != 0 ||
                !expect_round_trip_dump(&unsorted_symbol_file)) {
                fprintf(stderr, "[machine-elf] FAIL: parse should normalize unsorted symbol table: %s\n", elf_error.message);
                ok = 0;
            }
        }
        free(unsorted_symbol_bytes);
        free(unsorted_symbol_dump);
        machine_elf_file_free(&unsorted_symbol_file);
    }
    if (ok) {
        unsigned char *extended_bytes = (unsigned char *)calloc(516u, 1u);
        unsigned char *normalized_bytes = NULL;
        size_t normalized_byte_count = 0u;
        MachineElfFile extended_file;
        char *extended_dump = NULL;

        machine_elf_file_init(&extended_file);
        if (!extended_bytes) {
            ok = 0;
        } else {
            memcpy(extended_bytes, bytes, 227u);
            memcpy(extended_bytes + 236u, bytes + 228u, 240u);
            memcpy(extended_bytes + 227u, ".comment", 9u);
            write_u32_le(extended_bytes, 32u, 236u);
            write_u16_le(extended_bytes, 48u, 7u);
            write_u32_le(extended_bytes, 236u + 5u * 40u + 20u, 52u);
            write_u32_le(extended_bytes, 236u + 6u * 40u + 0u, 43u);
            write_u32_le(extended_bytes, 236u + 6u * 40u + 4u, 1u);
            write_u32_le(extended_bytes, 236u + 6u * 40u + 32u, 1u);

            if (!machine_elf_parse_file_from_bytes(extended_bytes, 516u, &extended_file, &elf_error) ||
                !machine_elf_normalize_bytes_from_bytes(
                    extended_bytes,
                    516u,
                    &normalized_bytes,
                    &normalized_byte_count,
                    &elf_error) ||
                !machine_elf_dump_file(&extended_file, &extended_dump, &elf_error) ||
                !extended_dump ||
                !normalized_bytes ||
                normalized_byte_count != byte_count ||
                memcmp(normalized_bytes, bytes, byte_count) != 0 ||
                strcmp(
                    extended_dump,
                    "machine_elf profile=generic-elf32 bytes=468 sections=6 symbols=5 relocations=2 shoff=228\n"
                    "sections:\n"
                    "  esec.0  type=0 file@0 bytes=0 link=0 info=0 align=0 entsize=0\n"
                    "  esec.1 .text type=1 file@52 bytes=9 link=0 info=0 align=1 entsize=0\n"
                    "  esec.2 .strtab type=3 file@61 bytes=24 link=0 info=0 align=1 entsize=0\n"
                    "  esec.3 .symtab type=2 file@88 bytes=80 link=2 info=4 align=4 entsize=16\n"
                    "  esec.4 .rel.text type=9 file@168 bytes=16 link=3 info=1 align=4 entsize=8\n"
                    "  esec.5 .shstrtab type=3 file@184 bytes=43 link=0 info=0 align=1 entsize=0\n"
                    "symbols:\n"
                    "  esym.0  bind=local type=0 defined=0 sec=0 value=0 size=0\n"
                    "  esym.1 F0.L0 bind=local type=0 defined=1 sec=1 value=0 size=5\n"
                    "  esym.2 F0.L1 bind=local type=0 defined=1 sec=1 value=5 size=2\n"
                    "  esym.3 F0.L2 bind=local type=0 defined=1 sec=1 value=7 size=2\n"
                    "  esym.4 main bind=global type=2 defined=1 sec=1 value=0 size=9\n"
                    "relocations:\n"
                    "  erel.0 kind=ctrl-primary sec=4 off=4 sym=3 type=2 name=F0.L2\n"
                    "  erel.1 kind=ctrl-secondary sec=4 off=4 sym=2 type=3 name=F0.L1\n") != 0 ||
                !expect_round_trip_dump(&extended_file)) {
                fprintf(stderr, "[machine-elf] FAIL: parse should ignore extra non-core section: %s\n", elf_error.message);
                ok = 0;
            }
        }
        free(extended_bytes);
        free(normalized_bytes);
        free(extended_dump);
        machine_elf_file_free(&extended_file);
    }

    free(bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    return ok;
}

static int test_machine_elf_parse_accepts_droppable_local_section_symbols(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfFile invalid_file;
    MachineElfError elf_error;
    unsigned char *bytes = NULL;
    unsigned char *augmented_bytes = NULL;
    unsigned char *normalized_bytes = NULL;
    size_t byte_count = 0u;
    size_t normalized_byte_count = 0u;
    size_t section_count = 0u;
    size_t symbol_count = 0u;
    size_t relocation_count = 0u;
    size_t parsed_byte_count = 0u;
    size_t first_global_symbol_index = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);
    machine_elf_file_init(&invalid_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: section-symbol setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        machine_elf_file_free(&invalid_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: section-symbol setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        machine_elf_file_free(&invalid_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: section-symbol setup failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    augmented_bytes = (unsigned char *)calloc(500u, 1u);
    if (!augmented_bytes) {
        ok = 0;
        goto cleanup;
    }

    memcpy(augmented_bytes, bytes, 168u);
    memcpy(augmented_bytes + 200u, bytes + 168u, 16u);
    memcpy(augmented_bytes + 216u, bytes + 184u, 43u);
    memcpy(augmented_bytes + 260u, bytes + 228u, 240u);
    write_u32_le(augmented_bytes, 32u, 260u);
    write_u32_le(augmented_bytes, 260u + 3u * 40u + 20u, 112u);
    write_u32_le(augmented_bytes, 260u + 4u * 40u + 16u, 200u);
    write_u32_le(augmented_bytes, 260u + 5u * 40u + 16u, 216u);

    augmented_bytes[180u] = (unsigned char)(((uint32_t)MACHINE_ELF_SYMBOL_LOCAL << 4u) |
        (uint32_t)MACHINE_ELF_SYMBOL_SECTION);
    write_u16_le(augmented_bytes, 182u, 1u);
    write_u16_le(augmented_bytes, 198u, 1u);

    if (!machine_elf_parse_file_from_bytes(augmented_bytes, 500u, &parsed_file, &elf_error) ||
        !machine_elf_file_get_summary(
            &parsed_file,
            &section_count,
            &symbol_count,
            &relocation_count,
            &parsed_byte_count) ||
        !machine_elf_file_get_first_global_symbol_index(&parsed_file, &first_global_symbol_index) ||
        section_count != 6u || symbol_count != 5u || relocation_count != 2u ||
        parsed_byte_count != byte_count || first_global_symbol_index != 4u ||
        !machine_elf_normalize_bytes_from_bytes(
            augmented_bytes,
            500u,
            &normalized_bytes,
            &normalized_byte_count,
            &elf_error) ||
        !normalized_bytes ||
        normalized_byte_count != byte_count ||
        memcmp(normalized_bytes, bytes, byte_count) != 0 ||
        !expect_round_trip_dump(&parsed_file)) {
        fprintf(stderr, "[machine-elf] FAIL: parse should drop local section-like symbols: %s\n", elf_error.message);
        ok = 0;
    }

    if (ok) {
        unsigned char *invalid_bytes = (unsigned char *)malloc(500u);

        if (!invalid_bytes) {
            ok = 0;
        } else {
            memcpy(invalid_bytes, augmented_bytes, 500u);
            write_u32_le(invalid_bytes, 200u + 4u, (5u << 8u) | 2u);
            if (machine_elf_parse_file_from_bytes(invalid_bytes, 500u, &invalid_file, &elf_error)) {
                fprintf(stderr, "[machine-elf] FAIL: parse should reject relocations targeting dropped local symbols\n");
                ok = 0;
            }
        }
        free(invalid_bytes);
    }

cleanup:
    free(bytes);
    free(augmented_bytes);
    free(normalized_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    machine_elf_file_free(&invalid_file);
    return ok;
}

static int test_machine_elf_parse_accepts_droppable_local_file_symbol(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfError elf_error;
    unsigned char *bytes = NULL;
    unsigned char *augmented_bytes = NULL;
    unsigned char *normalized_bytes = NULL;
    size_t byte_count = 0u;
    size_t normalized_byte_count = 0u;
    size_t symbol_count = 0u;
    size_t first_global_symbol_index = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: file-symbol setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: file-symbol setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: file-symbol setup failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    augmented_bytes = (unsigned char *)calloc(500u, 1u);
    if (!augmented_bytes) {
        ok = 0;
        goto cleanup;
    }

    memcpy(augmented_bytes, bytes, 85u);
    memcpy(augmented_bytes + 96u, bytes + 88u, 64u);
    memcpy(augmented_bytes + 176u, bytes + 152u, 16u);
    memcpy(augmented_bytes + 192u, bytes + 168u, 16u);
    memcpy(augmented_bytes + 208u, bytes + 184u, 43u);
    memcpy(augmented_bytes + 252u, bytes + 228u, 240u);
    write_u32_le(augmented_bytes, 32u, 252u);
    write_u32_le(augmented_bytes, 252u + 2u * 40u + 20u, 32u);
    write_u32_le(augmented_bytes, 252u + 3u * 40u + 16u, 96u);
    write_u32_le(augmented_bytes, 252u + 3u * 40u + 20u, 96u);
    write_u32_le(augmented_bytes, 252u + 3u * 40u + 28u, 5u);
    write_u32_le(augmented_bytes, 252u + 4u * 40u + 16u, 192u);
    write_u32_le(augmented_bytes, 252u + 5u * 40u + 16u, 208u);

    memcpy(augmented_bytes + 61u + 24u, "input.c", 8u);
    write_u32_le(augmented_bytes, 160u + 0u, 24u);
    augmented_bytes[172u] = (unsigned char)(((uint32_t)MACHINE_ELF_SYMBOL_LOCAL << 4u) |
        (uint32_t)MACHINE_ELF_SYMBOL_FILE);
    write_u16_le(augmented_bytes, 174u, 0xFFF1u);

    if (!machine_elf_parse_file_from_bytes(augmented_bytes, 500u, &parsed_file, &elf_error) ||
        !machine_elf_file_get_summary(&parsed_file, NULL, &symbol_count, NULL, NULL) ||
        !machine_elf_file_get_first_global_symbol_index(&parsed_file, &first_global_symbol_index) ||
        symbol_count != 5u || first_global_symbol_index != 4u ||
        !machine_elf_normalize_bytes_from_bytes(
            augmented_bytes,
            500u,
            &normalized_bytes,
            &normalized_byte_count,
            &elf_error) ||
        !normalized_bytes ||
        normalized_byte_count != byte_count ||
        memcmp(normalized_bytes, bytes, byte_count) != 0 ||
        !expect_round_trip_dump(&parsed_file)) {
        fprintf(stderr, "[machine-elf] FAIL: parse should drop local file symbols safely: %s\n", elf_error.message);
        ok = 0;
    }

cleanup:
    free(bytes);
    free(augmented_bytes);
    free(normalized_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    return ok;
}

static int test_machine_elf_parse_accepts_droppable_local_symbol_on_extra_section(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfFile invalid_file;
    MachineElfError elf_error;
    unsigned char *bytes = NULL;
    unsigned char *extended_bytes = NULL;
    unsigned char *symbolized_bytes = NULL;
    unsigned char *normalized_bytes = NULL;
    size_t byte_count = 0u;
    size_t normalized_byte_count = 0u;
    size_t symbol_count = 0u;
    size_t first_global_symbol_index = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);
    machine_elf_file_init(&invalid_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: extra-section-symbol setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        machine_elf_file_free(&invalid_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: extra-section-symbol setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        machine_elf_file_free(&invalid_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: extra-section-symbol setup failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    symbolized_bytes = (unsigned char *)calloc(532u, 1u);
    extended_bytes = (unsigned char *)calloc(516u, 1u);
    if (!extended_bytes || !symbolized_bytes) {
        ok = 0;
        goto cleanup;
    }

    memcpy(extended_bytes, bytes, 227u);
    memcpy(extended_bytes + 236u, bytes + 228u, 240u);
    memcpy(extended_bytes + 227u, ".comment", 9u);
    write_u32_le(extended_bytes, 32u, 236u);
    write_u16_le(extended_bytes, 48u, 7u);
    write_u32_le(extended_bytes, 236u + 5u * 40u + 20u, 52u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 0u, 43u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 4u, 1u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 32u, 1u);

    memcpy(symbolized_bytes, extended_bytes, 152u);
    write_u32_le(symbolized_bytes, 152u + 0u, 1u);
    write_u32_le(symbolized_bytes, 152u + 4u, 0u);
    write_u32_le(symbolized_bytes, 152u + 8u, 0u);
    symbolized_bytes[152u + 12u] = (unsigned char)(((uint32_t)MACHINE_ELF_SYMBOL_LOCAL << 4u) |
        (uint32_t)MACHINE_ELF_SYMBOL_NOTYPE);
    write_u16_le(symbolized_bytes, 152u + 14u, 6u);
    memcpy(symbolized_bytes + 168u, extended_bytes + 152u, 16u);
    memcpy(symbolized_bytes + 184u, extended_bytes + 168u, 16u);
    memcpy(symbolized_bytes + 200u, extended_bytes + 184u, 52u);
    memcpy(symbolized_bytes + 252u, extended_bytes + 236u, 280u);

    write_u32_le(symbolized_bytes, 32u, 252u);
    write_u32_le(symbolized_bytes, 252u + 3u * 40u + 20u, 96u);
    write_u32_le(symbolized_bytes, 252u + 3u * 40u + 28u, 5u);
    write_u32_le(symbolized_bytes, 252u + 4u * 40u + 16u, 184u);
    write_u32_le(symbolized_bytes, 252u + 5u * 40u + 16u, 200u);

    if (!machine_elf_parse_file_from_bytes(symbolized_bytes, 532u, &parsed_file, &elf_error) ||
        !machine_elf_file_get_summary(&parsed_file, NULL, &symbol_count, NULL, NULL) ||
        !machine_elf_file_get_first_global_symbol_index(&parsed_file, &first_global_symbol_index) ||
        symbol_count != 5u || first_global_symbol_index != 4u ||
        !machine_elf_normalize_bytes_from_bytes(
            symbolized_bytes,
            532u,
            &normalized_bytes,
            &normalized_byte_count,
            &elf_error) ||
        !normalized_bytes ||
        normalized_byte_count != byte_count ||
        memcmp(normalized_bytes, bytes, byte_count) != 0 ||
        !expect_round_trip_dump(&parsed_file)) {
        fprintf(stderr, "[machine-elf] FAIL: parse should drop local symbols on extra sections: %s\n", elf_error.message);
        ok = 0;
    }

    if (ok) {
        unsigned char *invalid_bytes = (unsigned char *)malloc(532u);

        if (!invalid_bytes) {
            ok = 0;
        } else {
            memcpy(invalid_bytes, symbolized_bytes, 532u);
            write_u32_le(invalid_bytes, 184u + 4u, (4u << 8u) | 2u);
            if (machine_elf_parse_file_from_bytes(invalid_bytes, 532u, &invalid_file, &elf_error)) {
                fprintf(stderr, "[machine-elf] FAIL: parse should reject relocations targeting extra-section locals\n");
                ok = 0;
            }
        }
        free(invalid_bytes);
    }

cleanup:
    free(bytes);
    free(extended_bytes);
    free(symbolized_bytes);
    free(normalized_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    machine_elf_file_free(&invalid_file);
    return ok;
}

static int test_machine_elf_normalize_drops_unused_undefined_global_symbols(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfFile preserved_file;
    MachineElfError elf_error;
    unsigned char *bytes = NULL;
    unsigned char *augmented_bytes = NULL;
    unsigned char *normalized_bytes = NULL;
    size_t byte_count = 0u;
    size_t normalized_byte_count = 0u;
    size_t symbol_count = 0u;
    size_t first_global_symbol_index = 0u;
    size_t preserved_byte_count = 0u;
    const MachineElfSymbol *symbol = NULL;
    unsigned char *preserved_normalized_bytes = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);
    machine_elf_file_init(&preserved_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: unused-undef-global setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        machine_elf_file_free(&preserved_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: unused-undef-global setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        machine_elf_file_free(&preserved_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: unused-undef-global setup failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    augmented_bytes = (unsigned char *)calloc(516u, 1u);
    if (!augmented_bytes) {
        ok = 0;
        goto cleanup;
    }

    memcpy(augmented_bytes, bytes, 85u);
    memcpy(augmented_bytes + 96u, bytes + 88u, 64u);
    memcpy(augmented_bytes + 176u, bytes + 152u, 16u);
    memcpy(augmented_bytes + 192u, bytes + 168u, 16u);
    memcpy(augmented_bytes + 208u, bytes + 184u, 43u);
    memcpy(augmented_bytes + 252u, bytes + 228u, 240u);

    write_u32_le(augmented_bytes, 32u, 252u);
    write_u32_le(augmented_bytes, 252u + 2u * 40u + 20u, 35u);
    write_u32_le(augmented_bytes, 252u + 3u * 40u + 16u, 96u);
    write_u32_le(augmented_bytes, 252u + 3u * 40u + 20u, 96u);
    write_u32_le(augmented_bytes, 252u + 3u * 40u + 28u, 4u);
    write_u32_le(augmented_bytes, 252u + 4u * 40u + 16u, 192u);
    write_u32_le(augmented_bytes, 252u + 5u * 40u + 16u, 208u);

    memcpy(augmented_bytes + 61u + 24u, "unused_ext", 11u);
    write_u32_le(augmented_bytes, 160u + 0u, 24u);
    augmented_bytes[172u] = (unsigned char)(((uint32_t)MACHINE_ELF_SYMBOL_GLOBAL << 4u) |
        (uint32_t)MACHINE_ELF_SYMBOL_NOTYPE);
    write_u16_le(augmented_bytes, 174u, 0u);

    if (!machine_elf_parse_file_from_bytes(augmented_bytes, 516u, &parsed_file, &elf_error) ||
        !machine_elf_file_get_summary(&parsed_file, NULL, &symbol_count, NULL, NULL) ||
        symbol_count != 5u ||
        machine_elf_file_find_symbol_by_name(&parsed_file, "unused_ext", NULL, &symbol) ||
        !machine_elf_normalize_bytes_from_bytes(
            augmented_bytes,
            516u,
            &normalized_bytes,
            &normalized_byte_count,
            &elf_error) ||
        !normalized_bytes ||
        normalized_byte_count != byte_count ||
        memcmp(normalized_bytes, bytes, byte_count) != 0) {
        fprintf(stderr, "[machine-elf] FAIL: normalize should drop unused undefined globals: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    if (ok) {
        unsigned char *preserved_bytes = (unsigned char *)malloc(516u);

        if (!preserved_bytes) {
            ok = 0;
        } else {
            memcpy(preserved_bytes, augmented_bytes, 516u);
            write_u32_le(preserved_bytes, 192u + 4u, (4u << 8u) | 2u);
            if (!machine_elf_parse_file_from_bytes(preserved_bytes, 516u, &preserved_file, &elf_error) ||
                !machine_elf_file_get_summary(&preserved_file, NULL, &symbol_count, NULL, NULL) ||
                symbol_count != 6u ||
                !machine_elf_file_find_symbol_by_name(&preserved_file, "unused_ext", NULL, &symbol) || !symbol ||
                !machine_elf_file_get_first_global_symbol_index(&preserved_file, &first_global_symbol_index) ||
                first_global_symbol_index != 4u ||
                !machine_elf_file_copy_bytes(
                    &preserved_file,
                    &preserved_normalized_bytes,
                    &preserved_byte_count,
                    &elf_error) ||
                !preserved_normalized_bytes ||
                preserved_byte_count < byte_count ||
                (preserved_byte_count == byte_count &&
                    memcmp(preserved_normalized_bytes, bytes, byte_count) == 0)) {
                fprintf(stderr, "[machine-elf] FAIL: normalize should preserve referenced undefined globals: %s\n", elf_error.message);
                ok = 0;
            }
        }
        free(preserved_bytes);
    }

cleanup:
    free(bytes);
    free(augmented_bytes);
    free(normalized_bytes);
    free(preserved_normalized_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    machine_elf_file_free(&preserved_file);
    return ok;
}

static int test_machine_elf_normalize_drops_unused_defined_global_symbols_on_unsupported_sections(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfFile invalid_file;
    MachineElfError elf_error;
    unsigned char *bytes = NULL;
    unsigned char *extended_bytes = NULL;
    unsigned char *symbolized_bytes = NULL;
    unsigned char *normalized_bytes = NULL;
    size_t byte_count = 0u;
    size_t normalized_byte_count = 0u;
    size_t symbol_count = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);
    machine_elf_file_init(&invalid_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: unsupported-defined-global setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        machine_elf_file_free(&invalid_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: unsupported-defined-global setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        machine_elf_file_free(&invalid_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: unsupported-defined-global setup failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    extended_bytes = (unsigned char *)calloc(516u, 1u);
    symbolized_bytes = (unsigned char *)calloc(548u, 1u);
    if (!extended_bytes || !symbolized_bytes) {
        ok = 0;
        goto cleanup;
    }

    memcpy(extended_bytes, bytes, 227u);
    memcpy(extended_bytes + 236u, bytes + 228u, 240u);
    memcpy(extended_bytes + 227u, ".comment", 9u);
    write_u32_le(extended_bytes, 32u, 236u);
    write_u16_le(extended_bytes, 48u, 7u);
    write_u32_le(extended_bytes, 236u + 5u * 40u + 20u, 52u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 0u, 43u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 4u, 1u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 32u, 1u);

    memcpy(symbolized_bytes, extended_bytes, 152u);
    write_u32_le(symbolized_bytes, 152u + 0u, 0u);
    write_u32_le(symbolized_bytes, 152u + 4u, 0u);
    write_u32_le(symbolized_bytes, 152u + 8u, 0u);
    symbolized_bytes[152u + 12u] = (unsigned char)(((uint32_t)MACHINE_ELF_SYMBOL_GLOBAL << 4u) |
        (uint32_t)MACHINE_ELF_SYMBOL_NOTYPE);
    write_u16_le(symbolized_bytes, 152u + 14u, 6u);

    write_u32_le(symbolized_bytes, 168u + 0u, 0u);
    write_u32_le(symbolized_bytes, 168u + 4u, 123u);
    write_u32_le(symbolized_bytes, 168u + 8u, 0u);
    symbolized_bytes[168u + 12u] = (unsigned char)(((uint32_t)MACHINE_ELF_SYMBOL_GLOBAL << 4u) |
        (uint32_t)MACHINE_ELF_SYMBOL_NOTYPE);
    write_u16_le(symbolized_bytes, 168u + 14u, 0xFFF1u);

    memcpy(symbolized_bytes + 184u, extended_bytes + 152u, 16u);
    memcpy(symbolized_bytes + 200u, extended_bytes + 168u, 16u);
    memcpy(symbolized_bytes + 216u, extended_bytes + 184u, 52u);
    memcpy(symbolized_bytes + 268u, extended_bytes + 236u, 280u);

    write_u32_le(symbolized_bytes, 32u, 268u);
    write_u32_le(symbolized_bytes, 268u + 2u * 40u + 20u, 24u);
    write_u32_le(symbolized_bytes, 268u + 3u * 40u + 16u, 88u);
    write_u32_le(symbolized_bytes, 268u + 3u * 40u + 20u, 112u);
    write_u32_le(symbolized_bytes, 268u + 3u * 40u + 28u, 4u);
    write_u32_le(symbolized_bytes, 268u + 4u * 40u + 16u, 200u);
    write_u32_le(symbolized_bytes, 268u + 5u * 40u + 16u, 216u);

    if (!machine_elf_parse_file_from_bytes(symbolized_bytes, 548u, &parsed_file, &elf_error) ||
        !machine_elf_file_get_summary(&parsed_file, NULL, &symbol_count, NULL, NULL) ||
        symbol_count != 5u ||
        !machine_elf_normalize_bytes_from_bytes(
            symbolized_bytes,
            548u,
            &normalized_bytes,
            &normalized_byte_count,
            &elf_error) ||
        !normalized_bytes ||
        normalized_byte_count != byte_count ||
        memcmp(normalized_bytes, bytes, byte_count) != 0) {
        fprintf(stderr, "[machine-elf] FAIL: normalize should drop unused defined globals on unsupported sections: %s\n", elf_error.message);
        ok = 0;
    }

    if (ok) {
        unsigned char *invalid_bytes = (unsigned char *)malloc(548u);

        if (!invalid_bytes) {
            ok = 0;
        } else {
            memcpy(invalid_bytes, symbolized_bytes, 548u);
            write_u32_le(invalid_bytes, 200u + 4u, (4u << 8u) | 2u);
            if (machine_elf_parse_file_from_bytes(invalid_bytes, 548u, &invalid_file, &elf_error)) {
                fprintf(stderr, "[machine-elf] FAIL: parse should reject relocations targeting unsupported defined globals\n");
                ok = 0;
            }
        }
        free(invalid_bytes);
    }

cleanup:
    free(bytes);
    free(extended_bytes);
    free(symbolized_bytes);
    free(normalized_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    machine_elf_file_free(&invalid_file);
    return ok;
}

static int test_machine_elf_parse_synthesizes_missing_null_symbol(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfError elf_error;
    unsigned char *bytes = NULL;
    unsigned char *trimmed_bytes = NULL;
    unsigned char *normalized_bytes = NULL;
    size_t byte_count = 0u;
    size_t normalized_byte_count = 0u;
    size_t symbol_count = 0u;
    size_t first_global_symbol_index = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-null-symbol setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-null-symbol setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-null-symbol setup failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    trimmed_bytes = (unsigned char *)calloc(byte_count, 1u);
    if (!trimmed_bytes) {
        ok = 0;
        goto cleanup;
    }

    memcpy(trimmed_bytes, bytes, byte_count);
    memcpy(trimmed_bytes + 88u, bytes + 104u, 64u);
    memset(trimmed_bytes + 152u, 0, 16u);
    write_u32_le(trimmed_bytes, 168u + 4u, (2u << 8u) | 2u);
    write_u32_le(trimmed_bytes, 176u + 4u, (1u << 8u) | 3u);

    write_u32_le(trimmed_bytes, 228u + 3u * 40u + 20u, 64u);
    write_u32_le(trimmed_bytes, 228u + 3u * 40u + 28u, 3u);

    if (!machine_elf_parse_file_from_bytes(trimmed_bytes, byte_count, &parsed_file, &elf_error) ||
        !machine_elf_file_get_summary(&parsed_file, NULL, &symbol_count, NULL, NULL) ||
        symbol_count != 5u ||
        !machine_elf_file_get_first_global_symbol_index(&parsed_file, &first_global_symbol_index) ||
        first_global_symbol_index != 4u ||
        !machine_elf_normalize_bytes_from_bytes(
            trimmed_bytes,
            byte_count,
            &normalized_bytes,
            &normalized_byte_count,
            &elf_error) ||
        !normalized_bytes ||
        normalized_byte_count != byte_count ||
        memcmp(normalized_bytes, bytes, byte_count) != 0 ||
        !expect_round_trip_dump(&parsed_file)) {
        fprintf(stderr, "[machine-elf] FAIL: parse should synthesize missing null symbol: %s\n", elf_error.message);
        ok = 0;
    }

cleanup:
    free(bytes);
    free(trimmed_bytes);
    free(normalized_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    return ok;
}

static int test_machine_elf_parse_accepts_reordered_null_section_header(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfError elf_error;
    unsigned char *bytes = NULL;
    unsigned char *extended_bytes = NULL;
    unsigned char *reordered_bytes = NULL;
    unsigned char *normalized_bytes = NULL;
    size_t byte_count = 0u;
    size_t normalized_byte_count = 0u;
    size_t section_order[7] = {6u, 0u, 1u, 2u, 3u, 4u, 5u};
    size_t header_index;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: reordered-null-section setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: reordered-null-section setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: reordered-null-section setup failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    extended_bytes = (unsigned char *)calloc(516u, 1u);
    reordered_bytes = (unsigned char *)calloc(516u, 1u);
    if (!extended_bytes || !reordered_bytes) {
        ok = 0;
        goto cleanup;
    }

    memcpy(extended_bytes, bytes, 227u);
    memcpy(extended_bytes + 236u, bytes + 228u, 240u);
    memcpy(extended_bytes + 227u, ".comment", 9u);
    write_u32_le(extended_bytes, 32u, 236u);
    write_u16_le(extended_bytes, 48u, 7u);
    write_u32_le(extended_bytes, 236u + 5u * 40u + 20u, 52u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 0u, 43u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 4u, 1u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 32u, 1u);

    memcpy(reordered_bytes, extended_bytes, 516u);
    for (header_index = 0u; header_index < 7u; ++header_index) {
        memcpy(reordered_bytes + 236u + header_index * 40u,
            extended_bytes + 236u + section_order[header_index] * 40u,
            40u);
    }
    write_u16_le(reordered_bytes, 50u, 6u);
    write_u32_le(reordered_bytes, 236u + 4u * 40u + 24u, 3u);
    write_u32_le(reordered_bytes, 236u + 5u * 40u + 24u, 4u);
    write_u32_le(reordered_bytes, 236u + 5u * 40u + 28u, 2u);
    for (header_index = 1u; header_index < 5u; ++header_index) {
        write_u16_le(reordered_bytes, 88u + header_index * 16u + 14u, 2u);
    }

    if (!machine_elf_parse_file_from_bytes(reordered_bytes, 516u, &parsed_file, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: parse should accept reordered null section header: parse: %s\n", elf_error.message);
        ok = 0;
    } else if (!machine_elf_normalize_bytes_from_bytes(
                   reordered_bytes,
                   516u,
                   &normalized_bytes,
                   &normalized_byte_count,
                   &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: parse should accept reordered null section header: normalize: %s\n", elf_error.message);
        ok = 0;
    } else if (!normalized_bytes ||
               normalized_byte_count != byte_count ||
               memcmp(normalized_bytes, bytes, byte_count) != 0 ||
               !expect_round_trip_dump(&parsed_file)) {
        fprintf(stderr, "[machine-elf] FAIL: parse should accept reordered null section header: normalized result mismatch\n");
        ok = 0;
    }

cleanup:
    free(bytes);
    free(extended_bytes);
    free(reordered_bytes);
    free(normalized_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    return ok;
}

static int test_machine_elf_parse_synthesizes_missing_null_section(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfError elf_error;
    unsigned char *bytes = NULL;
    unsigned char *extended_bytes = NULL;
    unsigned char *trimmed_bytes = NULL;
    unsigned char *normalized_bytes = NULL;
    size_t byte_count = 0u;
    size_t normalized_byte_count = 0u;
    size_t header_index;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-null-section setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-null-section setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-null-section setup failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    extended_bytes = (unsigned char *)calloc(516u, 1u);
    trimmed_bytes = (unsigned char *)calloc(516u, 1u);
    if (!extended_bytes || !trimmed_bytes) {
        ok = 0;
        goto cleanup;
    }

    memcpy(extended_bytes, bytes, 227u);
    memcpy(extended_bytes + 236u, bytes + 228u, 240u);
    memcpy(extended_bytes + 227u, ".comment", 9u);
    write_u32_le(extended_bytes, 32u, 236u);
    write_u16_le(extended_bytes, 48u, 7u);
    write_u32_le(extended_bytes, 236u + 5u * 40u + 20u, 52u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 0u, 43u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 4u, 1u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 32u, 1u);

    memcpy(trimmed_bytes, extended_bytes, 516u);
    memcpy(trimmed_bytes + 236u, extended_bytes + 236u + 6u * 40u, 40u);
    memcpy(trimmed_bytes + 276u, extended_bytes + 236u + 1u * 40u, 200u);
    write_u16_le(trimmed_bytes, 48u, 6u);
    write_u16_le(trimmed_bytes, 50u, 5u);
    write_u32_le(trimmed_bytes, 236u + 3u * 40u + 24u, 2u);
    write_u32_le(trimmed_bytes, 236u + 4u * 40u + 24u, 3u);
    write_u32_le(trimmed_bytes, 236u + 4u * 40u + 28u, 1u);
    for (header_index = 1u; header_index < 5u; ++header_index) {
        write_u16_le(trimmed_bytes, 88u + header_index * 16u + 14u, 1u);
    }

    if (!machine_elf_parse_file_from_bytes(trimmed_bytes, 516u, &parsed_file, &elf_error) ||
        !machine_elf_normalize_bytes_from_bytes(
            trimmed_bytes,
            516u,
            &normalized_bytes,
            &normalized_byte_count,
            &elf_error) ||
        !normalized_bytes ||
        normalized_byte_count != byte_count ||
        memcmp(normalized_bytes, bytes, byte_count) != 0 ||
        !expect_round_trip_dump(&parsed_file)) {
        fprintf(stderr, "[machine-elf] FAIL: parse should synthesize missing null section: %s\n", elf_error.message);
        ok = 0;
    }

cleanup:
    free(bytes);
    free(extended_bytes);
    free(trimmed_bytes);
    free(normalized_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    return ok;
}

static int test_machine_elf_parse_synthesizes_missing_null_section_and_symbol(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfError elf_error;
    unsigned char *bytes = NULL;
    unsigned char *extended_bytes = NULL;
    unsigned char *trimmed_bytes = NULL;
    unsigned char *normalized_bytes = NULL;
    size_t byte_count = 0u;
    size_t normalized_byte_count = 0u;
    size_t header_index;
    size_t symbol_count = 0u;
    size_t first_global_symbol_index = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-null-both setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-null-both setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error) ||
        !machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-null-both setup failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    extended_bytes = (unsigned char *)calloc(516u, 1u);
    trimmed_bytes = (unsigned char *)calloc(516u, 1u);
    if (!extended_bytes || !trimmed_bytes) {
        ok = 0;
        goto cleanup;
    }

    memcpy(extended_bytes, bytes, 227u);
    memcpy(extended_bytes + 236u, bytes + 228u, 240u);
    memcpy(extended_bytes + 227u, ".comment", 9u);
    write_u32_le(extended_bytes, 32u, 236u);
    write_u16_le(extended_bytes, 48u, 7u);
    write_u32_le(extended_bytes, 236u + 5u * 40u + 20u, 52u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 0u, 43u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 4u, 1u);
    write_u32_le(extended_bytes, 236u + 6u * 40u + 32u, 1u);

    memcpy(trimmed_bytes, extended_bytes, 516u);
    memcpy(trimmed_bytes + 88u, extended_bytes + 104u, 64u);
    memset(trimmed_bytes + 152u, 0, 16u);
    write_u32_le(trimmed_bytes, 168u + 4u, (2u << 8u) | 2u);
    write_u32_le(trimmed_bytes, 176u + 4u, (1u << 8u) | 3u);

    memcpy(trimmed_bytes + 236u, extended_bytes + 236u + 6u * 40u, 40u);
    memcpy(trimmed_bytes + 276u, extended_bytes + 236u + 1u * 40u, 200u);
    write_u16_le(trimmed_bytes, 48u, 6u);
    write_u16_le(trimmed_bytes, 50u, 5u);
    write_u32_le(trimmed_bytes, 236u + 3u * 40u + 16u, 88u);
    write_u32_le(trimmed_bytes, 236u + 3u * 40u + 20u, 64u);
    write_u32_le(trimmed_bytes, 236u + 3u * 40u + 24u, 2u);
    write_u32_le(trimmed_bytes, 236u + 3u * 40u + 28u, 3u);
    write_u32_le(trimmed_bytes, 236u + 4u * 40u + 24u, 3u);
    write_u32_le(trimmed_bytes, 236u + 4u * 40u + 28u, 1u);
    for (header_index = 1u; header_index < 5u; ++header_index) {
        write_u16_le(trimmed_bytes, 88u + header_index * 16u + 14u, 1u);
    }

    if (!machine_elf_parse_file_from_bytes(trimmed_bytes, 516u, &parsed_file, &elf_error) ||
        !machine_elf_file_get_summary(&parsed_file, NULL, &symbol_count, NULL, NULL) ||
        symbol_count != 5u ||
        !machine_elf_file_get_first_global_symbol_index(&parsed_file, &first_global_symbol_index) ||
        first_global_symbol_index != 4u ||
        !machine_elf_normalize_bytes_from_bytes(
            trimmed_bytes,
            516u,
            &normalized_bytes,
            &normalized_byte_count,
            &elf_error) ||
        !normalized_bytes ||
        normalized_byte_count != byte_count ||
        memcmp(normalized_bytes, bytes, byte_count) != 0 ||
        !expect_round_trip_dump(&parsed_file)) {
        fprintf(stderr, "[machine-elf] FAIL: parse should synthesize missing null section and symbol: %s\n", elf_error.message);
        ok = 0;
    }

cleanup:
    free(bytes);
    free(extended_bytes);
    free(trimmed_bytes);
    free(normalized_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    return ok;
}

static int test_machine_elf_parse_accepts_missing_rel_text_section(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfError elf_error;
    const MachineElfSection *rel_text_section = NULL;
    unsigned char *bytes = NULL;
    unsigned char *trimmed_bytes = NULL;
    size_t byte_count = 0u;
    size_t normalized_byte_count = 0u;
    unsigned char *normalized_bytes = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-rel.text setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-rel.text setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-rel.text setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error) ||
        byte_count < 5u * 40u + 52u) {
        fprintf(stderr, "[machine-elf] FAIL: missing-rel.text source build failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    trimmed_bytes = (unsigned char *)malloc(byte_count);
    if (!trimmed_bytes) {
        ok = 0;
        goto cleanup;
    }
    memcpy(trimmed_bytes, bytes, byte_count);
    memcpy(trimmed_bytes + 388u, bytes + 428u, 40u);
    write_u16_le(trimmed_bytes, 48u, 5u);
    write_u16_le(trimmed_bytes, 50u, 4u);

    if (!machine_elf_parse_file_from_bytes(trimmed_bytes, byte_count, &parsed_file, &elf_error) ||
        !machine_elf_file_get_rel_text_section(&parsed_file, NULL, &rel_text_section) || !rel_text_section ||
        rel_text_section->byte_count != 0u ||
        rel_text_section->file_offset == 0u ||
        !machine_elf_normalize_bytes_from_bytes(
            trimmed_bytes,
            byte_count,
            &normalized_bytes,
            &normalized_byte_count,
            &elf_error) ||
        normalized_byte_count == 0u ||
        !expect_round_trip_dump(&parsed_file)) {
        fprintf(stderr, "[machine-elf] FAIL: missing-rel.text parse mismatch: %s\n", elf_error.message);
        ok = 0;
    }

cleanup:
    free(bytes);
    free(trimmed_bytes);
    free(normalized_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    return ok;
}

static int test_machine_elf_builds_from_machine_container_with_external_call(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineContainerFile container_file;
    MachineElfFile elf_file;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    MachineContainerError container_error;
    MachineElfError elf_error;
    MachineElfHeaderSummary header_summary;
    const MachineElfSection *section = NULL;
    const MachineElfSection *symtab_section = NULL;
    const MachineElfSymbol *symbol = NULL;
    const MachineElfRelocation *relocation = NULL;
    size_t symbol_index = 0;
    size_t first_global_symbol_index = 0u;
    MachineElfTargetProfile target_profile;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    memset(&container_error, 0, sizeof(container_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);
    machine_container_file_init(&container_file);
    machine_elf_file_init(&elf_file);

    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
        machine_container_file_free(&container_file);
        machine_elf_file_free(&elf_file);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1;
    emit_program.functions[0].blocks[0].op_capacity = 1;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
        machine_container_file_free(&container_file);
        machine_elf_file_free(&elf_file);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_VOID_IMM;
    emit_program.functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("sink");
    emit_program.functions[0].blocks[0].ops[0].as.call.arg_count = 1;
    emit_program.functions[0].blocks[0].ops[0].as.call.args = (MachineEmitOperand *)calloc(1, sizeof(MachineEmitOperand));
    if (!emit_program.functions[0].blocks[0].ops[0].as.call.callee_name ||
        !emit_program.functions[0].blocks[0].ops[0].as.call.args) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
        machine_container_file_free(&container_file);
        machine_elf_file_free(&elf_file);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].as.call.args[0] = machine_select_operand_immediate(5);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_bytes_lower_program_from_machine_encode(&encode_program, &bytes_program, &bytes_error) ||
        !machine_object_build_from_machine_bytes_program(&bytes_program, &object_file, &object_error) ||
        !machine_reloc_build_from_machine_object_file(&object_file, &reloc_file, &reloc_error) ||
        !machine_container_build_from_machine_reloc_file(&reloc_file, &container_file, &container_error) ||
        !machine_elf_build_from_machine_container_file_with_profile(
            &container_file,
            MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW,
            &elf_file,
            &elf_error) ||
        !machine_elf_file_get_target_profile(&elf_file, &target_profile) ||
        target_profile != MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW ||
        !machine_elf_file_get_header_summary(&elf_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW ||
        header_summary.machine != (uint16_t)EM_RISCV ||
        header_summary.section_header_count != 6u ||
        header_summary.section_name_string_table_index != 5u ||
        !machine_elf_file_get_rel_text_section(&elf_file, NULL, &section) || !section ||
        section->byte_count != 8u ||
        !machine_elf_file_get_symtab_section(&elf_file, NULL, &symtab_section) || !symtab_section ||
        !machine_elf_file_get_first_global_symbol_index(&elf_file, &first_global_symbol_index) ||
        first_global_symbol_index != 2u ||
        !machine_elf_file_find_symbol_by_name(&elf_file, "sink", &symbol_index, &symbol) || !symbol ||
        symbol->binding != MACHINE_ELF_SYMBOL_GLOBAL || symbol_index != 3u || !symbol->name ||
        symbol->is_defined ||
        !machine_elf_file_get_relocation(&elf_file, 0u, &relocation) || !relocation ||
        relocation->source_kind != MACHINE_BYTES_FIXUP_CALL_TARGET ||
        strcmp(relocation->symbol_name, "sink") != 0 || relocation->symbol_index != 3u ||
        relocation->type != (uint32_t)R_RISCV_CALL) {
        fprintf(stderr, "[machine-elf] FAIL: external elf mismatch: %s\n", elf_error.message);
        ok = 0;
    }
    ok &= expect_round_trip_dump(&elf_file);

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    machine_container_file_free(&container_file);
    machine_elf_file_free(&elf_file);
    return ok;
}

static int test_machine_elf_builds_riscv32_preview_profile(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile generic_elf_file;
    MachineElfError elf_error;
    MachineElfHeaderSummary header_summary;
    const MachineElfSection *text_section = NULL;
    const MachineElfSection *generic_text_section = NULL;
    const MachineElfRelocation *relocation = NULL;
    MachineElfTargetProfile target_profile;
    unsigned char *preview_bytes = NULL;
    unsigned char *generic_bytes = NULL;
    size_t preview_byte_count = 0u;
    size_t generic_byte_count = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&generic_elf_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: riscv machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: riscv machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: riscv machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW,
            &elf_file,
            &elf_error) ||
        !machine_elf_build_from_machine_ir_report(&machine_report, &generic_elf_file, &elf_error) ||
        !machine_elf_file_get_target_profile(&elf_file, &target_profile) ||
        target_profile != MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW ||
        !machine_elf_file_get_header_summary(&elf_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW ||
        header_summary.machine != (uint16_t)EM_RISCV ||
        header_summary.flags != 0u ||
        !machine_elf_file_get_text_section(&elf_file, NULL, &text_section) || !text_section ||
        text_section->byte_count != 28u ||
        !machine_elf_file_get_text_section(&generic_elf_file, NULL, &generic_text_section) || !generic_text_section ||
        generic_text_section->byte_count != 9u ||
        !machine_elf_file_copy_bytes(&elf_file, &preview_bytes, &preview_byte_count, &elf_error) ||
        !machine_elf_file_copy_bytes(&generic_elf_file, &generic_bytes, &generic_byte_count, &elf_error) ||
        !preview_bytes || !generic_bytes ||
        !machine_elf_file_get_relocation(&elf_file, 0u, &relocation) || !relocation ||
        relocation->source_kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        relocation->offset != 4u ||
        relocation->type != (uint32_t)R_RISCV_BRANCH ||
        !machine_elf_file_get_relocation(&elf_file, 1u, &relocation) || !relocation ||
        relocation->source_kind != MACHINE_BYTES_FIXUP_CONTROL_SECONDARY ||
        relocation->offset != 8u ||
        relocation->type != (uint32_t)R_RISCV_JAL) {
        fprintf(stderr, "[machine-elf] FAIL: riscv preview mismatch: %s\n", elf_error.message);
        ok = 0;
    }
    if (ok && preview_byte_count >= text_section->file_offset + text_section->byte_count &&
        generic_byte_count >= generic_text_section->file_offset + generic_text_section->byte_count &&
        memcmp(preview_bytes + text_section->file_offset,
            generic_bytes + generic_text_section->file_offset,
            generic_text_section->byte_count) == 0) {
        fprintf(stderr, "[machine-elf] FAIL: riscv preview text bytes should differ from generic bytes\n");
        ok = 0;
    }
    ok &= expect_round_trip_dump(&elf_file);

    free(preview_bytes);
    free(generic_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&generic_elf_file);
    return ok;
}

static int test_machine_elf_builds_i386_preview_profile(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfError elf_error;
    MachineElfHeaderSummary header_summary;
    const MachineElfRelocation *relocation = NULL;
    MachineElfTargetProfile target_profile;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: i386 machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: i386 machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: i386 machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &elf_file,
            &elf_error) ||
        !machine_elf_file_get_target_profile(&elf_file, &target_profile) ||
        target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        !machine_elf_file_get_header_summary(&elf_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        header_summary.machine != (uint16_t)EM_386 ||
        header_summary.flags != 0u ||
        !machine_elf_file_get_relocation(&elf_file, 0u, &relocation) || !relocation ||
        relocation->source_kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        relocation->type != (uint32_t)R_386_PC32 ||
        !machine_elf_file_get_relocation(&elf_file, 1u, &relocation) || !relocation ||
        relocation->source_kind != MACHINE_BYTES_FIXUP_CONTROL_SECONDARY ||
        relocation->type != (uint32_t)R_386_32PLT ||
        !expect_round_trip_dump(&elf_file)) {
        fprintf(stderr, "[machine-elf] FAIL: i386 preview mismatch: %s\n", elf_error.message);
        ok = 0;
    }

    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    return ok;
}

static int test_machine_elf_refresh_bytes_reprofiles_and_renames_symbol(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfError elf_error;
    MachineElfHeaderSummary header_summary;
    const MachineElfSymbol *symbol = NULL;
    const MachineElfRelocation *relocation = NULL;
    unsigned char *bytes = NULL;
    size_t byte_count = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: refresh machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: refresh machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: refresh machine-ir setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error) ||
        !bytes ||
        !machine_elf_parse_file_from_bytes(bytes, byte_count, &parsed_file, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: refresh setup failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    parsed_file.target_profile = MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW;
    free(parsed_file.symbols[4].name);
    parsed_file.symbols[4].name = dup_text("entry");
    free(parsed_file.relocations[0].symbol_name);
    parsed_file.relocations[0].symbol_name = dup_text("bogus");
    if (!parsed_file.symbols[4].name || !parsed_file.relocations[0].symbol_name) {
        ok = 0;
        goto cleanup;
    }

    if (!machine_elf_refresh_bytes(&parsed_file, &elf_error) ||
        !machine_elf_file_get_header_summary(&parsed_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW ||
        header_summary.machine != (uint16_t)EM_RISCV ||
        !machine_elf_file_find_symbol_by_name(&parsed_file, "entry", NULL, &symbol) || !symbol ||
        machine_elf_file_find_symbol_by_name(&parsed_file, "main", NULL, NULL) ||
        !machine_elf_file_get_relocation(&parsed_file, 0u, &relocation) || !relocation ||
        relocation->source_kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
        relocation->type != (uint32_t)R_RISCV_BRANCH ||
        strcmp(relocation->symbol_name, "F0.L2") != 0 ||
        !expect_round_trip_dump(&parsed_file)) {
        fprintf(stderr, "[machine-elf] FAIL: refresh result mismatch: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    parsed_file.relocations[0].symbol_index = parsed_file.symbol_count;
    if (machine_elf_refresh_bytes(&parsed_file, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: refresh should reject out-of-range relocation symbol index\n");
        ok = 0;
        goto cleanup;
    }

cleanup:
    free(bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    return ok;
}

static int test_machine_elf_builds_i386_preview_external_call(void) {
    MachineEmitProgram emit_program;
    MachineEncodeProgram encode_program;
    MachineBytesProgram bytes_program;
    MachineObjectFile object_file;
    MachineRelocFile reloc_file;
    MachineContainerFile container_file;
    MachineElfFile elf_file;
    MachineEncodeError encode_error;
    MachineBytesError bytes_error;
    MachineObjectError object_error;
    MachineRelocError reloc_error;
    MachineContainerError container_error;
    MachineElfError elf_error;
    MachineElfHeaderSummary header_summary;
    const MachineElfRelocation *relocation = NULL;
    MachineElfTargetProfile target_profile;
    int ok = 1;

    memset(&encode_error, 0, sizeof(encode_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&object_error, 0, sizeof(object_error));
    memset(&reloc_error, 0, sizeof(reloc_error));
    memset(&container_error, 0, sizeof(container_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_emit_program_init(&emit_program);
    machine_encode_program_init(&encode_program);
    machine_bytes_program_init(&bytes_program);
    machine_object_file_init(&object_file);
    machine_reloc_file_init(&reloc_file);
    machine_container_file_init(&container_file);
    machine_elf_file_init(&elf_file);

    emit_program.function_count = 1;
    emit_program.function_capacity = 1;
    emit_program.functions = (MachineEmitFunction *)calloc(1, sizeof(MachineEmitFunction));
    if (!emit_program.functions) {
        return 0;
    }
    emit_program.functions[0].name = dup_text("main");
    emit_program.functions[0].has_body = 1;
    emit_program.functions[0].block_count = 1;
    emit_program.functions[0].block_capacity = 1;
    emit_program.functions[0].blocks = (MachineEmitBlock *)calloc(1, sizeof(MachineEmitBlock));
    if (!emit_program.functions[0].name || !emit_program.functions[0].blocks) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
        machine_container_file_free(&container_file);
        machine_elf_file_free(&elf_file);
        return 0;
    }

    emit_program.functions[0].blocks[0].emit_index = 0;
    emit_program.functions[0].blocks[0].original_layout_index = 0;
    emit_program.functions[0].blocks[0].original_block_id = 0;
    emit_program.functions[0].blocks[0].label_name = dup_text("F0.L0");
    emit_program.functions[0].blocks[0].op_count = 1;
    emit_program.functions[0].blocks[0].op_capacity = 1;
    emit_program.functions[0].blocks[0].ops = (MachineEmitOp *)calloc(1, sizeof(MachineEmitOp));
    if (!emit_program.functions[0].blocks[0].label_name || !emit_program.functions[0].blocks[0].ops) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
        machine_container_file_free(&container_file);
        machine_elf_file_free(&elf_file);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].kind = MACHINE_SELECT_OP_CALL_VOID_IMM;
    emit_program.functions[0].blocks[0].ops[0].as.call.callee_name = dup_text("sink");
    emit_program.functions[0].blocks[0].ops[0].as.call.arg_count = 1;
    emit_program.functions[0].blocks[0].ops[0].as.call.args = (MachineEmitOperand *)calloc(1, sizeof(MachineEmitOperand));
    if (!emit_program.functions[0].blocks[0].ops[0].as.call.callee_name ||
        !emit_program.functions[0].blocks[0].ops[0].as.call.args) {
        machine_emit_program_free(&emit_program);
        machine_encode_program_free(&encode_program);
        machine_bytes_program_free(&bytes_program);
        machine_object_file_free(&object_file);
        machine_reloc_file_free(&reloc_file);
        machine_container_file_free(&container_file);
        machine_elf_file_free(&elf_file);
        return 0;
    }
    emit_program.functions[0].blocks[0].ops[0].as.call.args[0] = machine_select_operand_immediate(5);
    emit_program.functions[0].blocks[0].has_terminator = 1;
    emit_program.functions[0].blocks[0].terminator.kind = MACHINE_LAYOUT_TERM_RETURN_IMM;
    emit_program.functions[0].blocks[0].terminator.as.return_value = machine_select_operand_immediate(0);

    if (!machine_encode_lower_program_from_machine_emit(&emit_program, &encode_program, &encode_error) ||
        !machine_bytes_lower_program_from_machine_encode(&encode_program, &bytes_program, &bytes_error) ||
        !machine_object_build_from_machine_bytes_program(&bytes_program, &object_file, &object_error) ||
        !machine_reloc_build_from_machine_object_file(&object_file, &reloc_file, &reloc_error) ||
        !machine_container_build_from_machine_reloc_file(&reloc_file, &container_file, &container_error) ||
        !machine_elf_build_from_machine_container_file_with_profile(
            &container_file,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &elf_file,
            &elf_error) ||
        !machine_elf_file_get_target_profile(&elf_file, &target_profile) ||
        target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        !machine_elf_file_get_header_summary(&elf_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        header_summary.machine != (uint16_t)EM_386 ||
        !machine_elf_file_get_relocation(&elf_file, 0u, &relocation) || !relocation ||
        relocation->source_kind != MACHINE_BYTES_FIXUP_CALL_TARGET ||
        relocation->type != (uint32_t)R_386_PLT32 ||
        !expect_round_trip_dump(&elf_file)) {
        fprintf(stderr, "[machine-elf] FAIL: i386 preview external call mismatch: %s\n", elf_error.message);
        ok = 0;
    }

    machine_emit_program_free(&emit_program);
    machine_encode_program_free(&encode_program);
    machine_bytes_program_free(&bytes_program);
    machine_object_file_free(&object_file);
    machine_reloc_file_free(&reloc_file);
    machine_container_file_free(&container_file);
    machine_elf_file_free(&elf_file);
    return ok;
}

static int test_machine_elf_normalize_bytes_with_profile_retargets_header_and_relocs(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile parsed_file;
    MachineElfError elf_error;
    MachineElfHeaderSummary header_summary;
    const MachineElfRelocation *relocation = NULL;
    unsigned char *bytes = NULL;
    unsigned char *normalized_riscv = NULL;
    unsigned char *normalized_i386 = NULL;
    size_t byte_count = 0u;
    size_t normalized_riscv_count = 0u;
    size_t normalized_i386_count = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&parsed_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: normalize-with-profile setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: normalize-with-profile setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: normalize-with-profile setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&parsed_file);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error) ||
        !machine_elf_normalize_bytes_from_bytes_with_profile(
            bytes,
            byte_count,
            MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW,
            &normalized_riscv,
            &normalized_riscv_count,
            &elf_error) ||
        !machine_elf_parse_file_from_bytes(normalized_riscv, normalized_riscv_count, &parsed_file, &elf_error) ||
        !machine_elf_file_get_header_summary(&parsed_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW ||
        header_summary.machine != (uint16_t)EM_RISCV ||
        !machine_elf_file_get_relocation(&parsed_file, 0u, &relocation) || !relocation ||
        relocation->type != (uint32_t)R_RISCV_BRANCH ||
        !machine_elf_file_get_relocation(&parsed_file, 1u, &relocation) || !relocation ||
        relocation->type != (uint32_t)R_RISCV_JAL ||
        !machine_elf_normalize_bytes_from_bytes_with_profile(
            bytes,
            byte_count,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &normalized_i386,
            &normalized_i386_count,
            &elf_error) ||
        normalized_i386_count != byte_count ||
        !machine_elf_parse_file_from_bytes(normalized_i386, normalized_i386_count, &parsed_file, &elf_error) ||
        !machine_elf_file_get_header_summary(&parsed_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        header_summary.machine != (uint16_t)EM_386 ||
        !machine_elf_file_get_relocation(&parsed_file, 0u, &relocation) || !relocation ||
        relocation->type != (uint32_t)R_386_PC32 ||
        !machine_elf_file_get_relocation(&parsed_file, 1u, &relocation) || !relocation ||
        relocation->type != (uint32_t)R_386_32PLT) {
        fprintf(stderr, "[machine-elf] FAIL: normalize-with-profile mismatch: %s\n", elf_error.message);
        ok = 0;
    }

    free(bytes);
    free(normalized_riscv);
    free(normalized_i386);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&parsed_file);
    return ok;
}

static int test_machine_elf_clone_isolated_mutation_and_profile_dump(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile cloned_file;
    MachineElfError elf_error;
    unsigned char *bytes = NULL;
    size_t byte_count = 0u;
    char *profile_dump = NULL;
    const MachineElfSymbol *symbol = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&cloned_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: clone setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&cloned_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: clone setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&cloned_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: clone setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&cloned_file);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_clone_file(&elf_file, &cloned_file, &elf_error)) {
        fprintf(stderr, "[machine-elf] FAIL: clone failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    free(cloned_file.symbols[4].name);
    cloned_file.symbols[4].name = dup_text("entry");
    if (!cloned_file.symbols[4].name ||
        !machine_elf_refresh_bytes(&cloned_file, &elf_error) ||
        !machine_elf_file_find_symbol_by_name(&cloned_file, "entry", NULL, &symbol) || !symbol ||
        machine_elf_file_find_symbol_by_name(&elf_file, "entry", NULL, NULL) ||
        !machine_elf_file_find_symbol_by_name(&elf_file, "main", NULL, &symbol) || !symbol) {
        fprintf(stderr, "[machine-elf] FAIL: clone mutation isolation failed: %s\n", elf_error.message);
        ok = 0;
        goto cleanup;
    }

    if (!machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error) ||
        !machine_elf_build_dump_from_bytes_with_profile(
            bytes,
            byte_count,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &profile_dump,
            &elf_error) ||
        !profile_dump ||
        strstr(profile_dump, "machine_elf profile=i386-preview") == NULL ||
        strstr(profile_dump, "type=2") == NULL ||
        strstr(profile_dump, "type=11") == NULL) {
        fprintf(stderr, "[machine-elf] FAIL: profile dump helper mismatch: %s\n", elf_error.message);
        ok = 0;
    }

cleanup:
    free(bytes);
    free(profile_dump);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&cloned_file);
    return ok;
}

static int test_machine_elf_build_file_from_bytes_helpers(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfFile rebuilt_file;
    MachineElfFile rebuilt_i386_file;
    MachineElfError elf_error;
    MachineElfHeaderSummary header_summary;
    const MachineElfRelocation *relocation = NULL;
    unsigned char *bytes = NULL;
    size_t byte_count = 0u;
    MachineElfTargetProfile target_profile;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&rebuilt_file);
    machine_elf_file_init(&rebuilt_i386_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: build-from-bytes setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&rebuilt_file);
        machine_elf_file_free(&rebuilt_i386_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: build-from-bytes setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&rebuilt_file);
        machine_elf_file_free(&rebuilt_i386_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: build-from-bytes setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&rebuilt_file);
        machine_elf_file_free(&rebuilt_i386_file);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error) ||
        !machine_elf_build_file_from_bytes(bytes, byte_count, &rebuilt_file, &elf_error) ||
        !machine_elf_file_get_target_profile(&rebuilt_file, &target_profile) ||
        target_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        !machine_elf_file_get_header_summary(&rebuilt_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        header_summary.machine != 0u ||
        !machine_elf_build_file_from_bytes_with_profile(
            bytes,
            byte_count,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &rebuilt_i386_file,
            &elf_error) ||
        !machine_elf_file_get_target_profile(&rebuilt_i386_file, &target_profile) ||
        target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        !machine_elf_file_get_header_summary(&rebuilt_i386_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        header_summary.machine != (uint16_t)EM_386 ||
        !machine_elf_file_get_relocation(&rebuilt_i386_file, 0u, &relocation) || !relocation ||
        relocation->type != (uint32_t)R_386_PC32 ||
        !machine_elf_file_get_relocation(&rebuilt_i386_file, 1u, &relocation) || !relocation ||
        relocation->type != (uint32_t)R_386_32PLT ||
        !expect_round_trip_dump(&rebuilt_file) ||
        !expect_round_trip_dump(&rebuilt_i386_file)) {
        fprintf(stderr, "[machine-elf] FAIL: build-from-bytes helper mismatch: %s\n", elf_error.message);
        ok = 0;
    }

    free(bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&rebuilt_file);
    machine_elf_file_free(&rebuilt_i386_file);
    return ok;
}

static int test_machine_elf_build_report_helpers(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfReport report;
    MachineElfReport riscv_report;
    MachineElfError elf_error;
    const MachineElfHeaderSummary *header_summary = NULL;
    const MachineElfSectionSummary *section_summary = NULL;
    const MachineElfSymbolSummary *symbol_summary = NULL;
    const MachineElfRelocationSummary *relocation_summary = NULL;
    unsigned char *bytes = NULL;
    size_t byte_count = 0u;
    char *dump_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_report_init(&report);
    machine_elf_report_init(&riscv_report);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: report helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&report);
        machine_elf_report_free(&riscv_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: report helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&report);
        machine_elf_report_free(&riscv_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: report helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&report);
        machine_elf_report_free(&riscv_report);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error) ||
        !machine_elf_build_report_from_bytes_with_profile(
            bytes,
            byte_count,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &report,
            &elf_error) ||
        !machine_elf_report_get_header_summary_artifact(&report, &header_summary) || !header_summary ||
        header_summary->target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        header_summary->machine != (uint16_t)EM_386 ||
        !machine_elf_report_find_section_summary_by_name(&report, ".symtab", NULL, &section_summary) || !section_summary ||
        section_summary->info != 4u || section_summary->link != 2u ||
        !machine_elf_report_find_symbol_summary_by_name(&report, "main", NULL, &symbol_summary) || !symbol_summary ||
        symbol_summary->binding != MACHINE_ELF_SYMBOL_GLOBAL ||
        !machine_elf_report_get_relocation_summary(&report, 0u, &relocation_summary) || !relocation_summary ||
        relocation_summary->type != (uint32_t)R_386_PC32 ||
        !machine_elf_dump_report(&report, &dump_text, &elf_error) ||
        !dump_text || strstr(dump_text, "machine_elf profile=i386-preview") == NULL ||
        !machine_elf_build_report_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW,
            &riscv_report,
            &elf_error) ||
        !machine_elf_report_get_header_summary_artifact(&riscv_report, &header_summary) || !header_summary ||
        header_summary->target_profile != MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW ||
        header_summary->machine != (uint16_t)EM_RISCV ||
        !machine_elf_report_get_relocation_summary(&riscv_report, 1u, &relocation_summary) || !relocation_summary ||
        relocation_summary->type != (uint32_t)R_RISCV_JAL) {
        fprintf(stderr, "[machine-elf] FAIL: report helper mismatch: %s\n", elf_error.message);
        ok = 0;
    }

    free(bytes);
    free(dump_text);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_report_free(&report);
    machine_elf_report_free(&riscv_report);
    return ok;
}

static int test_machine_elf_report_summary_and_dump_helpers(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfReport report;
    MachineElfError elf_error;
    unsigned char *bytes = NULL;
    size_t section_count = 0u;
    size_t symbol_count = 0u;
    size_t relocation_count = 0u;
    size_t byte_count = 0u;
    char *dump_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_report_init(&report);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: report-summary setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: report-summary setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: report-summary setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&report);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_file_copy_bytes(&elf_file, &bytes, &byte_count, &elf_error) ||
        !machine_elf_build_report_from_file(&elf_file, &report, &elf_error) ||
        !machine_elf_report_get_summary(&report, &section_count, &symbol_count, &relocation_count, &byte_count) ||
        section_count != 6u || symbol_count != 5u || relocation_count != 2u || byte_count != 468u ||
        !machine_elf_build_report_dump_from_bytes_with_profile(
            bytes,
            byte_count,
            MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW,
            &dump_text,
            &elf_error) ||
        !dump_text ||
        strstr(dump_text, "machine_elf profile=riscv32-preview") == NULL ||
        strstr(dump_text, "type=16") == NULL ||
        strstr(dump_text, "type=17") == NULL) {
        fprintf(stderr, "[machine-elf] FAIL: report-summary helper mismatch: %s\n", elf_error.message);
        ok = 0;
    }

    free(bytes);
    free(dump_text);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_report_free(&report);
    return ok;
}

static int test_machine_elf_profile_helpers_from_file_and_machine_ir(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfReport i386_report;
    MachineElfError elf_error;
    const MachineElfHeaderSummary *header_summary = NULL;
    const MachineElfRelocationSummary *relocation_summary = NULL;
    char *dump_text = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_report_init(&i386_report);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: profile helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&i386_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: profile helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&i386_report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: profile helper setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&i386_report);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_build_report_from_file_with_profile(
            &elf_file,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &i386_report,
            &elf_error) ||
        !machine_elf_report_get_header_summary_artifact(&i386_report, &header_summary) || !header_summary ||
        header_summary->target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        header_summary->machine != (uint16_t)EM_386 ||
        !machine_elf_report_get_relocation_summary(&i386_report, 1u, &relocation_summary) || !relocation_summary ||
        relocation_summary->type != (uint32_t)R_386_32PLT ||
        !machine_elf_build_report_dump_from_file_with_profile(
            &elf_file,
            MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW,
            &dump_text,
            &elf_error) ||
        !dump_text ||
        strstr(dump_text, "machine_elf profile=riscv32-preview") == NULL ||
        strstr(dump_text, "type=16") == NULL ||
        strstr(dump_text, "type=17") == NULL) {
        fprintf(stderr, "[machine-elf] FAIL: profile helper mismatch: %s\n", elf_error.message);
        ok = 0;
    }

    free(dump_text);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_report_free(&i386_report);
    return ok;
}

static int test_machine_elf_report_dedicated_section_helpers(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfReport report;
    MachineElfError elf_error;
    const MachineElfSectionSummary *section_summary = NULL;
    size_t section_index = 0u;
    size_t first_global_symbol_index = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_report_init(&report);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: report-dedicated setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: report-dedicated setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: report-dedicated setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_report_free(&report);
        return 0;
    }

    if (!machine_elf_build_report_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &report,
            &elf_error) ||
        !machine_elf_report_get_text_section_summary(&report, &section_index, &section_summary) || !section_summary ||
        section_index != 1u || section_summary->type != MACHINE_ELF_SECTION_PROGBITS ||
        !machine_elf_report_get_strtab_section_summary(&report, &section_index, &section_summary) || !section_summary ||
        section_index != 2u || section_summary->type != MACHINE_ELF_SECTION_STRTAB ||
        !machine_elf_report_get_symtab_section_summary(&report, &section_index, &section_summary) || !section_summary ||
        section_index != 3u || section_summary->link != 2u ||
        !machine_elf_report_get_rel_text_section_summary(&report, &section_index, &section_summary) || !section_summary ||
        section_index != 4u || section_summary->link != 3u ||
        !machine_elf_report_get_shstrtab_section_summary(&report, &section_index, &section_summary) || !section_summary ||
        section_index != 5u || section_summary->type != MACHINE_ELF_SECTION_STRTAB ||
        !machine_elf_report_get_first_global_symbol_index(&report, &first_global_symbol_index) ||
        first_global_symbol_index != 4u) {
        fprintf(stderr, "[machine-elf] FAIL: report-dedicated helper mismatch: %s\n", elf_error.message);
        ok = 0;
    }

    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_report_free(&report);
    return ok;
}

static int test_machine_elf_target_policy_summary_helpers(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineElfFile elf_file;
    MachineElfReport report;
    MachineElfError elf_error;
    MachineElfTargetPolicySummary policy_summary;
    const MachineElfTargetPolicySummary *report_policy_summary = NULL;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&policy_summary, 0, sizeof(policy_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_elf_file_init(&elf_file);
    machine_elf_report_init(&report);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: target-policy setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: target-policy setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&report);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: target-policy setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_elf_file_free(&elf_file);
        machine_elf_report_free(&report);
        return 0;
    }

    if (!machine_elf_get_target_policy_summary(
            MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
            &policy_summary) ||
        policy_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 ||
        policy_summary.machine != 0u ||
        policy_summary.flags != 0u ||
        policy_summary.call_relocation_type != 1u ||
        policy_summary.primary_control_relocation_type != 2u ||
        policy_summary.secondary_control_relocation_type != 3u ||
        !machine_elf_build_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &elf_file,
            &elf_error) ||
        !machine_elf_file_get_target_policy_summary(&elf_file, &policy_summary) ||
        policy_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        policy_summary.machine != (uint16_t)EM_386 ||
        policy_summary.flags != 0u ||
        policy_summary.call_relocation_type != (uint32_t)R_386_PLT32 ||
        policy_summary.primary_control_relocation_type != (uint32_t)R_386_PC32 ||
        policy_summary.secondary_control_relocation_type != (uint32_t)R_386_32PLT ||
        !machine_elf_build_report_from_file_with_profile(
            &elf_file,
            MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW,
            &report,
            &elf_error) ||
        !machine_elf_report_get_target_policy_summary_artifact(&report, &report_policy_summary) ||
        !report_policy_summary ||
        report_policy_summary->target_profile != MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW ||
        report_policy_summary->machine != (uint16_t)EM_RISCV ||
        report_policy_summary->flags != 0u ||
        report_policy_summary->call_relocation_type != (uint32_t)R_RISCV_CALL ||
        report_policy_summary->primary_control_relocation_type != (uint32_t)R_RISCV_BRANCH ||
        report_policy_summary->secondary_control_relocation_type != (uint32_t)R_RISCV_JAL) {
        fprintf(stderr, "[machine-elf] FAIL: target-policy helper mismatch: %s\n", elf_error.message);
        ok = 0;
    }

    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_elf_file_free(&elf_file);
    machine_elf_report_free(&report);
    return ok;
}

static int test_machine_elf_build_bytes_helpers(void) {
    MachineIrAllocateRewriteReport machine_report;
    MachineIrFunction *function = NULL;
    MachineIrInstruction instruction;
    MachineIrError machine_error;
    MachineContainerFile container_file;
    MachineContainerError container_error;
    MachineElfFile elf_file;
    MachineElfFile rebuilt_file;
    MachineElfError elf_error;
    MachineElfHeaderSummary header_summary;
    unsigned char *file_bytes = NULL;
    unsigned char *container_bytes = NULL;
    unsigned char *ir_bytes = NULL;
    size_t file_byte_count = 0u;
    size_t container_byte_count = 0u;
    size_t ir_byte_count = 0u;
    int ok = 1;

    memset(&machine_error, 0, sizeof(machine_error));
    memset(&container_error, 0, sizeof(container_error));
    memset(&elf_error, 0, sizeof(elf_error));
    memset(&header_summary, 0, sizeof(header_summary));
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_container_file_init(&container_file);
    machine_elf_file_init(&elf_file);
    machine_elf_file_init(&rebuilt_file);

    machine_report.program.register_bank.register_count = 1;
    machine_report.program.register_bank.registers =
        (MachineIrRegisterDesc *)calloc(1, sizeof(MachineIrRegisterDesc));
    if (!machine_report.program.register_bank.registers) {
        return 0;
    }
    machine_report.program.register_bank.registers[0].register_id = 0;
    machine_report.program.register_bank.registers[0].name = dup_text("r0");
    machine_report.program.register_bank.registers[0].allocatable = 1u;

    if (!machine_ir_program_append_function(&machine_report.program, "main", 1, &function, &machine_error) ||
        !function ||
        !machine_ir_function_append_local(function, "a", 1, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error) ||
        !machine_ir_function_append_block(function, NULL, NULL, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: build-bytes setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_container_file_free(&container_file);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&rebuilt_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.load_slot = machine_ir_slot_local(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: build-bytes setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_container_file_free(&container_file);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&rebuilt_file);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MACHINE_IR_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = machine_ir_operand_register(0);
    instruction.as.binary.op = MACHINE_IR_BINARY_EQ;
    instruction.as.binary.lhs = machine_ir_operand_register(0);
    instruction.as.binary.rhs = machine_ir_operand_immediate(0);
    if (!machine_ir_block_append_instruction(&function->blocks[0], &instruction, &machine_error) ||
        !machine_ir_block_set_branch(&function->blocks[0], machine_ir_operand_register(0), 2, 1, &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[1], machine_ir_operand_immediate(1), &machine_error) ||
        !machine_ir_block_set_return(&function->blocks[2], machine_ir_operand_immediate(2), &machine_error)) {
        fprintf(stderr, "[machine-elf] FAIL: build-bytes setup failed: %s\n", machine_error.message);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        machine_container_file_free(&container_file);
        machine_elf_file_free(&elf_file);
        machine_elf_file_free(&rebuilt_file);
        return 0;
    }

    if (!machine_elf_build_from_machine_ir_report(&machine_report, &elf_file, &elf_error) ||
        !machine_elf_build_bytes_from_file_with_profile(
            &elf_file,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &file_bytes,
            &file_byte_count,
            &elf_error) ||
        !machine_elf_build_file_from_bytes(file_bytes, file_byte_count, &rebuilt_file, &elf_error) ||
        !machine_elf_file_get_header_summary(&rebuilt_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        header_summary.machine != (uint16_t)EM_386 ||
        !machine_container_build_from_machine_ir_report(&machine_report, &container_file, &container_error) ||
        !machine_elf_build_bytes_from_machine_container_file_with_profile(
            &container_file,
            MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW,
            &container_bytes,
            &container_byte_count,
            &elf_error) ||
        !machine_elf_build_file_from_bytes(container_bytes, container_byte_count, &rebuilt_file, &elf_error) ||
        !machine_elf_file_get_header_summary(&rebuilt_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW ||
        header_summary.machine != (uint16_t)EM_RISCV ||
        !machine_elf_build_bytes_from_machine_ir_report_with_profile(
            &machine_report,
            MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
            &ir_bytes,
            &ir_byte_count,
            &elf_error) ||
        !machine_elf_build_file_from_bytes(ir_bytes, ir_byte_count, &rebuilt_file, &elf_error) ||
        !machine_elf_file_get_header_summary(&rebuilt_file, &header_summary) ||
        header_summary.target_profile != MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ||
        header_summary.machine != (uint16_t)EM_386) {
        fprintf(stderr, "[machine-elf] FAIL: build-bytes helper mismatch: %s\n", elf_error.message);
        ok = 0;
    }

    free(file_bytes);
    free(container_bytes);
    free(ir_bytes);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    machine_container_file_free(&container_file);
    machine_elf_file_free(&elf_file);
    machine_elf_file_free(&rebuilt_file);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_machine_elf_builds_from_machine_ir_report();
    ok &= test_machine_elf_parse_accepts_droppable_local_section_symbols();
    ok &= test_machine_elf_parse_accepts_droppable_local_file_symbol();
    ok &= test_machine_elf_parse_accepts_droppable_local_symbol_on_extra_section();
    ok &= test_machine_elf_normalize_drops_unused_undefined_global_symbols();
    ok &= test_machine_elf_normalize_drops_unused_defined_global_symbols_on_unsupported_sections();
    ok &= test_machine_elf_parse_synthesizes_missing_null_symbol();
    ok &= test_machine_elf_parse_accepts_reordered_null_section_header();
    ok &= test_machine_elf_parse_synthesizes_missing_null_section();
    ok &= test_machine_elf_parse_synthesizes_missing_null_section_and_symbol();
    ok &= test_machine_elf_parse_accepts_missing_rel_text_section();
    ok &= test_machine_elf_builds_from_machine_container_with_external_call();
    ok &= test_machine_elf_builds_riscv32_preview_profile();
    ok &= test_machine_elf_builds_i386_preview_profile();
    ok &= test_machine_elf_refresh_bytes_reprofiles_and_renames_symbol();
    ok &= test_machine_elf_builds_i386_preview_external_call();
    ok &= test_machine_elf_normalize_bytes_with_profile_retargets_header_and_relocs();
    ok &= test_machine_elf_clone_isolated_mutation_and_profile_dump();
    ok &= test_machine_elf_build_file_from_bytes_helpers();
    ok &= test_machine_elf_build_report_helpers();
    ok &= test_machine_elf_report_summary_and_dump_helpers();
    ok &= test_machine_elf_profile_helpers_from_file_and_machine_ir();
    ok &= test_machine_elf_report_dedicated_section_helpers();
    ok &= test_machine_elf_target_policy_summary_helpers();
    ok &= test_machine_elf_build_bytes_helpers();

    if (!ok) {
        return 1;
    }

    printf("[machine-elf] PASS\n");
    return 0;
}
