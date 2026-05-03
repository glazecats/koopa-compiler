#include "compiler.h"

#include "ast.h"
#include "ir.h"
#include "lexer.h"
#include "lower_ir.h"
#include "machine/bytes.h"
#include "machine/ir.h"
#include "parser.h"
#include "semantic.h"
#include "value_ssa.h"
#include "value_ssa_pass.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define COMPILER_DEFAULT_COLOR_BUDGET 8u
#define COMPILER_DEFAULT_MACHINE_REGISTER_COUNT 8u

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} CompilerStringBuilder;

static void compiler_set_error(CompilerError *error, int line, int column, const char *message);
static char *compiler_read_file_text(const char *path, CompilerError *error);
static void compiler_copy_stage_error(CompilerError *error,
    int line,
    int column,
    const char *message);
static void compiler_builder_init(CompilerStringBuilder *builder);
static void compiler_builder_free(CompilerStringBuilder *builder);
static int compiler_builder_appendf(CompilerStringBuilder *builder, const char *fmt, ...);
static uint32_t compiler_read_u32_le(const unsigned char *bytes);
static int32_t compiler_sign_extend_u32(uint32_t value, unsigned bits);
static int32_t compiler_riscv_decode_i_imm(uint32_t word);
static int32_t compiler_riscv_decode_s_imm(uint32_t word);
static int32_t compiler_riscv_decode_b_imm(uint32_t word);
static int32_t compiler_riscv_decode_j_imm(uint32_t word);
static const char *compiler_riscv_register_name(uint32_t reg);
static int compiler_lookup_block_label(
    const MachineBytesReport *report,
    const char *raw_label_name,
    const char **out_function_name,
    size_t *out_block_index);
static int compiler_append_pretty_symbol_name(
    CompilerStringBuilder *builder,
    const MachineBytesReport *report,
    const char *raw_label_name);
static const char *compiler_find_global_name_for_gp_offset(const MachineBytesProgram *program, int32_t byte_offset);
static int compiler_emit_global_sections(
    CompilerStringBuilder *builder,
    const MachineBytesProgram *program);
static const MachineBytesFixupSummary *compiler_find_fixup_at_patch_offset(
    const MachineBytesReport *report,
    size_t patch_byte_offset);
static const char *compiler_find_label_at_program_byte_offset(
    const MachineBytesReport *report,
    size_t program_byte_offset);
static int compiler_append_riscv_preview_instruction(
    CompilerStringBuilder *builder,
    const MachineBytesReport *report,
    size_t program_byte_offset,
    uint32_t word);
static MachineBytesTargetProfile compiler_backend_profile_for_mode(CompilerMode mode);
static int compiler_emit_riscv_preview_text_from_report(const MachineIrAllocateRewriteReport *report,
    CompilerMode mode,
    char **out_text,
    CompilerError *error);

static void compiler_set_error(CompilerError *error, int line, int column, const char *message) {
    if (!error) {
        return;
    }
    error->line = line;
    error->column = column;
    if (!message) {
        error->message[0] = '\0';
        return;
    }
    snprintf(error->message, sizeof(error->message), "%s", message);
}

static void compiler_copy_stage_error(CompilerError *error,
    int line,
    int column,
    const char *message) {
    compiler_set_error(error, line, column, message ? message : "unknown compiler stage failure");
}

static void compiler_builder_init(CompilerStringBuilder *builder) {
    if (!builder) {
        return;
    }
    builder->data = NULL;
    builder->length = 0u;
    builder->capacity = 0u;
}

static void compiler_builder_free(CompilerStringBuilder *builder) {
    if (!builder) {
        return;
    }
    free(builder->data);
    builder->data = NULL;
    builder->length = 0u;
    builder->capacity = 0u;
}

static int compiler_builder_appendf(CompilerStringBuilder *builder, const char *fmt, ...) {
    va_list args;
    va_list measure_args;
    int needed;
    size_t required;
    char *new_data;

    if (!builder || !fmt) {
        return 0;
    }

    va_start(args, fmt);
    va_copy(measure_args, args);
    needed = vsnprintf(NULL, 0, fmt, measure_args);
    va_end(measure_args);
    if (needed < 0) {
        va_end(args);
        return 0;
    }

    required = builder->length + (size_t)needed + 1u;
    if (required > builder->capacity) {
        size_t new_capacity = builder->capacity > 0u ? builder->capacity : 128u;
        while (new_capacity < required) {
            new_capacity *= 2u;
        }
        new_data = (char *)realloc(builder->data, new_capacity);
        if (!new_data) {
            va_end(args);
            return 0;
        }
        builder->data = new_data;
        builder->capacity = new_capacity;
    }

    vsnprintf(builder->data + builder->length, builder->capacity - builder->length, fmt, args);
    va_end(args);
    builder->length += (size_t)needed;
    return 1;
}

static uint32_t compiler_read_u32_le(const unsigned char *bytes) {
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8u) |
        ((uint32_t)bytes[2] << 16u) |
        ((uint32_t)bytes[3] << 24u);
}

static int32_t compiler_sign_extend_u32(uint32_t value, unsigned bits) {
    uint32_t shift;

    if (bits == 0u || bits >= 32u) {
        return (int32_t)value;
    }
    shift = 32u - bits;
    return (int32_t)((int32_t)(value << shift) >> shift);
}

static int32_t compiler_riscv_decode_i_imm(uint32_t word) {
    return compiler_sign_extend_u32(word >> 20u, 12u);
}

static int32_t compiler_riscv_decode_s_imm(uint32_t word) {
    uint32_t value = ((word >> 7u) & 0x1Fu) | (((word >> 25u) & 0x7Fu) << 5u);
    return compiler_sign_extend_u32(value, 12u);
}

static int32_t compiler_riscv_decode_b_imm(uint32_t word) {
    uint32_t value = 0u;

    value |= ((word >> 8u) & 0xFu) << 1u;
    value |= ((word >> 25u) & 0x3Fu) << 5u;
    value |= ((word >> 7u) & 0x1u) << 11u;
    value |= ((word >> 31u) & 0x1u) << 12u;
    return compiler_sign_extend_u32(value, 13u);
}

static int32_t compiler_riscv_decode_j_imm(uint32_t word) {
    uint32_t value = 0u;

    value |= ((word >> 21u) & 0x3FFu) << 1u;
    value |= ((word >> 20u) & 0x1u) << 11u;
    value |= ((word >> 12u) & 0xFFu) << 12u;
    value |= ((word >> 31u) & 0x1u) << 20u;
    return compiler_sign_extend_u32(value, 21u);
}

static const char *compiler_riscv_register_name(uint32_t reg) {
    static const char *const names[] = {"zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
        "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
        "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
        "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

    if (reg < (sizeof(names) / sizeof(names[0]))) {
        return names[reg];
    }
    return "x?";
}

static int compiler_lookup_block_label(
    const MachineBytesReport *report,
    const char *raw_label_name,
    const char **out_function_name,
    size_t *out_block_index) {
    size_t function_count = 0u;
    size_t total_block_summary_count = 0u;
    size_t function_index;

    if (out_function_name) {
        *out_function_name = NULL;
    }
    if (out_block_index) {
        *out_block_index = 0u;
    }
    if (!report || !raw_label_name ||
        !machine_bytes_report_get_summary(report, NULL, NULL, &function_count, &total_block_summary_count)) {
        return 0;
    }
    (void)total_block_summary_count;
    for (function_index = 0u; function_index < function_count; ++function_index) {
        const MachineBytesFunction *function = NULL;
        const char *function_name = NULL;
        int has_body = 0;
        size_t parameter_count = 0u;
        size_t local_count = 0u;
        size_t block_count = 0u;
        size_t spill_slot_count = 0u;
        size_t block_index;

        if (!machine_bytes_report_get_function(report, function_index, &function) || !function ||
            !machine_bytes_function_get_summary(
                function,
                &function_name,
                &has_body,
                &parameter_count,
                &local_count,
                &block_count,
                &spill_slot_count)) {
            return 0;
        }
        (void)parameter_count;
        (void)local_count;
        (void)spill_slot_count;
        if (!has_body || !function_name) {
            continue;
        }
        for (block_index = 0u; block_index < block_count; ++block_index) {
            const MachineBytesBlockSummary *block_summary = NULL;

            if (!machine_bytes_report_get_block_summary(report, function_index, block_index, &block_summary) ||
                !block_summary || !block_summary->label_name) {
                return 0;
            }
            if (strcmp(block_summary->label_name, raw_label_name) == 0) {
                if (out_function_name) {
                    *out_function_name = function_name;
                }
                if (out_block_index) {
                    *out_block_index = block_index;
                }
                return 1;
            }
        }
    }
    return 0;
}

static int compiler_append_pretty_symbol_name(
    CompilerStringBuilder *builder,
    const MachineBytesReport *report,
    const char *raw_label_name) {
    const char *function_name = NULL;
    size_t block_index = 0u;

    if (!builder || !raw_label_name) {
        return 0;
    }
    if (compiler_lookup_block_label(report, raw_label_name, &function_name, &block_index) && function_name) {
        return compiler_builder_appendf(builder, ".L%s_%zu", function_name, block_index);
    }
    return compiler_builder_appendf(builder, "%s", raw_label_name);
}

static const char *compiler_find_global_name_for_gp_offset(const MachineBytesProgram *program, int32_t byte_offset) {
    size_t global_index;

    if (!program || byte_offset < 0 || (byte_offset % 4) != 0) {
        return NULL;
    }
    global_index = (size_t)(byte_offset / 4);
    if (global_index >= program->global_count) {
        return NULL;
    }
    if (!program->globals || !program->globals[global_index].name || program->globals[global_index].name[0] == '\0') {
        return NULL;
    }
    return program->globals[global_index].name;
}

static int compiler_emit_global_sections(
    CompilerStringBuilder *builder,
    const MachineBytesProgram *program) {
    size_t global_index;
    int has_bss = 0;
    int has_data = 0;

    if (!builder || !program) {
        return 0;
    }

    for (global_index = 0u; global_index < program->global_count; ++global_index) {
        const MachineEmitGlobal *global = &program->globals[global_index];

        if (!global->name || global->name[0] == '\0') {
            continue;
        }
        if (global->has_initializer) {
            has_data = 1;
        } else {
            has_bss = 1;
        }
    }

    if (has_bss) {
        if (!compiler_builder_appendf(builder, ".section .sbss,\"aw\",@nobits\n")) {
            return 0;
        }
        for (global_index = 0u; global_index < program->global_count; ++global_index) {
            const MachineEmitGlobal *global = &program->globals[global_index];

            if (!global->name || global->name[0] == '\0' || global->has_initializer) {
                continue;
            }
            if (!compiler_builder_appendf(
                    builder,
                    ".globl %s\n.type %s, @object\n.size %s, 4\n.p2align 2\n%s:\n  .zero 4\n",
                    global->name,
                    global->name,
                    global->name,
                    global->name)) {
                return 0;
            }
        }
        if (!compiler_builder_appendf(builder, "\n")) {
            return 0;
        }
    }

    if (has_data) {
        if (!compiler_builder_appendf(builder, ".section .sdata,\"aw\",@progbits\n")) {
            return 0;
        }
        for (global_index = 0u; global_index < program->global_count; ++global_index) {
            const MachineEmitGlobal *global = &program->globals[global_index];

            if (!global->name || global->name[0] == '\0' || !global->has_initializer) {
                continue;
            }
            if (!compiler_builder_appendf(
                    builder,
                    ".globl %s\n.type %s, @object\n.size %s, 4\n.p2align 2\n%s:\n  .word %lld\n",
                    global->name,
                    global->name,
                    global->name,
                    global->name,
                    global->initializer_value)) {
                return 0;
            }
        }
        if (!compiler_builder_appendf(builder, "\n")) {
            return 0;
        }
    }

    return 1;
}

static const MachineBytesFixupSummary *compiler_find_fixup_at_patch_offset(
    const MachineBytesReport *report,
    size_t patch_byte_offset) {
    size_t fixup_count = 0u;
    size_t fixup_index;

    if (!report || !machine_bytes_report_get_fixup_summary_count(report, &fixup_count)) {
        return NULL;
    }
    for (fixup_index = 0u; fixup_index < fixup_count; ++fixup_index) {
        const MachineBytesFixupSummary *fixup = NULL;

        if (!machine_bytes_report_get_fixup_summary(report, fixup_index, &fixup) || !fixup) {
            return NULL;
        }
        if (fixup->patch_byte_offset == patch_byte_offset) {
            return fixup;
        }
    }
    return NULL;
}

static const char *compiler_find_label_at_program_byte_offset(
    const MachineBytesReport *report,
    size_t program_byte_offset) {
    size_t function_count = 0u;
    size_t total_block_summary_count = 0u;
    size_t function_index;

    if (!report ||
        !machine_bytes_report_get_summary(report, NULL, NULL, &function_count, &total_block_summary_count)) {
        return NULL;
    }
    (void)total_block_summary_count;
    for (function_index = 0u; function_index < function_count; ++function_index) {
        const MachineBytesFunction *function = NULL;
        const char *function_name = NULL;
        int has_body = 0;
        size_t parameter_count = 0u;
        size_t local_count = 0u;
        size_t block_count = 0u;
        size_t spill_slot_count = 0u;
        size_t function_start = 0u;
        size_t function_end = 0u;
        size_t block_index;

        if (!machine_bytes_report_get_function(report, function_index, &function) || !function ||
            !machine_bytes_function_get_summary(
                function,
                &function_name,
                &has_body,
                &parameter_count,
                &local_count,
                &block_count,
                &spill_slot_count) ||
            !machine_bytes_report_get_function_byte_span(report, function_index, &function_start, &function_end)) {
            return NULL;
        }
        (void)parameter_count;
        (void)local_count;
        (void)spill_slot_count;
        if (!has_body || program_byte_offset < function_start || program_byte_offset >= function_end) {
            continue;
        }
        for (block_index = 0u; block_index < block_count; ++block_index) {
            const MachineBytesBlockSummary *block_summary = NULL;
            size_t absolute_start;

            if (!machine_bytes_report_get_block_summary(report, function_index, block_index, &block_summary) ||
                !block_summary) {
                return NULL;
            }
            absolute_start = function_start + block_summary->start_byte_offset;
            if (absolute_start == program_byte_offset) {
                return block_summary->label_name;
            }
        }
        if (function_start == program_byte_offset) {
            return function_name;
        }
        return NULL;
    }
    return NULL;
}

static int compiler_append_riscv_preview_instruction(
    CompilerStringBuilder *builder,
    const MachineBytesReport *report,
    size_t program_byte_offset,
    uint32_t word) {
    uint32_t opcode = word & 0x7Fu;
    uint32_t rd = (word >> 7u) & 0x1Fu;
    uint32_t funct3 = (word >> 12u) & 0x7u;
    uint32_t rs1 = (word >> 15u) & 0x1Fu;
    uint32_t rs2 = (word >> 20u) & 0x1Fu;
    uint32_t funct7 = (word >> 25u) & 0x7Fu;
    const MachineBytesFixupSummary *fixup = compiler_find_fixup_at_patch_offset(report, program_byte_offset);
    const char *target_name = NULL;

    switch (opcode) {
        case 0x03u:
            if (funct3 == 0x2u) {
                if (fixup &&
                    fixup->kind == MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET &&
                    fixup->target_name && fixup->target_name[0] != '\0') {
                    return compiler_builder_appendf(
                        builder,
                        "  lw %s, %%lo(%s)(%s)\n",
                        compiler_riscv_register_name(rd),
                        fixup->target_name,
                        compiler_riscv_register_name(rs1));
                }
                const char *global_name = report ? compiler_find_global_name_for_gp_offset(&report->program, compiler_riscv_decode_i_imm(word)) : NULL;

                if (rs1 == 3u && global_name) {
                    return compiler_builder_appendf(
                        builder,
                        "  lw %s, %d(gp) # %s\n",
                        compiler_riscv_register_name(rd),
                        compiler_riscv_decode_i_imm(word),
                        global_name);
                }
                return compiler_builder_appendf(
                    builder,
                    "  lw %s, %d(%s)\n",
                    compiler_riscv_register_name(rd),
                    compiler_riscv_decode_i_imm(word),
                    compiler_riscv_register_name(rs1));
            }
            break;
        case 0x13u: {
            int32_t imm = compiler_riscv_decode_i_imm(word);

            switch (funct3) {
                case 0x0u:
                    if (rd == 0u && rs1 == 0u && imm == 0) {
                        return compiler_builder_appendf(builder, "  nop\n");
                    }
                    if (rs1 == 0u) {
                        return compiler_builder_appendf(
                            builder, "  li %s, %d\n", compiler_riscv_register_name(rd), imm);
                    }
                    if (imm == 0) {
                        return compiler_builder_appendf(
                            builder,
                            "  mv %s, %s\n",
                            compiler_riscv_register_name(rd),
                            compiler_riscv_register_name(rs1));
                    }
                    return compiler_builder_appendf(
                        builder,
                        "  addi %s, %s, %d\n",
                        compiler_riscv_register_name(rd),
                        compiler_riscv_register_name(rs1),
                        imm);
                case 0x1u:
                    return compiler_builder_appendf(
                        builder,
                        "  slli %s, %s, %u\n",
                        compiler_riscv_register_name(rd),
                        compiler_riscv_register_name(rs1),
                        (unsigned)((word >> 20u) & 0x1Fu));
                case 0x2u:
                    return compiler_builder_appendf(
                        builder,
                        "  slti %s, %s, %d\n",
                        compiler_riscv_register_name(rd),
                        compiler_riscv_register_name(rs1),
                        imm);
                case 0x3u:
                    return compiler_builder_appendf(
                        builder,
                        "  sltiu %s, %s, %d\n",
                        compiler_riscv_register_name(rd),
                        compiler_riscv_register_name(rs1),
                        imm);
                case 0x4u:
                    return compiler_builder_appendf(
                        builder,
                        "  xori %s, %s, %d\n",
                        compiler_riscv_register_name(rd),
                        compiler_riscv_register_name(rs1),
                        imm);
                case 0x5u:
                    if (funct7 == 0x20u) {
                        return compiler_builder_appendf(
                            builder,
                            "  srai %s, %s, %u\n",
                            compiler_riscv_register_name(rd),
                            compiler_riscv_register_name(rs1),
                            (unsigned)((word >> 20u) & 0x1Fu));
                    }
                    return compiler_builder_appendf(
                        builder,
                        "  srli %s, %s, %u\n",
                        compiler_riscv_register_name(rd),
                        compiler_riscv_register_name(rs1),
                        (unsigned)((word >> 20u) & 0x1Fu));
                case 0x6u:
                    return compiler_builder_appendf(
                        builder,
                        "  ori %s, %s, %d\n",
                        compiler_riscv_register_name(rd),
                        compiler_riscv_register_name(rs1),
                        imm);
                case 0x7u:
                    return compiler_builder_appendf(
                        builder,
                        "  andi %s, %s, %d\n",
                        compiler_riscv_register_name(rd),
                        compiler_riscv_register_name(rs1),
                        imm);
                default:
                    break;
            }
            break;
        }
        case 0x23u:
            if (funct3 == 0x2u) {
                if (fixup &&
                    fixup->kind == MACHINE_BYTES_FIXUP_DATA_STORE_TARGET &&
                    fixup->target_name && fixup->target_name[0] != '\0') {
                    return compiler_builder_appendf(
                        builder,
                        "  sw %s, %%lo(%s)(%s)\n",
                        compiler_riscv_register_name(rs2),
                        fixup->target_name,
                        compiler_riscv_register_name(rs1));
                }
                const char *global_name = report ? compiler_find_global_name_for_gp_offset(&report->program, compiler_riscv_decode_s_imm(word)) : NULL;

                if (rs1 == 3u && global_name) {
                    return compiler_builder_appendf(
                        builder,
                        "  sw %s, %d(gp) # %s\n",
                        compiler_riscv_register_name(rs2),
                        compiler_riscv_decode_s_imm(word),
                        global_name);
                }
                return compiler_builder_appendf(
                    builder,
                    "  sw %s, %d(%s)\n",
                    compiler_riscv_register_name(rs2),
                    compiler_riscv_decode_s_imm(word),
                    compiler_riscv_register_name(rs1));
            }
            break;
        case 0x33u: {
            const char *mnemonic = NULL;

            if (funct7 == 0x00u && funct3 == 0x0u) {
                mnemonic = "add";
            } else if (funct7 == 0x20u && funct3 == 0x0u) {
                mnemonic = "sub";
            } else if (funct7 == 0x00u && funct3 == 0x1u) {
                mnemonic = "sll";
            } else if (funct7 == 0x00u && funct3 == 0x2u) {
                mnemonic = "slt";
            } else if (funct7 == 0x00u && funct3 == 0x3u) {
                mnemonic = "sltu";
            } else if (funct7 == 0x00u && funct3 == 0x4u) {
                mnemonic = "xor";
            } else if (funct7 == 0x00u && funct3 == 0x5u) {
                mnemonic = "srl";
            } else if (funct7 == 0x20u && funct3 == 0x5u) {
                mnemonic = "sra";
            } else if (funct7 == 0x00u && funct3 == 0x6u) {
                mnemonic = "or";
            } else if (funct7 == 0x00u && funct3 == 0x7u) {
                mnemonic = "and";
            } else if (funct7 == 0x01u && funct3 == 0x0u) {
                mnemonic = "mul";
            } else if (funct7 == 0x01u && funct3 == 0x4u) {
                mnemonic = "div";
            } else if (funct7 == 0x01u && funct3 == 0x6u) {
                mnemonic = "rem";
            }
            if (mnemonic) {
                return compiler_builder_appendf(
                    builder,
                    "  %s %s, %s, %s\n",
                    mnemonic,
                    compiler_riscv_register_name(rd),
                    compiler_riscv_register_name(rs1),
                    compiler_riscv_register_name(rs2));
            }
            break;
        }
        case 0x37u:
            if (fixup &&
                fixup->kind == MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET &&
                fixup->target_name && fixup->target_name[0] != '\0') {
                return compiler_builder_appendf(
                    builder,
                    "  lui %s, %%hi(%s)\n",
                    compiler_riscv_register_name(rd),
                    fixup->target_name);
            }
            return compiler_builder_appendf(
                builder,
                "  lui %s, 0x%x\n",
                compiler_riscv_register_name(rd),
                (unsigned)(word >> 12u));
        case 0x63u: {
            const char *mnemonic = NULL;
            int32_t imm = compiler_riscv_decode_b_imm(word);

            if (fixup && fixup->target_name && fixup->target_name[0] != '\0') {
                target_name = fixup->target_name;
            } else {
                target_name = compiler_find_label_at_program_byte_offset(report, program_byte_offset + (size_t)imm);
            }
            switch (funct3) {
                case 0x0u:
                    mnemonic = "beq";
                    break;
                case 0x1u:
                    mnemonic = "bne";
                    break;
                case 0x4u:
                    mnemonic = "blt";
                    break;
                case 0x5u:
                    mnemonic = "bge";
                    break;
                case 0x6u:
                    mnemonic = "bltu";
                    break;
                case 0x7u:
                    mnemonic = "bgeu";
                    break;
                default:
                    break;
            }
            if (mnemonic && target_name) {
                if (!compiler_builder_appendf(
                        builder,
                        "  %s %s, %s, ",
                        mnemonic,
                        compiler_riscv_register_name(rs1),
                        compiler_riscv_register_name(rs2)) ||
                    !compiler_append_pretty_symbol_name(builder, report, target_name) ||
                    !compiler_builder_appendf(builder, "\n")) {
                    return 0;
                }
                return 1;
            }
            if (mnemonic) {
                return compiler_builder_appendf(
                    builder,
                    "  %s %s, %s, %d\n",
                    mnemonic,
                    compiler_riscv_register_name(rs1),
                    compiler_riscv_register_name(rs2),
                    imm);
            }
            break;
        }
        case 0x67u: {
            int32_t imm = compiler_riscv_decode_i_imm(word);

            if (rd == 0u && rs1 == 1u && imm == 0) {
                return compiler_builder_appendf(builder, "  ret\n");
            }
            return compiler_builder_appendf(
                builder,
                "  jalr %s, %d(%s)\n",
                compiler_riscv_register_name(rd),
                imm,
                compiler_riscv_register_name(rs1));
        }
        case 0x6Fu: {
            int32_t imm = compiler_riscv_decode_j_imm(word);

            if (fixup && fixup->target_name && fixup->target_name[0] != '\0') {
                target_name = fixup->target_name;
            } else {
                target_name = compiler_find_label_at_program_byte_offset(report, program_byte_offset + (size_t)imm);
            }
            if (rd == 0u) {
                if (target_name) {
                    if (!compiler_builder_appendf(builder, "  j ") ||
                        !compiler_append_pretty_symbol_name(builder, report, target_name) ||
                        !compiler_builder_appendf(builder, "\n")) {
                        return 0;
                    }
                    return 1;
                }
                return compiler_builder_appendf(builder, "  jal zero, %d\n", imm);
            }
            if (target_name) {
                if (!compiler_builder_appendf(builder, "  jal %s, ", compiler_riscv_register_name(rd)) ||
                    !compiler_append_pretty_symbol_name(builder, report, target_name) ||
                    !compiler_builder_appendf(builder, "\n")) {
                    return 0;
                }
                return 1;
            }
            return compiler_builder_appendf(builder, "  jal %s, %d\n", compiler_riscv_register_name(rd), imm);
        }
        default:
            break;
    }

    return compiler_builder_appendf(builder, "  .4byte 0x%08x\n", (unsigned int)word);
}

static char *compiler_read_file_text(const char *path, CompilerError *error) {
    FILE *fp;
    char *buffer = NULL;
    long file_size;
    size_t nread;

    if (!path) {
        compiler_set_error(error, 0, 0, "COMPILER-101: invalid input path");
        return NULL;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        compiler_set_error(error, 0, 0, "COMPILER-102: failed to open input file");
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        compiler_set_error(error, 0, 0, "COMPILER-103: failed to seek input file");
        return NULL;
    }
    file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        compiler_set_error(error, 0, 0, "COMPILER-104: failed to measure input file");
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        compiler_set_error(error, 0, 0, "COMPILER-105: failed to rewind input file");
        return NULL;
    }
    buffer = (char *)malloc((size_t)file_size + 1u);
    if (!buffer) {
        fclose(fp);
        compiler_set_error(error, 0, 0, "COMPILER-106: out of memory reading input file");
        return NULL;
    }
    nread = fread(buffer, 1u, (size_t)file_size, fp);
    fclose(fp);
    if (nread != (size_t)file_size) {
        free(buffer);
        compiler_set_error(error, 0, 0, "COMPILER-107: failed to read full input file");
        return NULL;
    }
    buffer[file_size] = '\0';
    return buffer;
}

static MachineBytesTargetProfile compiler_backend_profile_for_mode(CompilerMode mode) {
    switch (mode) {
        case COMPILER_MODE_PERF:
        case COMPILER_MODE_RISCV:
        default:
            return MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW;
    }
}

static int compiler_emit_riscv_preview_text_from_report(const MachineIrAllocateRewriteReport *report,
    CompilerMode mode,
    char **out_text,
    CompilerError *error) {
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    const MachineBytesProgram *program = NULL;
    size_t symbol_count = 0u;
    CompilerStringBuilder builder;
    int ok = 0;
    size_t symbol_index;
    size_t function_index;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        compiler_set_error(error, 0, 0, "COMPILER-109: invalid riscv text export contract");
        return 0;
    }

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_bytes_report_init(&bytes_report);
    compiler_builder_init(&builder);

    ok = machine_bytes_build_report_from_machine_ir_report_with_profile(
        report,
        compiler_backend_profile_for_mode(mode),
        &bytes_report,
        &bytes_error);
    if (!ok) {
        compiler_copy_stage_error(error, bytes_error.line, bytes_error.column, bytes_error.message);
        goto cleanup;
    }
    if (!machine_bytes_report_get_program(&bytes_report, &program) || !program) {
        compiler_set_error(error, 0, 0, "COMPILER-110: missing bytes program in riscv text export");
        ok = 0;
        goto cleanup;
    }
    if (!compiler_builder_appendf(&builder, ".attribute arch, \"rv32im\"\n") ||
        !compiler_emit_global_sections(&builder, program) ||
        !compiler_builder_appendf(&builder, ".text\n.p2align 2\n")) {
        compiler_set_error(error, 0, 0, "COMPILER-111: out of memory building riscv text");
        ok = 0;
        goto cleanup;
    }

    if (!machine_bytes_report_get_symbol_summary_count(&bytes_report, &symbol_count)) {
        compiler_set_error(error, 0, 0, "COMPILER-112: missing bytes symbol summary surface");
        ok = 0;
        goto cleanup;
    }
    for (symbol_index = 0u; symbol_index < symbol_count; ++symbol_index) {
        const MachineBytesSymbolSummary *symbol = NULL;

        if (!machine_bytes_report_get_symbol_summary(&bytes_report, symbol_index, &symbol) || !symbol) {
            compiler_set_error(error, 0, 0, "COMPILER-113: malformed bytes symbol summary");
            ok = 0;
            goto cleanup;
        }
        if (symbol->kind == MACHINE_BYTES_SYMBOL_EXTERNAL && !symbol->is_defined && symbol->name &&
            symbol->name[0] != '\0') {
            if (!compiler_builder_appendf(&builder, ".globl %s\n.type %s, @function\n", symbol->name, symbol->name)) {
                compiler_set_error(error, 0, 0, "COMPILER-114: out of memory writing external declaration");
                ok = 0;
                goto cleanup;
            }
        }
    }

    if (program->function_count > 0u) {
        if (!compiler_builder_appendf(&builder, "\n")) {
            compiler_set_error(error, 0, 0, "COMPILER-115: out of memory separating text header");
            ok = 0;
            goto cleanup;
        }
    }
    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        const MachineBytesFunction *function = NULL;
        const char *function_name = NULL;
        int has_body = 0;
        size_t parameter_count = 0u;
        size_t local_count = 0u;
        size_t block_count = 0u;
        size_t spill_slot_count = 0u;
        size_t block_index;

        if (!machine_bytes_report_get_function(&bytes_report, function_index, &function) || !function ||
            !machine_bytes_function_get_summary(
                function,
                &function_name,
                &has_body,
                &parameter_count,
                &local_count,
                &block_count,
                &spill_slot_count)) {
            compiler_set_error(error, 0, 0, "COMPILER-116: malformed bytes function surface");
            ok = 0;
            goto cleanup;
        }
        (void)parameter_count;
        (void)local_count;
        (void)spill_slot_count;
        if (!has_body || !function_name || function_name[0] == '\0') {
            continue;
        }

        if (!compiler_builder_appendf(&builder, ".globl %s\n.type %s, @function\n%s:\n", function_name, function_name, function_name)) {
            compiler_set_error(error, 0, 0, "COMPILER-117: out of memory writing function header");
            ok = 0;
            goto cleanup;
        }

        for (block_index = 0u; block_index < block_count; ++block_index) {
            const MachineBytesBlock *block = NULL;
            MachineBytesBlockSummary block_summary;
            const unsigned char *block_bytes = NULL;
            size_t block_byte_count = 0u;
            size_t function_start = 0u;
            size_t byte_offset;

            memset(&block_summary, 0, sizeof(block_summary));
            if (!machine_bytes_function_get_block(function, block_index, &block) || !block ||
                !machine_bytes_block_get_summary(block, &block_summary) ||
                !machine_bytes_block_get_bytes(block, &block_bytes, &block_byte_count) || !block_bytes ||
                !machine_bytes_report_get_function_byte_span(&bytes_report, function_index, &function_start, NULL)) {
                compiler_set_error(error, 0, 0, "COMPILER-118: malformed bytes block surface");
                ok = 0;
                goto cleanup;
            }
            if ((block_byte_count % 4u) != 0u) {
                compiler_set_error(error, 0, 0, "COMPILER-119: riscv preview block is not 4-byte aligned");
                ok = 0;
                goto cleanup;
            }
            if (block_summary.label_name && block_summary.label_name[0] != '\0') {
                if (!compiler_append_pretty_symbol_name(&builder, &bytes_report, block_summary.label_name) ||
                    !compiler_builder_appendf(&builder, ":\n")) {
                    compiler_set_error(error, 0, 0, "COMPILER-120: out of memory writing block label");
                    ok = 0;
                    goto cleanup;
                }
            }
            for (byte_offset = 0u; byte_offset < block_byte_count; byte_offset += 4u) {
                uint32_t word = compiler_read_u32_le(block_bytes + byte_offset);
                size_t program_byte_offset = function_start + block_summary.start_byte_offset + byte_offset;

                if (!compiler_append_riscv_preview_instruction(&builder, &bytes_report, program_byte_offset, word)) {
                    compiler_set_error(error, 0, 0, "COMPILER-121: out of memory writing riscv instruction");
                    ok = 0;
                    goto cleanup;
                }
            }
        }

        if (!compiler_builder_appendf(&builder, ".size %s, .-%s\n\n", function_name, function_name)) {
            compiler_set_error(error, 0, 0, "COMPILER-122: out of memory separating functions");
            ok = 0;
            goto cleanup;
        }
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

int compiler_mode_from_flag(const char *flag, CompilerMode *out_mode) {
    if (!flag || !out_mode) {
        return 0;
    }
    if (strcmp(flag, "-riscv") == 0) {
        *out_mode = COMPILER_MODE_RISCV;
        return 1;
    }
    if (strcmp(flag, "-perf") == 0) {
        *out_mode = COMPILER_MODE_PERF;
        return 1;
    }
    return 0;
}

int compiler_compile_source_text(const char *source,
    CompilerMode mode,
    char **out_text,
    CompilerError *error) {
    TokenArray tokens;
    AstProgram ast_program;
    IrProgram ir_program;
    LowerIrProgram lower_program;
    ValueSsaProgram value_program;
    MachineIrAllocateRewriteReport machine_report;
    ParserError parser_error;
    SemanticError semantic_error;
    IrError ir_error;
    LowerIrError lower_error;
    ValueSsaError value_error;
    MachineIrError machine_error;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        compiler_set_error(error, 0, 0, "COMPILER-100: invalid compile contract");
        return 0;
    }

    memset(&parser_error, 0, sizeof(parser_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_error, 0, sizeof(machine_error));
    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    value_ssa_program_init(&value_program);
    machine_ir_allocate_rewrite_report_init(&machine_report);

    ok = lexer_tokenize(source, &tokens);
    if (!ok) {
        compiler_set_error(error, 0, 0, "COMPILER-108: lexical analysis failed");
        goto cleanup;
    }
    ok = parser_parse_translation_unit_ast(&tokens, &ast_program, &parser_error);
    if (!ok) {
        compiler_copy_stage_error(error, parser_error.line, parser_error.column, parser_error.message);
        goto cleanup;
    }
    ok = semantic_analyze_program(&ast_program, &semantic_error);
    if (!ok) {
        compiler_copy_stage_error(error, semantic_error.line, semantic_error.column, semantic_error.message);
        goto cleanup;
    }
    ok = ir_lower_program(&ast_program, &ir_program, &ir_error);
    if (!ok) {
        compiler_copy_stage_error(error, ir_error.line, ir_error.column, ir_error.message);
        goto cleanup;
    }
    ok = lower_ir_lower_from_ir(&ir_program, &lower_program, &lower_error);
    if (!ok) {
        compiler_copy_stage_error(error, lower_error.line, lower_error.column, lower_error.message);
        goto cleanup;
    }
    ok = value_ssa_build_default_from_lower_ir(&lower_program, &value_program, &value_error);
    if (!ok) {
        compiler_copy_stage_error(error, value_error.line, value_error.column, value_error.message);
        goto cleanup;
    }
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
        &value_program,
        COMPILER_DEFAULT_COLOR_BUDGET,
        COMPILER_DEFAULT_MACHINE_REGISTER_COUNT,
        &machine_report,
        &machine_error);
    if (!ok) {
        compiler_copy_stage_error(error, machine_error.line, machine_error.column, machine_error.message);
        goto cleanup;
    }
    ok = compiler_emit_riscv_preview_text_from_report(&machine_report, mode, out_text, error);
    if (!ok) {
        goto cleanup;
    }

cleanup:
    machine_ir_allocate_rewrite_report_free(&machine_report);
    value_ssa_program_free(&value_program);
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

int compiler_compile_file(const char *input_path,
    CompilerMode mode,
    char **out_text,
    CompilerError *error) {
    char *source_text;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    source_text = compiler_read_file_text(input_path, error);
    if (!source_text) {
        return 0;
    }
    ok = compiler_compile_source_text(source_text, mode, out_text, error);
    free(source_text);
    return ok;
}
