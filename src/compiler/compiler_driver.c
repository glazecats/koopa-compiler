#define _POSIX_C_SOURCE 200809L

#include "compiler.h"
#include "compiler_backend.h"

#include "ast.h"
#include "ir.h"
#include "lexer.h"
#include "lower_ir.h"
#include "machine/bytes.h"
#include "machine/emit.h"
#include "machine/layout.h"
#include "machine/ir.h"
#include "machine/select.h"
#include "parser.h"
#include "semantic.h"
#include "value_ssa.h"
#include "value_ssa_pass.h"
#include "memory_ssa_pass.h"

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

typedef struct {
    size_t program_byte_offset;
    const char *function_name;
    size_t block_index;
} CompilerBlockLabelCacheEntry;

typedef struct {
    const MachineBytesFixupSummary **fixups_by_patch;
    const MachineBytesFixupSummary **fixups_by_owner;
    size_t fixup_count;
    CompilerBlockLabelCacheEntry *block_labels;
    size_t block_label_count;
} CompilerRiscvPreviewCache;

typedef struct {
    const char *name;
    char *value;
    int had_value;
} CompilerSavedEnv;

static int compiler_env_flag_enabled(const char *name, int default_value) {
    const char *flag = NULL;

    if (!name || name[0] == '\0') {
        return default_value;
    }
    flag = getenv(name);
    if (!flag || flag[0] == '\0') {
        return default_value;
    }
    return strcmp(flag, "0") != 0;
}

static int compiler_aggressive_opt_mode_enabled(void) {
    /*
     * Keep the historical default on the main path, but let narrower toggles
     * override individual downstream behaviors for isolation experiments.
     */
    return compiler_env_flag_enabled("COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS", 1);
}

static int compiler_use_small_data_sections_enabled(void) {
    return compiler_env_flag_enabled(
        "COMPILER_USE_SMALL_DATA_SECTIONS",
        compiler_aggressive_opt_mode_enabled());
}

static int compiler_use_default_ssa_build_enabled(void) {
    return compiler_env_flag_enabled(
        "COMPILER_USE_DEFAULT_SSA_BUILD",
        compiler_aggressive_opt_mode_enabled());
}

static int compiler_use_perf_hotspots_enabled(void) {
    return compiler_env_flag_enabled(
        "COMPILER_USE_PERF_HOTSPOTS",
        compiler_aggressive_opt_mode_enabled());
}

static int compiler_use_caller_save_text_enabled(void) {
    return compiler_env_flag_enabled(
        "COMPILER_USE_CALLER_SAVE_TEXT",
        !compiler_aggressive_opt_mode_enabled());
}

static int compiler_use_final_text_peepholes_enabled(void) {
    return compiler_env_flag_enabled(
        "COMPILER_USE_FINAL_TEXT_PEEPHOLES",
        compiler_aggressive_opt_mode_enabled());
}

static int compiler_use_simple_backend_enabled(void) {
    const char *explicit_enable = getenv("COMPILER_SIMPLE_BACKEND");

    if (explicit_enable && explicit_enable[0] != '\0') {
        return strcmp(explicit_enable, "0") != 0;
    }
    return 0;
}

static int compiler_use_direct_simple_text_enabled(void) {
    return compiler_env_flag_enabled("COMPILER_USE_DIRECT_SIMPLE_TEXT", 0);
}

static int compiler_use_runtime_startup_stub_enabled(void) {
    return compiler_env_flag_enabled("COMPILER_USE_RUNTIME_STARTUP_STUB", 1);
}

static void compiler_saved_env_init(CompilerSavedEnv *slot, const char *name) {
    if (!slot) {
        return;
    }
    slot->name = name;
    slot->value = NULL;
    slot->had_value = 0;
}

static int compiler_save_env(CompilerSavedEnv *slot) {
    const char *value = NULL;

    if (!slot || !slot->name) {
        return 0;
    }
    value = getenv(slot->name);
    if (!value) {
        slot->had_value = 0;
        return 1;
    }
    slot->value = strdup(value);
    if (!slot->value) {
        return 0;
    }
    slot->had_value = 1;
    return 1;
}

static void compiler_restore_env(CompilerSavedEnv *slot) {
    if (!slot || !slot->name) {
        return;
    }
    if (slot->had_value) {
        setenv(slot->name, slot->value ? slot->value : "", 1);
    } else {
        unsetenv(slot->name);
    }
    free(slot->value);
    slot->value = NULL;
    slot->had_value = 0;
}

static int compiler_apply_simple_backend_profile(void) {
    return setenv("COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS", "0", 1) == 0 &&
        setenv("COMPILER_USE_CALLER_SAVE_TEXT", "0", 1) == 0 &&
        setenv("COMPILER_USE_FINAL_TEXT_PEEPHOLES", "0", 1) == 0 &&
        setenv("COMPILER_USE_SMALL_DATA_SECTIONS", "0", 1) == 0 &&
        setenv("COMPILER_USE_DEFAULT_SSA_BUILD", "0", 1) == 0 &&
        setenv("COMPILER_USE_PERF_HOTSPOTS", "0", 1) == 0 &&
        setenv("MACHINE_SELECT_SKIP_REUSE_ADDR_ROOTS", "1", 1) == 0 &&
        setenv("MACHINE_SELECT_SKIP_CLEANUP_PURE", "1", 1) == 0 &&
        setenv("MACHINE_SELECT_SKIP_REUSE_INTERNAL_PURE_CALLS", "1", 1) == 0 &&
        setenv("MACHINE_SELECT_SKIP_REUSE_UNIQUE_PREDECESSOR_PURE_CALLS", "1", 1) == 0 &&
        setenv("MACHINE_SELECT_SKIP_FORWARD_SAME_BLOCK_INDIRECT_LOADS", "1", 1) == 0 &&
        setenv("MACHINE_SELECT_SKIP_REUSE_SPILL_PURE_EXPR", "1", 1) == 0 &&
        setenv("MACHINE_SELECT_SKIP_FULL_CLEANUP", "1", 1) == 0;
}

static size_t compiler_preview_caller_save_area_size(void) {
    return 15u * 4u;
}

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
static void compiler_riscv_preview_cache_init(CompilerRiscvPreviewCache *cache);
static void compiler_riscv_preview_cache_free(CompilerRiscvPreviewCache *cache);
static int compiler_riscv_preview_cache_build(
    const MachineBytesReport *report,
    CompilerRiscvPreviewCache *cache);
static const char *compiler_find_global_name_for_gp_offset(const MachineBytesProgram *program, int32_t byte_offset);
static int compiler_emit_global_sections(
    CompilerStringBuilder *builder,
    const MachineBytesProgram *program);
static const MachineBytesFixupSummary *compiler_find_fixup_at_patch_offset_cached(
    const CompilerRiscvPreviewCache *cache,
    size_t patch_byte_offset);
static const MachineBytesFixupSummary *compiler_find_fixup_at_patch_offset_and_kind_cached(
    const CompilerRiscvPreviewCache *cache,
    size_t patch_byte_offset,
    MachineBytesFixupKind kind);
static const MachineBytesFixupSummary *compiler_find_call_fixup_covering_offset_cached(
    const CompilerRiscvPreviewCache *cache,
    size_t program_byte_offset);
static int compiler_call_requires_a0_preservation(
    const MachineBytesReport *report,
    size_t function_index,
    size_t program_byte_offset);
static const char *compiler_find_label_at_program_byte_offset(
    const MachineBytesReport *report,
    size_t program_byte_offset);
static const CompilerBlockLabelCacheEntry *compiler_find_block_label_at_program_byte_offset_cached(
    const CompilerRiscvPreviewCache *cache,
    size_t program_byte_offset);
static int compiler_append_stack_adjust(
    CompilerStringBuilder *builder,
    long long delta);
static int compiler_append_stack_access(
    CompilerStringBuilder *builder,
    const char *op,
    const char *reg,
    const char *base,
    size_t offset,
    const char *scratch);
static int compiler_riscv_is_stack_load_word(uint32_t word, uint32_t rd, int32_t offset);
static int compiler_riscv_is_stack_adjust_word(uint32_t word, size_t stack_bytes);
static int compiler_riscv_is_ret_word(uint32_t word);
static int compiler_riscv_preview_detect_tail_call_epilogue(
    const unsigned char *block_bytes,
    size_t block_byte_count,
    size_t byte_offset,
    uint32_t word,
    size_t epilogue_stack_size,
    size_t epilogue_ra_offset,
    int epilogue_restores_ra,
    size_t *skip_byte_count);
static int compiler_append_riscv_preview_tail_call(
    CompilerStringBuilder *builder,
    const MachineBytesReport *report,
    const char *target_name,
    const CompilerBlockLabelCacheEntry *cached_label,
    size_t epilogue_stack_size,
    size_t epilogue_ra_offset);
static int compiler_riscv_preview_line_is_jal_ra(const char *line, char *target, size_t target_size);
static int compiler_riscv_preview_line_is_restore_from_s11(const char *line);
static int compiler_riscv_preview_line_is_restore_from_sp(
    const char *line,
    const char *reg_name,
    int *out_offset);
static int compiler_riscv_preview_line_is_addi_sp(const char *line, int *out_delta);
static int compiler_riscv_preview_line_is_ret(const char *line);
static int compiler_optimize_riscv_preview_tail_calls(char **io_text);
static int compiler_optimize_riscv_preview_mul_by_four(char **io_text);
static int compiler_optimize_riscv_preview_sub_by_one(char **io_text);
static int compiler_optimize_riscv_preview_reuse_repeated_lui_addi_constants(char **io_text);
static int compiler_riscv_preview_reg_is_call_clobbered(const char *reg_name);
static int compiler_riscv_preview_line_is_call_like(const char *line);
static int compiler_riscv_preview_line_defines_reg(const char *line, const char *reg_name);
static int compiler_append_caller_save_sequence(
    CompilerStringBuilder *builder,
    size_t call_save_area_offset,
    int is_store,
    int include_a0);
static int compiler_append_riscv_preview_instruction(
    CompilerStringBuilder *builder,
    const MachineBytesReport *report,
    const CompilerRiscvPreviewCache *cache,
    size_t program_byte_offset,
    uint32_t word,
    size_t epilogue_stack_size,
    size_t epilogue_ra_offset,
    int epilogue_restores_ra,
    size_t call_save_area_offset,
    int save_caller_regs_around_call);
static MachineBytesTargetProfile compiler_backend_profile_for_mode(CompilerMode mode);
int compiler_emit_riscv_preview_text_from_report(const MachineIrAllocateRewriteReport *report,
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

static void compiler_riscv_preview_cache_init(CompilerRiscvPreviewCache *cache) {
    if (!cache) {
        return;
    }
    cache->fixups_by_patch = NULL;
    cache->fixups_by_owner = NULL;
    cache->fixup_count = 0u;
    cache->block_labels = NULL;
    cache->block_label_count = 0u;
}

static void compiler_riscv_preview_cache_free(CompilerRiscvPreviewCache *cache) {
    if (!cache) {
        return;
    }
    free(cache->fixups_by_patch);
    free(cache->fixups_by_owner);
    free(cache->block_labels);
    compiler_riscv_preview_cache_init(cache);
}

static int compiler_compare_fixup_patch_ptrs(const void *lhs, const void *rhs) {
    const MachineBytesFixupSummary *const *left = (const MachineBytesFixupSummary *const *)lhs;
    const MachineBytesFixupSummary *const *right = (const MachineBytesFixupSummary *const *)rhs;

    if ((*left)->patch_byte_offset < (*right)->patch_byte_offset) {
        return -1;
    }
    if ((*left)->patch_byte_offset > (*right)->patch_byte_offset) {
        return 1;
    }
    return 0;
}

static int compiler_is_call_restore_point(
    const MachineBytesFixupSummary *call_fixup,
    size_t program_byte_offset,
    int save_caller_regs_around_call) {
    if (!call_fixup) {
        return 0;
    }
    /*
     * In the normal pretty path we collapse preview `auipc+jalr` into a
     * single `call` pseudo-instruction, so restoring at the patch offset is
     * correct. When caller-save text mode is enabled we deliberately keep the
     * explicit pair, so restoring at the patch offset would land between
     * `auipc` and `jalr` and clobber call arguments before the jump happens.
     */
    if (save_caller_regs_around_call &&
        call_fixup->has_target_function_index) {
        return program_byte_offset == call_fixup->patch_byte_offset + 4u;
    }
    return program_byte_offset == call_fixup->patch_byte_offset;
}

static int compiler_should_collapse_call_fixup(
    const MachineBytesReport *report,
    const MachineBytesFixupSummary *fixup,
    int save_caller_regs_around_call) {
    const MachineBytesFunction *target_function = NULL;
    const char *target_function_name = NULL;
    int has_body = 0;
    size_t parameter_count = 0u;
    size_t local_count = 0u;
    size_t block_count = 0u;
    size_t spill_slot_count = 0u;

    if (!fixup || !fixup->target_name || fixup->target_name[0] == '\0') {
        return 0;
    }
    /*
     * Caller-save text mode needs explicit `auipc+jalr` only for internal
     * calls where we already know the resolved in-program target. External
     * helper calls still need the symbolic `call foo` surface so the assembler
     * can attach the correct relocation instead of seeing `jalr ra, 0(ra)`.
     */
    if (save_caller_regs_around_call && fixup->has_target_function_index) {
        if (!report ||
            !machine_bytes_report_get_function(report, fixup->target_function_index, &target_function) ||
            !target_function ||
            !machine_bytes_function_get_summary(
                target_function,
                &target_function_name,
                &has_body,
                &parameter_count,
                &local_count,
                &block_count,
                &spill_slot_count) ||
            !has_body) {
            return 1;
        }
        return 0;
    }
    return 1;
}

static int compiler_compare_fixup_owner_ptrs(const void *lhs, const void *rhs) {
    const MachineBytesFixupSummary *const *left = (const MachineBytesFixupSummary *const *)lhs;
    const MachineBytesFixupSummary *const *right = (const MachineBytesFixupSummary *const *)rhs;

    if ((*left)->owner_byte_offset < (*right)->owner_byte_offset) {
        return -1;
    }
    if ((*left)->owner_byte_offset > (*right)->owner_byte_offset) {
        return 1;
    }
    if ((*left)->patch_byte_offset < (*right)->patch_byte_offset) {
        return -1;
    }
    if ((*left)->patch_byte_offset > (*right)->patch_byte_offset) {
        return 1;
    }
    return 0;
}

static int compiler_compare_block_label_offsets(const void *lhs, const void *rhs) {
    const CompilerBlockLabelCacheEntry *left = (const CompilerBlockLabelCacheEntry *)lhs;
    const CompilerBlockLabelCacheEntry *right = (const CompilerBlockLabelCacheEntry *)rhs;

    if (left->program_byte_offset < right->program_byte_offset) {
        return -1;
    }
    if (left->program_byte_offset > right->program_byte_offset) {
        return 1;
    }
    return 0;
}

static int compiler_riscv_preview_cache_build(
    const MachineBytesReport *report,
    CompilerRiscvPreviewCache *cache) {
    size_t fixup_count = 0u;
    size_t function_count = 0u;
    size_t total_block_summary_count = 0u;
    size_t function_index;
    size_t fixup_index;
    size_t block_label_write_index = 0u;

    if (!report || !cache) {
        return 0;
    }

    compiler_riscv_preview_cache_free(cache);
    if (!machine_bytes_report_get_fixup_summary_count(report, &fixup_count) ||
        !machine_bytes_report_get_summary(report, NULL, NULL, &function_count, &total_block_summary_count)) {
        return 0;
    }

    if (fixup_count > 0u) {
        cache->fixups_by_patch =
            (const MachineBytesFixupSummary **)malloc(fixup_count * sizeof(MachineBytesFixupSummary *));
        cache->fixups_by_owner =
            (const MachineBytesFixupSummary **)malloc(fixup_count * sizeof(MachineBytesFixupSummary *));
        if (!cache->fixups_by_patch || !cache->fixups_by_owner) {
            compiler_riscv_preview_cache_free(cache);
            return 0;
        }
        cache->fixup_count = fixup_count;
        for (fixup_index = 0u; fixup_index < fixup_count; ++fixup_index) {
            const MachineBytesFixupSummary *fixup = NULL;

            if (!machine_bytes_report_get_fixup_summary(report, fixup_index, &fixup) || !fixup) {
                compiler_riscv_preview_cache_free(cache);
                return 0;
            }
            cache->fixups_by_patch[fixup_index] = fixup;
            cache->fixups_by_owner[fixup_index] = fixup;
        }
        qsort(cache->fixups_by_patch,
            cache->fixup_count,
            sizeof(MachineBytesFixupSummary *),
            compiler_compare_fixup_patch_ptrs);
        qsort(cache->fixups_by_owner,
            cache->fixup_count,
            sizeof(MachineBytesFixupSummary *),
            compiler_compare_fixup_owner_ptrs);
    }

    if (total_block_summary_count > 0u) {
        cache->block_labels =
            (CompilerBlockLabelCacheEntry *)calloc(total_block_summary_count, sizeof(CompilerBlockLabelCacheEntry));
        if (!cache->block_labels) {
            compiler_riscv_preview_cache_free(cache);
            return 0;
        }
    }

    for (function_index = 0u; function_index < function_count; ++function_index) {
        const MachineBytesFunction *function = NULL;
        const char *function_name = NULL;
        int has_body = 0;
        size_t parameter_count = 0u;
        size_t local_count = 0u;
        size_t block_count = 0u;
        size_t spill_slot_count = 0u;
        size_t function_start = 0u;
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
            !machine_bytes_report_get_function_byte_span(report, function_index, &function_start, NULL)) {
            compiler_riscv_preview_cache_free(cache);
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
                !block_summary || !block_summary->label_name || block_summary->label_name[0] == '\0') {
                continue;
            }
            cache->block_labels[block_label_write_index].program_byte_offset =
                function_start + block_summary->start_byte_offset;
            cache->block_labels[block_label_write_index].function_name = function_name;
            cache->block_labels[block_label_write_index].block_index = block_index;
            ++block_label_write_index;
        }
    }

    cache->block_label_count = block_label_write_index;
    if (cache->block_label_count > 1u) {
        qsort(cache->block_labels,
            cache->block_label_count,
            sizeof(CompilerBlockLabelCacheEntry),
            compiler_compare_block_label_offsets);
    }
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
    size_t running_offset = 0u;

    if (!program || byte_offset < 0 || (byte_offset % 4) != 0) {
        return NULL;
    }
    for (global_index = 0u; program->globals && global_index < program->global_count; ++global_index) {
        const MachineEmitGlobal *global = &program->globals[global_index];
        size_t byte_size = global->byte_size ? global->byte_size : 4u;

        if (byte_offset >= (int32_t)running_offset && byte_offset < (int32_t)(running_offset + byte_size) &&
            global->name && global->name[0] != '\0') {
            return global->name;
        }
        running_offset += byte_size;
    }
    return NULL;
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
        if (!compiler_builder_appendf(
                builder,
                compiler_use_small_data_sections_enabled()
                    ? ".section .sbss,\"aw\",@nobits\n"
                    : ".section .bss,\"aw\",@nobits\n")) {
            return 0;
        }
        for (global_index = 0u; global_index < program->global_count; ++global_index) {
            const MachineEmitGlobal *global = &program->globals[global_index];

            if (!global->name || global->name[0] == '\0' || global->has_initializer) {
                continue;
            }
        if (!compiler_builder_appendf(
                    builder,
                    ".globl %s\n.type %s, @object\n.size %s, %zu\n.p2align 2\n%s:\n  .zero %zu\n",
                    global->name,
                    global->name,
                    global->name,
                    global->byte_size ? global->byte_size : 4u,
                    global->name,
                    global->byte_size ? global->byte_size : 4u)) {
                return 0;
            }
        }
        if (!compiler_builder_appendf(builder, "\n")) {
            return 0;
        }
    }

    if (has_data) {
        if (!compiler_builder_appendf(
                builder,
                compiler_use_small_data_sections_enabled()
                    ? ".section .sdata,\"aw\",@progbits\n"
                    : ".section .data,\"aw\",@progbits\n")) {
            return 0;
        }
        for (global_index = 0u; global_index < program->global_count; ++global_index) {
            const MachineEmitGlobal *global = &program->globals[global_index];
            size_t byte_size;

            if (!global->name || global->name[0] == '\0' || !global->has_initializer) {
                continue;
            }
            byte_size = global->byte_size ? global->byte_size : 4u;
            if (!compiler_builder_appendf(
                    builder,
                    ".globl %s\n.type %s, @object\n.size %s, %zu\n.p2align 2\n%s:\n  .word %lld\n",
                    global->name,
                    global->name,
                    global->name,
                    byte_size,
                    global->name,
                    global->initializer_value)) {
                return 0;
            }
            if (byte_size > 4u &&
                !compiler_builder_appendf(builder, "  .zero %zu\n", byte_size - 4u)) {
                return 0;
            }
        }
        if (!compiler_builder_appendf(builder, "\n")) {
            return 0;
        }
    }

    return 1;
}

static const MachineBytesFixupSummary *compiler_find_fixup_at_patch_offset_cached(
    const CompilerRiscvPreviewCache *cache,
    size_t patch_byte_offset) {
    size_t lo = 0u;
    size_t hi;

    if (!cache || !cache->fixups_by_patch || cache->fixup_count == 0u) {
        return NULL;
    }

    hi = cache->fixup_count;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) / 2u);
        const MachineBytesFixupSummary *fixup = cache->fixups_by_patch[mid];

        if (fixup->patch_byte_offset < patch_byte_offset) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    if (lo < cache->fixup_count && cache->fixups_by_patch[lo]->patch_byte_offset == patch_byte_offset) {
        return cache->fixups_by_patch[lo];
    }
    return NULL;
}

static const MachineBytesFixupSummary *compiler_find_fixup_at_patch_offset_and_kind_cached(
    const CompilerRiscvPreviewCache *cache,
    size_t patch_byte_offset,
    MachineBytesFixupKind kind) {
    size_t lo = 0u;
    size_t hi;
    size_t index;

    if (!cache || !cache->fixups_by_patch || cache->fixup_count == 0u) {
        return NULL;
    }

    hi = cache->fixup_count;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) / 2u);
        const MachineBytesFixupSummary *mid_fixup = cache->fixups_by_patch[mid];

        if (mid_fixup->patch_byte_offset < patch_byte_offset) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    for (index = lo; index < cache->fixup_count; ++index) {
        const MachineBytesFixupSummary *candidate = cache->fixups_by_patch[index];

        if (candidate->patch_byte_offset != patch_byte_offset) {
            break;
        }
        if (candidate->kind == kind) {
            return candidate;
        }
    }
    return NULL;
}

static const MachineBytesFixupSummary *compiler_find_call_fixup_covering_offset_cached(
    const CompilerRiscvPreviewCache *cache,
    size_t program_byte_offset) {
    size_t lo = 0u;
    size_t hi;

    if (!cache || !cache->fixups_by_owner || cache->fixup_count == 0u) {
        return NULL;
    }

    hi = cache->fixup_count;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) / 2u);
        const MachineBytesFixupSummary *fixup = cache->fixups_by_owner[mid];

        if (fixup->owner_byte_offset <= program_byte_offset) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }

    while (lo > 0u) {
        const MachineBytesFixupSummary *fixup = cache->fixups_by_owner[lo - 1u];

        if (fixup->owner_byte_offset > program_byte_offset) {
            --lo;
            continue;
        }
        if (fixup->kind == MACHINE_BYTES_FIXUP_CALL_TARGET &&
            program_byte_offset >= fixup->owner_byte_offset &&
            program_byte_offset < fixup->owner_byte_offset + fixup->owner_byte_count) {
            return fixup;
        }
        if (program_byte_offset >= fixup->owner_byte_offset + fixup->owner_byte_count) {
            break;
        }
        --lo;
    }

    return NULL;
}

static int compiler_call_requires_a0_preservation(
    const MachineBytesReport *report,
    size_t function_index,
    size_t program_byte_offset) {
    const MachineBytesReferenceSummary *references = NULL;
    size_t reference_count = 0u;
    size_t reference_index;

    if (!report ||
        !machine_bytes_report_get_function_reference_summaries(
            report, function_index, &reference_count, &references) ||
        !references) {
        return 0;
    }

    for (reference_index = 0u; reference_index < reference_count; ++reference_index) {
        const MachineBytesReferenceSummary *reference = &references[reference_index];

        if (reference->kind != MACHINE_BYTES_REFERENCE_CALL ||
            reference->owner_byte_offset != program_byte_offset) {
            continue;
        }
        return reference->op_kind == MACHINE_SELECT_OP_CALL_VOID ||
            reference->op_kind == MACHINE_SELECT_OP_CALL_VOID_IMM;
    }
    return 0;
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

static const CompilerBlockLabelCacheEntry *compiler_find_block_label_at_program_byte_offset_cached(
    const CompilerRiscvPreviewCache *cache,
    size_t program_byte_offset) {
    size_t lo = 0u;
    size_t hi;

    if (!cache || !cache->block_labels || cache->block_label_count == 0u) {
        return NULL;
    }

    hi = cache->block_label_count;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) / 2u);
        const CompilerBlockLabelCacheEntry *entry = &cache->block_labels[mid];

        if (entry->program_byte_offset < program_byte_offset) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    if (lo < cache->block_label_count &&
        cache->block_labels[lo].program_byte_offset == program_byte_offset) {
        return &cache->block_labels[lo];
    }
    return NULL;
}

static int compiler_append_stack_adjust(
    CompilerStringBuilder *builder,
    long long delta) {
    if (!builder) {
        return 0;
    }
    if (delta >= -2048 && delta <= 2047) {
        return compiler_builder_appendf(builder, "  addi sp, sp, %lld\n", delta);
    }
    return compiler_builder_appendf(
        builder,
        "  li t6, %lld\n"
        "  add sp, sp, t6\n",
        delta);
}

static int compiler_append_stack_access(
    CompilerStringBuilder *builder,
    const char *op,
    const char *reg,
    const char *base,
    size_t offset,
    const char *scratch) {
    if (!builder || !op || !reg || !base || !scratch) {
        return 0;
    }
    if (strcmp(base, "s11") == 0 && offset > 2047u) {
        const char *addr_reg = strcmp(reg, "s11") == 0 ? "sp" : reg;

        return compiler_builder_appendf(
            builder,
            "  mv %s, sp\n"
            "  li sp, %zu\n"
            "  add sp, s11, sp\n"
            "  %s %s, 0(sp)\n"
            "%s"
            "  mv sp, %s\n",
            scratch,
            offset,
            op,
            addr_reg,
            strcmp(reg, "s11") == 0 ? "  mv s11, sp\n" : "",
            scratch);
    }
    if (offset <= 2047u) {
        return compiler_builder_appendf(builder, "  %s %s, %zu(%s)\n", op, reg, offset, base);
    }
    return compiler_builder_appendf(
        builder,
        "  li %s, %zu\n"
        "  add %s, %s, %s\n"
        "  %s %s, 0(%s)\n",
        scratch,
        offset,
        scratch,
        base,
        scratch,
        op,
        reg,
        scratch);
}

static int compiler_riscv_is_stack_load_word(uint32_t word, uint32_t rd, int32_t offset) {
    return (word & 0x7Fu) == 0x03u &&
        ((word >> 12u) & 0x7u) == 0x2u &&
        ((word >> 7u) & 0x1Fu) == rd &&
        ((word >> 15u) & 0x1Fu) == 2u &&
        compiler_riscv_decode_i_imm(word) == offset;
}

static int compiler_riscv_is_stack_adjust_word(uint32_t word, size_t stack_bytes) {
    return (word & 0x7Fu) == 0x13u &&
        ((word >> 12u) & 0x7u) == 0x0u &&
        ((word >> 7u) & 0x1Fu) == 2u &&
        ((word >> 15u) & 0x1Fu) == 2u &&
        compiler_riscv_decode_i_imm(word) == (int32_t)stack_bytes;
}

static int compiler_riscv_is_ret_word(uint32_t word) {
    return (word & 0x7Fu) == 0x67u &&
        ((word >> 12u) & 0x7u) == 0x0u &&
        ((word >> 7u) & 0x1Fu) == 0u &&
        ((word >> 15u) & 0x1Fu) == 1u &&
        compiler_riscv_decode_i_imm(word) == 0;
}

static int compiler_riscv_preview_detect_tail_call_epilogue(
    const unsigned char *block_bytes,
    size_t block_byte_count,
    size_t byte_offset,
    uint32_t word,
    size_t epilogue_stack_size,
    size_t epilogue_ra_offset,
    int epilogue_restores_ra,
    size_t *skip_byte_count) {
    uint32_t load_s11_word;
    uint32_t load_ra_word;
    uint32_t adjust_sp_word;
    uint32_t ret_word;

    if (!block_bytes || !skip_byte_count) {
        return 0;
    }
    if ((word & 0x7Fu) != 0x6Fu || ((word >> 7u) & 0x1Fu) != 1u) {
        return 0;
    }
    if (!epilogue_restores_ra || epilogue_stack_size == 0u || epilogue_ra_offset < 4u) {
        return 0;
    }
    if (byte_offset + 20u > block_byte_count) {
        return 0;
    }

    load_s11_word = compiler_read_u32_le(block_bytes + byte_offset + 4u);
    load_ra_word = compiler_read_u32_le(block_bytes + byte_offset + 8u);
    adjust_sp_word = compiler_read_u32_le(block_bytes + byte_offset + 12u);
    ret_word = compiler_read_u32_le(block_bytes + byte_offset + 16u);

    if (!compiler_riscv_is_stack_load_word(load_s11_word, 27u, (int32_t)(epilogue_ra_offset - 4u)) ||
        !compiler_riscv_is_stack_load_word(load_ra_word, 1u, (int32_t)epilogue_ra_offset) ||
        !compiler_riscv_is_stack_adjust_word(adjust_sp_word, epilogue_stack_size) ||
        !compiler_riscv_is_ret_word(ret_word)) {
        return 0;
    }

    *skip_byte_count = 20u;
    return 1;
}

static int compiler_append_riscv_preview_tail_call(
    CompilerStringBuilder *builder,
    const MachineBytesReport *report,
    const char *target_name,
    const CompilerBlockLabelCacheEntry *cached_label,
    size_t epilogue_stack_size,
    size_t epilogue_ra_offset) {
    if (!builder || !report ||
        ((!target_name || target_name[0] == '\0') && !cached_label) ||
        epilogue_stack_size == 0u || epilogue_ra_offset < 4u) {
        return 0;
    }
    return compiler_append_stack_access(builder, "lw", "s11", "sp", epilogue_ra_offset - 4u, "t6") &&
        compiler_append_stack_access(builder, "lw", "ra", "sp", epilogue_ra_offset, "t6") &&
        compiler_append_stack_adjust(builder, (long long)epilogue_stack_size) &&
        compiler_builder_appendf(builder, "  j ") &&
        ((cached_label &&
            compiler_builder_appendf(
                builder, ".L%s_%zu", cached_label->function_name, cached_label->block_index)) ||
            compiler_append_pretty_symbol_name(builder, report, target_name)) &&
        compiler_builder_appendf(builder, "\n");
}

static int compiler_riscv_preview_line_is_jal_ra(const char *line, char *target, size_t target_size) {
    if (!line || !target || target_size == 0u) {
        return 0;
    }
    target[0] = '\0';
    return sscanf(line, "  jal ra, %255s", target) == 1;
}

static int compiler_riscv_preview_line_is_restore_from_s11(const char *line) {
    char reg_name[32];
    int offset = 0;

    if (!line) {
        return 0;
    }
    if (sscanf(line, "  lw %31[^,], %d(s11)", reg_name, &offset) != 2) {
        return 0;
    }
    return strcmp(reg_name, "s11") != 0;
}

static int compiler_riscv_preview_line_is_restore_from_sp(
    const char *line,
    const char *reg_name,
    int *out_offset) {
    char actual_reg[32];
    int offset = 0;

    if (!line || !reg_name) {
        return 0;
    }
    if (sscanf(line, "  lw %31[^,], %d(sp)", actual_reg, &offset) != 2) {
        return 0;
    }
    if (strcmp(actual_reg, reg_name) != 0) {
        return 0;
    }
    if (out_offset) {
        *out_offset = offset;
    }
    return 1;
}

static int compiler_riscv_preview_line_is_addi_sp(const char *line, int *out_delta) {
    int delta = 0;

    if (!line) {
        return 0;
    }
    if (sscanf(line, "  addi sp, sp, %d", &delta) != 1) {
        return 0;
    }
    if (out_delta) {
        *out_delta = delta;
    }
    return 1;
}

static int compiler_riscv_preview_line_is_ret(const char *line) {
    return line && strcmp(line, "  ret") == 0;
}

static int compiler_riscv_preview_line_loads_immediate(
    const char *line,
    char *reg_name,
    size_t reg_name_size,
    int *out_imm) {
    int imm = 0;

    if (out_imm) {
        *out_imm = 0;
    }
    if (!line || !reg_name || reg_name_size == 0u) {
        return 0;
    }
    reg_name[0] = '\0';
    if (sscanf(line, "  li %31[^,], %d", reg_name, &imm) != 2) {
        reg_name[0] = '\0';
        if (sscanf(line, "  addi %31[^,], zero, %d", reg_name, &imm) != 2) {
            return 0;
        }
    }
    if (out_imm) {
        *out_imm = imm;
    }
    return 1;
}

static int compiler_riscv_preview_line_is_li_zero(const char *line, char *reg_name, size_t reg_name_size) {
    int imm = 1;

    if (!compiler_riscv_preview_line_loads_immediate(line, reg_name, reg_name_size, &imm)) {
        return 0;
    }
    return imm == 0;
}

static int compiler_riscv_preview_line_is_li_four(const char *line, char *reg_name, size_t reg_name_size) {
    int imm = 0;

    if (!compiler_riscv_preview_line_loads_immediate(line, reg_name, reg_name_size, &imm)) {
        return 0;
    }
    return imm == 4;
}

static int compiler_riscv_preview_line_is_li_one(const char *line, char *reg_name, size_t reg_name_size) {
    int imm = 0;

    if (!compiler_riscv_preview_line_loads_immediate(line, reg_name, reg_name_size, &imm)) {
        return 0;
    }
    return imm == 1;
}

static int compiler_riscv_preview_line_is_addi_sp_imm(
    const char *line,
    char *rd,
    size_t rd_size,
    int *out_imm) {
    int imm = 0;

    if (out_imm) {
        *out_imm = 0;
    }
    if (!line || !rd || rd_size == 0u) {
        return 0;
    }
    rd[0] = '\0';
    if (sscanf(line, "  addi %31[^,], sp, %d", rd, &imm) != 2) {
        rd[0] = '\0';
        return 0;
    }
    if (out_imm) {
        *out_imm = imm;
    }
    return 1;
}

static int compiler_riscv_preview_line_is_mv_sp(
    const char *line,
    char *rd,
    size_t rd_size) {
    if (!line || !rd || rd_size == 0u) {
        return 0;
    }
    rd[0] = '\0';
    return sscanf(line, "  mv %31[^,], sp", rd) == 1;
}

static int compiler_riscv_preview_line_is_sw_sp_reg(
    const char *line,
    char *rs,
    size_t rs_size,
    int *out_offset) {
    int offset = 0;

    if (out_offset) {
        *out_offset = 0;
    }
    if (!line || !rs || rs_size == 0u) {
        return 0;
    }
    rs[0] = '\0';
    if (sscanf(line, "  sw %31[^,], %d(sp)", rs, &offset) != 2) {
        rs[0] = '\0';
        return 0;
    }
    if (out_offset) {
        *out_offset = offset;
    }
    return 1;
}

static int compiler_riscv_preview_line_is_lw_sp_reg(
    const char *line,
    char *rd,
    size_t rd_size,
    int *out_offset) {
    int offset = 0;

    if (out_offset) {
        *out_offset = 0;
    }
    if (!line || !rd || rd_size == 0u) {
        return 0;
    }
    rd[0] = '\0';
    if (sscanf(line, "  lw %31[^,], %d(sp)", rd, &offset) != 2) {
        rd[0] = '\0';
        return 0;
    }
    if (out_offset) {
        *out_offset = offset;
    }
    return 1;
}

static int compiler_riscv_preview_line_is_lw_reg_offset(
    const char *line,
    char *rd,
    size_t rd_size,
    int *out_offset,
    char *base,
    size_t base_size) {
    int offset = 0;

    if (out_offset) {
        *out_offset = 0;
    }
    if (!line || !rd || !base || rd_size == 0u || base_size == 0u) {
        return 0;
    }
    rd[0] = '\0';
    base[0] = '\0';
    if (sscanf(line, "  lw %31[^,], %d(%31[^)])", rd, &offset, base) != 3) {
        rd[0] = '\0';
        base[0] = '\0';
        return 0;
    }
    if (out_offset) {
        *out_offset = offset;
    }
    return 1;
}

static int compiler_riscv_preview_line_is_sw_reg_offset(
    const char *line,
    char *rs,
    size_t rs_size,
    int *out_offset,
    char *base,
    size_t base_size) {
    int offset = 0;

    if (out_offset) {
        *out_offset = 0;
    }
    if (!line || !rs || !base || rs_size == 0u || base_size == 0u) {
        return 0;
    }
    rs[0] = '\0';
    base[0] = '\0';
    if (sscanf(line, "  sw %31[^,], %d(%31[^)])", rs, &offset, base) != 3) {
        rs[0] = '\0';
        base[0] = '\0';
        return 0;
    }
    if (out_offset) {
        *out_offset = offset;
    }
    return 1;
}

static int compiler_riscv_preview_two_line_is_sw_materialized_stack_slot(
    char **lines,
    size_t line_index,
    int *out_sp_offset) {
    char addr_reg[32];
    char store_reg[32];
    char store_base[32];
    int addr_imm = 0;
    int store_offset = 0;

    if (out_sp_offset) {
        *out_sp_offset = 0;
    }
    if (!lines) {
        return 0;
    }
    if (!((compiler_riscv_preview_line_is_addi_sp_imm(
                lines[line_index],
                addr_reg,
                sizeof(addr_reg),
                &addr_imm)) ||
            (compiler_riscv_preview_line_is_mv_sp(
                lines[line_index],
                addr_reg,
                sizeof(addr_reg)) && (addr_imm = 0, 1)))) {
        return 0;
    }
    if (!compiler_riscv_preview_line_is_sw_reg_offset(
            lines[line_index + 1u],
            store_reg,
            sizeof(store_reg),
            &store_offset,
            store_base,
            sizeof(store_base))) {
        return 0;
    }
    if (store_offset != 0 || strcmp(store_base, addr_reg) != 0) {
        return 0;
    }
    if (out_sp_offset) {
        *out_sp_offset = addr_imm;
    }
    return 1;
}

static int compiler_riscv_preview_two_line_is_lw_materialized_stack_slot(
    char **lines,
    size_t line_index,
    int *out_sp_offset) {
    char addr_reg[32];
    char load_reg[32];
    char load_base[32];
    int addr_imm = 0;
    int load_offset = 0;

    if (out_sp_offset) {
        *out_sp_offset = 0;
    }
    if (!lines) {
        return 0;
    }
    if (!((compiler_riscv_preview_line_is_addi_sp_imm(
                lines[line_index],
                addr_reg,
                sizeof(addr_reg),
                &addr_imm)) ||
            (compiler_riscv_preview_line_is_mv_sp(
                lines[line_index],
                addr_reg,
                sizeof(addr_reg)) && (addr_imm = 0, 1)))) {
        return 0;
    }
    if (!compiler_riscv_preview_line_is_lw_reg_offset(
            lines[line_index + 1u],
            load_reg,
            sizeof(load_reg),
            &load_offset,
            load_base,
            sizeof(load_base))) {
        return 0;
    }
    if (load_offset != 0 || strcmp(load_base, addr_reg) != 0) {
        return 0;
    }
    if (out_sp_offset) {
        *out_sp_offset = addr_imm;
    }
    return 1;
}

static int compiler_riscv_preview_stack_slot_is_loaded_later(
    char **lines,
    size_t line_count,
    size_t start_index,
    int stack_offset) {
    size_t index;

    if (!lines) {
        return 0;
    }
    for (index = start_index; index < line_count; ++index) {
        char reg_name[32];
        int line_offset = 0;
        int materialized_stack_offset = 0;

        if (compiler_riscv_preview_line_defines_reg(lines[index], "sp")) {
            return 0;
        }
        if (compiler_riscv_preview_line_is_sw_sp_reg(
                lines[index], reg_name, sizeof(reg_name), &line_offset) &&
            line_offset == stack_offset) {
            return 0;
        }
        if (index + 1u < line_count &&
            compiler_riscv_preview_two_line_is_sw_materialized_stack_slot(
                lines,
                index,
                &materialized_stack_offset) &&
            materialized_stack_offset == stack_offset) {
            return 0;
        }
        if (compiler_riscv_preview_line_is_lw_sp_reg(
                lines[index], reg_name, sizeof(reg_name), &line_offset) &&
            line_offset == stack_offset) {
            return 1;
        }
        if (index + 1u < line_count &&
            compiler_riscv_preview_two_line_is_lw_materialized_stack_slot(
                lines,
                index,
                &materialized_stack_offset) &&
            materialized_stack_offset == stack_offset) {
            return 1;
        }
    }
    return 0;
}

static int compiler_riscv_preview_line_is_add_regs(
    const char *line,
    char *rd,
    size_t rd_size,
    char *rs1,
    size_t rs1_size,
    char *rs2,
    size_t rs2_size) {
    if (!line || !rd || !rs1 || !rs2 || rd_size == 0u || rs1_size == 0u || rs2_size == 0u) {
        return 0;
    }
    rd[0] = '\0';
    rs1[0] = '\0';
    rs2[0] = '\0';
    return sscanf(line, "  add %31[^,], %31[^,], %31s", rd, rs1, rs2) == 3;
}

static int compiler_riscv_preview_line_is_mul_regs(
    const char *line,
    char *rd,
    size_t rd_size,
    char *rs1,
    size_t rs1_size,
    char *rs2,
    size_t rs2_size) {
    if (!line || !rd || !rs1 || !rs2 || rd_size == 0u || rs1_size == 0u || rs2_size == 0u) {
        return 0;
    }
    rd[0] = '\0';
    rs1[0] = '\0';
    rs2[0] = '\0';
    return sscanf(line, "  mul %31[^,], %31[^,], %31s", rd, rs1, rs2) == 3;
}

static int compiler_riscv_preview_line_is_sub_regs(
    const char *line,
    char *rd,
    size_t rd_size,
    char *rs1,
    size_t rs1_size,
    char *rs2,
    size_t rs2_size) {
    if (!line || !rd || !rs1 || !rs2 || rd_size == 0u || rs1_size == 0u || rs2_size == 0u) {
        return 0;
    }
    rd[0] = '\0';
    rs1[0] = '\0';
    rs2[0] = '\0';
    return sscanf(line, "  sub %31[^,], %31[^,], %31s", rd, rs1, rs2) == 3;
}

static int compiler_riscv_preview_line_is_slli_by_two(
    const char *line,
    char *rd,
    size_t rd_size,
    char *rs1,
    size_t rs1_size) {
    unsigned imm = 0u;

    if (!line || !rd || !rs1 || rd_size == 0u || rs1_size == 0u) {
        return 0;
    }
    rd[0] = '\0';
    rs1[0] = '\0';
    if (sscanf(line, "  slli %31[^,], %31[^,], %u", rd, rs1, &imm) != 3 || imm != 2u) {
        rd[0] = '\0';
        rs1[0] = '\0';
        return 0;
    }
    return 1;
}

static int compiler_riscv_preview_line_is_slli_imm(
    const char *line,
    char *rd,
    size_t rd_size,
    char *rs1,
    size_t rs1_size,
    unsigned *out_imm) {
    unsigned imm = 0u;

    if (out_imm) {
        *out_imm = 0u;
    }
    if (!line || !rd || !rs1 || rd_size == 0u || rs1_size == 0u) {
        return 0;
    }
    rd[0] = '\0';
    rs1[0] = '\0';
    if (sscanf(line, "  slli %31[^,], %31[^,], %u", rd, rs1, &imm) != 3) {
        rd[0] = '\0';
        rs1[0] = '\0';
        return 0;
    }
    if (out_imm) {
        *out_imm = imm;
    }
    return 1;
}

static int compiler_riscv_preview_line_is_lui_imm(
    const char *line,
    char *rd,
    size_t rd_size,
    char *imm,
    size_t imm_size) {
    if (!line || !rd || !imm || rd_size == 0u || imm_size == 0u) {
        return 0;
    }
    rd[0] = '\0';
    imm[0] = '\0';
    return sscanf(line, "  lui %31[^,], %31s", rd, imm) == 2;
}

static int compiler_riscv_preview_line_is_addi_reg_imm(
    const char *line,
    char *rd,
    size_t rd_size,
    char *rs1,
    size_t rs1_size,
    char *imm,
    size_t imm_size) {
    if (!line || !rd || !rs1 || !imm || rd_size == 0u || rs1_size == 0u || imm_size == 0u) {
        return 0;
    }
    rd[0] = '\0';
    rs1[0] = '\0';
    imm[0] = '\0';
    return sscanf(line, "  addi %31[^,], %31[^,], %31s", rd, rs1, imm) == 3;
}

static int compiler_riscv_preview_line_is_label(const char *line) {
    size_t length;

    if (!line) {
        return 0;
    }
    length = strlen(line);
    return length > 0u && line[length - 1u] == ':';
}

static void compiler_riscv_preview_tokenize_line(const char *line,
    char tokens[][32],
    size_t *out_token_count) {
    size_t src_index = 0u;
    size_t token_count = 0u;
    size_t token_char_index = 0u;
    char current[32];

    if (out_token_count) {
        *out_token_count = 0u;
    }
    if (!line) {
        return;
    }

    while (line[src_index] != '\0') {
        char ch = line[src_index++];
        if (ch == ',' || ch == '(' || ch == ')' || ch == ':') {
            ch = ' ';
        }
        if (ch == ' ' || ch == '\t') {
            if (token_char_index > 0u && token_count < 8u) {
                current[token_char_index] = '\0';
                strncpy(tokens[token_count], current, 31u);
                tokens[token_count][31u] = '\0';
                ++token_count;
                token_char_index = 0u;
            }
            continue;
        }
        if (token_char_index + 1u < sizeof(current)) {
            current[token_char_index++] = ch;
        }
    }
    if (token_char_index > 0u && token_count < 8u) {
        current[token_char_index] = '\0';
        strncpy(tokens[token_count], current, 31u);
        tokens[token_count][31u] = '\0';
        ++token_count;
    }
    if (out_token_count) {
        *out_token_count = token_count;
    }
}

static int compiler_riscv_preview_mnemonic_defines_first_operand(const char *mnemonic) {
    if (!mnemonic || mnemonic[0] == '\0') {
        return 0;
    }
    return strcmp(mnemonic, "li") == 0 ||
        strcmp(mnemonic, "addi") == 0 ||
        strcmp(mnemonic, "mv") == 0 ||
        strcmp(mnemonic, "lui") == 0 ||
        strcmp(mnemonic, "lw") == 0 ||
        strcmp(mnemonic, "add") == 0 ||
        strcmp(mnemonic, "sub") == 0 ||
        strcmp(mnemonic, "mul") == 0 ||
        strcmp(mnemonic, "div") == 0 ||
        strcmp(mnemonic, "rem") == 0 ||
        strcmp(mnemonic, "slli") == 0 ||
        strcmp(mnemonic, "srai") == 0 ||
        strcmp(mnemonic, "andi") == 0 ||
        strcmp(mnemonic, "xori") == 0 ||
        strcmp(mnemonic, "ori") == 0 ||
        strcmp(mnemonic, "slti") == 0 ||
        strcmp(mnemonic, "sltiu") == 0 ||
        strcmp(mnemonic, "xor") == 0 ||
        strcmp(mnemonic, "or") == 0 ||
        strcmp(mnemonic, "and") == 0 ||
        strcmp(mnemonic, "sll") == 0 ||
        strcmp(mnemonic, "srl") == 0 ||
        strcmp(mnemonic, "sra") == 0 ||
        strcmp(mnemonic, "slt") == 0 ||
        strcmp(mnemonic, "sltu") == 0 ||
        strcmp(mnemonic, "jal") == 0 ||
        strcmp(mnemonic, "jalr") == 0;
}

static int compiler_riscv_preview_reg_is_call_clobbered(const char *reg_name) {
    if (!reg_name || reg_name[0] == '\0') {
        return 0;
    }

    return strcmp(reg_name, "ra") == 0 ||
        strcmp(reg_name, "t0") == 0 ||
        strcmp(reg_name, "t1") == 0 ||
        strcmp(reg_name, "t2") == 0 ||
        strcmp(reg_name, "t3") == 0 ||
        strcmp(reg_name, "t4") == 0 ||
        strcmp(reg_name, "t5") == 0 ||
        strcmp(reg_name, "t6") == 0 ||
        strcmp(reg_name, "a0") == 0 ||
        strcmp(reg_name, "a1") == 0 ||
        strcmp(reg_name, "a2") == 0 ||
        strcmp(reg_name, "a3") == 0 ||
        strcmp(reg_name, "a4") == 0 ||
        strcmp(reg_name, "a5") == 0 ||
        strcmp(reg_name, "a6") == 0 ||
        strcmp(reg_name, "a7") == 0;
}

static int compiler_riscv_preview_line_is_call_like(const char *line) {
    char tokens[8][32];
    size_t token_count = 0u;

    if (!line || compiler_riscv_preview_line_is_label(line)) {
        return 0;
    }

    compiler_riscv_preview_tokenize_line(line, tokens, &token_count);
    if (token_count == 0u) {
        return 0;
    }

    if (strcmp(tokens[0], "call") == 0 || strcmp(tokens[0], "ecall") == 0) {
        return 1;
    }
    if ((strcmp(tokens[0], "jal") == 0 || strcmp(tokens[0], "jalr") == 0) &&
        token_count >= 2u &&
        strcmp(tokens[1], "ra") == 0) {
        return 1;
    }

    return 0;
}

static int compiler_riscv_preview_line_is_control_transfer_barrier(const char *line) {
    char tokens[8][32];
    size_t token_count = 0u;

    if (!line || compiler_riscv_preview_line_is_label(line)) {
        return 0;
    }
    if (compiler_riscv_preview_line_is_call_like(line) || compiler_riscv_preview_line_is_ret(line)) {
        return 1;
    }

    compiler_riscv_preview_tokenize_line(line, tokens, &token_count);
    if (token_count == 0u) {
        return 0;
    }
    if (strcmp(tokens[0], "j") == 0 || strcmp(tokens[0], "beq") == 0 || strcmp(tokens[0], "bne") == 0 ||
        strcmp(tokens[0], "blt") == 0 || strcmp(tokens[0], "bge") == 0 || strcmp(tokens[0], "bltu") == 0 ||
        strcmp(tokens[0], "bgeu") == 0) {
        return 1;
    }
    return strcmp(tokens[0], "jal") == 0 && token_count >= 2u && strcmp(tokens[1], "zero") == 0;
}

static int compiler_riscv_preview_line_defines_reg(const char *line, const char *reg_name) {
    char tokens[8][32];
    size_t token_count = 0u;

    if (!line || !reg_name || reg_name[0] == '\0' || compiler_riscv_preview_line_is_label(line)) {
        return 0;
    }
    if (compiler_riscv_preview_line_is_call_like(line) &&
        compiler_riscv_preview_reg_is_call_clobbered(reg_name)) {
        return 1;
    }
    compiler_riscv_preview_tokenize_line(line, tokens, &token_count);
    if (token_count < 2u) {
        return 0;
    }
    return compiler_riscv_preview_mnemonic_defines_first_operand(tokens[0]) &&
        strcmp(tokens[1], reg_name) == 0;
}

static int compiler_riscv_preview_line_uses_reg(const char *line, const char *reg_name) {
    char tokens[8][32];
    size_t token_count = 0u;
    size_t token_index = 1u;

    if (!line || !reg_name || reg_name[0] == '\0' || compiler_riscv_preview_line_is_label(line)) {
        return 0;
    }
    compiler_riscv_preview_tokenize_line(line, tokens, &token_count);
    if (token_count < 2u) {
        return 0;
    }
    if (compiler_riscv_preview_mnemonic_defines_first_operand(tokens[0])) {
        token_index = 2u;
    }
    for (; token_index < token_count; ++token_index) {
        if (strcmp(tokens[token_index], reg_name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int compiler_riscv_preview_reg_is_used_again_before_redefinition(
    char **lines,
    size_t line_count,
    size_t start_index,
    const char *reg_name) {
    size_t index;

    if (!lines || !reg_name || reg_name[0] == '\0') {
        return 0;
    }
    for (index = start_index; index < line_count; ++index) {
        const char *line = lines[index];

        if (!line) {
            continue;
        }
        if (compiler_riscv_preview_line_is_label(line) ||
            compiler_riscv_preview_line_is_control_transfer_barrier(line)) {
            return 0;
        }
        if (compiler_riscv_preview_line_defines_reg(line, reg_name)) {
            return 0;
        }
        if (compiler_riscv_preview_line_uses_reg(line, reg_name)) {
            return 1;
        }
    }
    return 0;
}

static int compiler_riscv_preview_reg_may_be_needed_past_label_before_redefinition(
    char **lines,
    size_t line_count,
    size_t start_index,
    const char *reg_name) {
    size_t index;

    if (!lines || !reg_name || reg_name[0] == '\0') {
        return 0;
    }
    for (index = start_index; index < line_count; ++index) {
        const char *line = lines[index];

        if (!line) {
            continue;
        }
        if (compiler_riscv_preview_line_is_label(line) ||
            compiler_riscv_preview_line_is_control_transfer_barrier(line)) {
            return 1;
        }
        if (compiler_riscv_preview_line_defines_reg(line, reg_name)) {
            return 0;
        }
        if (compiler_riscv_preview_line_uses_reg(line, reg_name)) {
            return 1;
        }
    }
    return 0;
}

static int compiler_optimize_riscv_preview_tail_calls(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char target_name[256];
        size_t scan_index = index + 1u;
        int s11_offset = 0;
        int ra_offset = 0;
        int sp_delta = 0;

        if (compiler_riscv_preview_line_is_jal_ra(lines[index], target_name, sizeof(target_name))) {
            if (scan_index + 3u < line_count &&
                /*
                 * Do not fold across intervening `lw ..., off(s11)` restore rows.
                 * Earlier retries skipped those lines and then dropped them from
                 * the rebuilt tail-call sequence, which can corrupt callee-saved
                 * state in the generated program.
                 */
                !compiler_riscv_preview_line_is_restore_from_s11(lines[scan_index]) &&
                compiler_riscv_preview_line_is_restore_from_sp(lines[scan_index], "s11", &s11_offset) &&
                compiler_riscv_preview_line_is_restore_from_sp(lines[scan_index + 1u], "ra", &ra_offset) &&
                compiler_riscv_preview_line_is_addi_sp(lines[scan_index + 2u], &sp_delta) &&
                compiler_riscv_preview_line_is_ret(lines[scan_index + 3u]) &&
                ra_offset - s11_offset == 4 &&
                sp_delta > 0) {
                size_t emit_index;

                if (!compiler_builder_appendf(&builder, "  lw s11, %d(sp)\n", s11_offset) ||
                    !compiler_builder_appendf(&builder, "  lw ra, %d(sp)\n", ra_offset) ||
                    !compiler_builder_appendf(&builder, "  addi sp, sp, %d\n", sp_delta) ||
                    !compiler_builder_appendf(&builder, "  j %s\n", target_name)) {
                    goto cleanup;
                }
                index = scan_index + 4u;
                for (emit_index = index; emit_index < line_count && lines[emit_index][0] == '\0'; ++emit_index) {
                    if (!compiler_builder_appendf(&builder, "\n")) {
                        goto cleanup;
                    }
                }
                index = emit_index;
                continue;
            }
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

int compiler_test_optimize_riscv_preview_tail_calls(char **io_text) {
    return compiler_optimize_riscv_preview_tail_calls(io_text);
}

static int compiler_optimize_riscv_preview_zero_adds(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char zero_reg[32];
        char rd[32];
        char rs1[32];
        char rs2[32];

        if (index + 1u < line_count &&
            compiler_riscv_preview_line_is_li_zero(lines[index], zero_reg, sizeof(zero_reg)) &&
            compiler_riscv_preview_line_is_add_regs(lines[index + 1u], rd, sizeof(rd), rs1, sizeof(rs1), rs2, sizeof(rs2)) &&
            (strcmp(rs1, zero_reg) == 0 || strcmp(rs2, zero_reg) == 0) &&
            (!compiler_riscv_preview_reg_may_be_needed_past_label_before_redefinition(
                    lines, line_count, index + 2u, zero_reg) ||
                strcmp(rd, zero_reg) == 0)) {
            const char *kept_reg = strcmp(rs1, zero_reg) == 0 ? rs2 : rs1;

            if (strcmp(rd, kept_reg) != 0) {
                if (!compiler_builder_appendf(&builder, "  mv %s, %s\n", rd, kept_reg)) {
                    goto cleanup;
                }
            }
            index += 2u;
            continue;
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

int compiler_test_optimize_riscv_preview_zero_adds(char **io_text) {
    return compiler_optimize_riscv_preview_zero_adds(io_text);
}

int compiler_test_optimize_riscv_preview_mul_by_four(char **io_text) {
    return compiler_optimize_riscv_preview_mul_by_four(io_text);
}

static int compiler_optimize_riscv_preview_mul_by_four(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char scale_reg[32];
        char rd[32];
        char rs1[32];
        char rs2[32];

        if (index + 1u < line_count &&
            compiler_riscv_preview_line_is_li_four(lines[index], scale_reg, sizeof(scale_reg)) &&
            compiler_riscv_preview_line_is_mul_regs(lines[index + 1u], rd, sizeof(rd), rs1, sizeof(rs1), rs2, sizeof(rs2)) &&
            strcmp(rs2, scale_reg) == 0 &&
            !compiler_riscv_preview_reg_may_be_needed_past_label_before_redefinition(
                lines, line_count, index + 2u, scale_reg)) {
            if (!compiler_builder_appendf(&builder, "  slli %s, %s, 2\n", rd, rs1)) {
                goto cleanup;
            }
            index += 2u;
            continue;
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

static int compiler_optimize_riscv_preview_sub_by_one(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char one_reg[32];
        char rd[32];
        char rs1[32];
        char rs2[32];

        if (index + 1u < line_count &&
            compiler_riscv_preview_line_is_li_one(lines[index], one_reg, sizeof(one_reg)) &&
            compiler_riscv_preview_line_is_sub_regs(lines[index + 1u], rd, sizeof(rd), rs1, sizeof(rs1), rs2, sizeof(rs2)) &&
            strcmp(rs2, one_reg) == 0 &&
            !compiler_riscv_preview_reg_may_be_needed_past_label_before_redefinition(
                lines, line_count, index + 2u, one_reg)) {
            if (!compiler_builder_appendf(&builder, "  addi %s, %s, -1\n", rd, rs1)) {
                goto cleanup;
            }
            index += 2u;
            continue;
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

int compiler_test_optimize_riscv_preview_sub_by_one(char **io_text) {
    return compiler_optimize_riscv_preview_sub_by_one(io_text);
}

static int compiler_optimize_riscv_preview_reuse_repeated_lui_addi_constants(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char first_rd[32];
        char first_imm_hi[32];
        char first_add_rd[32];
        char first_add_rs[32];
        char first_imm_lo[32];
        size_t scan_index;

        if (index + 1u < line_count &&
            compiler_riscv_preview_line_is_lui_imm(
                lines[index], first_rd, sizeof(first_rd), first_imm_hi, sizeof(first_imm_hi)) &&
            compiler_riscv_preview_line_is_addi_reg_imm(
                lines[index + 1u],
                first_add_rd,
                sizeof(first_add_rd),
                first_add_rs,
                sizeof(first_add_rs),
                first_imm_lo,
                sizeof(first_imm_lo)) &&
            strcmp(first_add_rd, first_rd) == 0 &&
            strcmp(first_add_rs, first_rd) == 0) {
            for (scan_index = index + 2u; scan_index + 1u < line_count; ++scan_index) {
                char next_rd[32];
                char next_imm_hi[32];
                char next_add_rd[32];
                char next_add_rs[32];
                char next_imm_lo[32];
                size_t check_index;
                int blocked = 0;

                if (compiler_riscv_preview_line_is_label(lines[scan_index]) ||
                    compiler_riscv_preview_line_is_control_transfer_barrier(lines[scan_index])) {
                    break;
                }
                if (!compiler_riscv_preview_line_is_lui_imm(
                        lines[scan_index], next_rd, sizeof(next_rd), next_imm_hi, sizeof(next_imm_hi)) ||
                    !compiler_riscv_preview_line_is_addi_reg_imm(
                        lines[scan_index + 1u],
                        next_add_rd,
                        sizeof(next_add_rd),
                        next_add_rs,
                        sizeof(next_add_rs),
                        next_imm_lo,
                        sizeof(next_imm_lo)) ||
                    strcmp(next_rd, first_rd) != 0 ||
                    strcmp(next_add_rd, first_rd) != 0 ||
                    strcmp(next_add_rs, first_rd) != 0 ||
                    strcmp(next_imm_hi, first_imm_hi) != 0 ||
                    strcmp(next_imm_lo, first_imm_lo) != 0) {
                    continue;
                }

                for (check_index = index + 2u; check_index < scan_index; ++check_index) {
                    if (compiler_riscv_preview_line_is_label(lines[check_index]) ||
                        compiler_riscv_preview_line_is_control_transfer_barrier(lines[check_index]) ||
                        compiler_riscv_preview_line_defines_reg(lines[check_index], first_rd)) {
                        blocked = 1;
                        break;
                    }
                }
                if (blocked) {
                    break;
                }

                if (!compiler_builder_appendf(&builder, "%s\n", lines[index]) ||
                    !compiler_builder_appendf(&builder, "%s\n", lines[index + 1u])) {
                    goto cleanup;
                }
                index += 2u;
                while (index < scan_index) {
                    if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
                        goto cleanup;
                    }
                    ++index;
                }
                index += 2u;
                goto next_line;
            }
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
next_line:
        continue;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

int compiler_test_optimize_riscv_preview_reuse_repeated_lui_addi_constants(char **io_text) {
    return compiler_optimize_riscv_preview_reuse_repeated_lui_addi_constants(io_text);
}

static int compiler_optimize_riscv_preview_stack_addr_reuse(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char tmp_reg[32];
        char store_reg[32];
        int tmp_imm = 0;
        int store_offset = 0;
        size_t scan_index;
        size_t end_index;
        int replaced_any = 0;

        if (index + 1u < line_count &&
            compiler_riscv_preview_line_is_addi_sp_imm(lines[index], tmp_reg, sizeof(tmp_reg), &tmp_imm) &&
            compiler_riscv_preview_line_is_sw_sp_reg(lines[index + 1u], store_reg, sizeof(store_reg), &store_offset) &&
            strcmp(tmp_reg, store_reg) == 0 &&
            !compiler_riscv_preview_reg_may_be_needed_past_label_before_redefinition(
                lines, line_count, index + 2u, tmp_reg)) {
            end_index = line_count;

            for (scan_index = index + 2u; scan_index < line_count; ++scan_index) {
                char load_reg[32];
                int load_offset = 0;
                int materialized_stack_offset = 0;

                if (compiler_riscv_preview_line_is_label(lines[scan_index]) ||
                    compiler_riscv_preview_line_is_control_transfer_barrier(lines[scan_index]) ||
                    compiler_riscv_preview_line_defines_reg(lines[scan_index], "sp")) {
                    end_index = scan_index;
                    break;
                }
                if (compiler_riscv_preview_line_is_sw_sp_reg(
                        lines[scan_index], store_reg, sizeof(store_reg), &load_offset) &&
                    load_offset == store_offset) {
                    end_index = scan_index;
                    break;
                }
                if (scan_index + 1u < line_count &&
                    compiler_riscv_preview_two_line_is_sw_materialized_stack_slot(
                        lines,
                        scan_index,
                        &materialized_stack_offset) &&
                    materialized_stack_offset == store_offset) {
                    end_index = scan_index;
                    break;
                }
                if (compiler_riscv_preview_line_is_lw_sp_reg(
                        lines[scan_index], load_reg, sizeof(load_reg), &load_offset) &&
                    load_offset == store_offset) {
                    replaced_any = 1;
                }
            }

            if (replaced_any &&
                !compiler_riscv_preview_stack_slot_is_loaded_later(
                    lines, line_count, end_index, store_offset)) {
                for (scan_index = index + 2u; scan_index < end_index; ++scan_index) {
                    char load_reg[32];
                    int load_offset = 0;

                    if (compiler_riscv_preview_line_is_lw_sp_reg(
                            lines[scan_index], load_reg, sizeof(load_reg), &load_offset) &&
                        load_offset == store_offset) {
                        if (!compiler_builder_appendf(&builder, "  addi %s, sp, %d\n", load_reg, tmp_imm)) {
                            goto cleanup;
                        }
                    } else {
                        if (!compiler_builder_appendf(&builder, "%s\n", lines[scan_index])) {
                            goto cleanup;
                        }
                    }
                }
                index = end_index;
                continue;
            }
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

int compiler_test_optimize_riscv_preview_stack_addr_reuse(char **io_text) {
    return compiler_optimize_riscv_preview_stack_addr_reuse(io_text);
}

static int compiler_optimize_riscv_preview_repeated_indexed_addr_triples(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char shift_rd[32];
        char shift_rs[32];
        char load_rd[32];
        char add_rd[32];
        char add_rs1[32];
        char add_rs2[32];
        int load_offset = 0;
        size_t scan_index;

        if (index + 2u < line_count &&
            compiler_riscv_preview_line_is_slli_by_two(lines[index], shift_rd, sizeof(shift_rd), shift_rs, sizeof(shift_rs)) &&
            compiler_riscv_preview_line_is_lw_sp_reg(lines[index + 1u], load_rd, sizeof(load_rd), &load_offset) &&
            compiler_riscv_preview_line_is_add_regs(
                lines[index + 2u], add_rd, sizeof(add_rd), add_rs1, sizeof(add_rs1), add_rs2, sizeof(add_rs2)) &&
            strcmp(add_rd, shift_rd) == 0 &&
            strcmp(add_rs1, load_rd) == 0 &&
            strcmp(add_rs2, shift_rd) == 0) {
            for (scan_index = index + 3u; scan_index + 2u < line_count; ++scan_index) {
                char next_shift_rd[32];
                char next_shift_rs[32];
                char next_load_rd[32];
                char next_add_rd[32];
                char next_add_rs1[32];
                char next_add_rs2[32];
                char store_reg[32];
                int next_load_offset = 0;
                int store_offset = 0;
                int materialized_stack_offset = 0;
                size_t check_index;
                int invalidated = 0;

                if (!compiler_riscv_preview_line_is_slli_by_two(
                        lines[scan_index], next_shift_rd, sizeof(next_shift_rd), next_shift_rs, sizeof(next_shift_rs)) ||
                    !compiler_riscv_preview_line_is_lw_sp_reg(
                        lines[scan_index + 1u], next_load_rd, sizeof(next_load_rd), &next_load_offset) ||
                    !compiler_riscv_preview_line_is_add_regs(
                        lines[scan_index + 2u],
                        next_add_rd,
                        sizeof(next_add_rd),
                        next_add_rs1,
                        sizeof(next_add_rs1),
                        next_add_rs2,
                        sizeof(next_add_rs2))) {
                    if (compiler_riscv_preview_line_is_label(lines[scan_index])) {
                        break;
                    }
                    continue;
                }

                if (strcmp(next_shift_rd, shift_rd) != 0 ||
                    strcmp(next_shift_rs, shift_rs) != 0 ||
                    strcmp(next_load_rd, load_rd) != 0 ||
                    next_load_offset != load_offset ||
                    strcmp(next_add_rd, add_rd) != 0 ||
                    strcmp(next_add_rs1, add_rs1) != 0 ||
                    strcmp(next_add_rs2, add_rs2) != 0) {
                    continue;
                }

                for (check_index = index + 3u; check_index < scan_index; ++check_index) {
                    if (compiler_riscv_preview_line_is_label(lines[check_index]) ||
                        compiler_riscv_preview_line_is_control_transfer_barrier(lines[check_index]) ||
                        compiler_riscv_preview_line_defines_reg(lines[check_index], shift_rd) ||
                        compiler_riscv_preview_line_defines_reg(lines[check_index], shift_rs) ||
                        (compiler_riscv_preview_line_is_sw_sp_reg(
                                lines[check_index],
                                store_reg,
                                sizeof(store_reg),
                                &store_offset) &&
                            store_offset == load_offset) ||
                        (check_index + 1u < scan_index &&
                            compiler_riscv_preview_two_line_is_sw_materialized_stack_slot(
                                lines,
                                check_index,
                                &materialized_stack_offset) &&
                            materialized_stack_offset == load_offset) ||
                        compiler_riscv_preview_line_defines_reg(lines[check_index], load_rd)) {
                        invalidated = 1;
                        break;
                    }
                }
                if (invalidated) {
                    break;
                }

                if (!compiler_builder_appendf(&builder, "%s\n", lines[index]) ||
                    !compiler_builder_appendf(&builder, "%s\n", lines[index + 1u]) ||
                    !compiler_builder_appendf(&builder, "%s\n", lines[index + 2u])) {
                    goto cleanup;
                }
                index += 3u;
                while (index < scan_index) {
                    if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
                        goto cleanup;
                    }
                    ++index;
                }
                index = scan_index + 3u;
                goto next_line;
            }
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
next_line:
        continue;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

static int compiler_optimize_riscv_preview_repeated_indexed_addr_sequences(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char first_shift_rd[32];
        char first_shift_rs[32];
        char first_load_rd[32];
        char first_load_base[32];
        char first_add_rd[32];
        char first_add_rs1[32];
        char first_add_rs2[32];
        unsigned first_shift_imm = 0u;
        int first_load_offset = 0;

        if (index + 2u < line_count &&
            compiler_riscv_preview_line_is_slli_imm(
                lines[index], first_shift_rd, sizeof(first_shift_rd), first_shift_rs, sizeof(first_shift_rs), &first_shift_imm) &&
            first_shift_imm == 2u &&
            compiler_riscv_preview_line_is_lw_reg_offset(
                lines[index + 1u],
                first_load_rd,
                sizeof(first_load_rd),
                &first_load_offset,
                first_load_base,
                sizeof(first_load_base)) &&
            strcmp(first_load_base, "sp") == 0 &&
            compiler_riscv_preview_line_is_add_regs(
                lines[index + 2u],
                first_add_rd,
                sizeof(first_add_rd),
                first_add_rs1,
                sizeof(first_add_rs1),
                first_add_rs2,
                sizeof(first_add_rs2)) &&
            strcmp(first_add_rd, first_shift_rd) == 0 &&
            strcmp(first_add_rs1, first_load_rd) == 0 &&
            strcmp(first_add_rs2, first_shift_rd) == 0) {
            size_t scan_index;
            int replaced = 0;

            for (scan_index = index + 3u; scan_index + 2u < line_count; ++scan_index) {
                char second_shift_rd[32];
                char second_shift_rs[32];
                char second_load_rd[32];
                char second_load_base[32];
                char second_add_rd[32];
                char second_add_rs1[32];
                char second_add_rs2[32];
                char store_reg[32];
                char store_base[32];
                unsigned second_shift_imm = 0u;
                int second_load_offset = 0;
                int store_offset = 0;
                int materialized_stack_offset = 0;
                size_t check_index;
                int invalidated = 0;

                if (!compiler_riscv_preview_line_is_slli_imm(lines[scan_index],
                        second_shift_rd,
                        sizeof(second_shift_rd),
                        second_shift_rs,
                        sizeof(second_shift_rs),
                        &second_shift_imm) ||
                    second_shift_imm != first_shift_imm ||
                    strcmp(second_shift_rd, first_shift_rd) != 0 ||
                    strcmp(second_shift_rs, first_shift_rs) != 0 ||
                    !compiler_riscv_preview_line_is_lw_reg_offset(
                        lines[scan_index + 1u],
                        second_load_rd,
                        sizeof(second_load_rd),
                        &second_load_offset,
                        second_load_base,
                        sizeof(second_load_base)) ||
                    second_load_offset != first_load_offset ||
                    strcmp(second_load_rd, first_load_rd) != 0 ||
                    strcmp(second_load_base, first_load_base) != 0 ||
                    !compiler_riscv_preview_line_is_add_regs(
                        lines[scan_index + 2u],
                        second_add_rd,
                        sizeof(second_add_rd),
                        second_add_rs1,
                        sizeof(second_add_rs1),
                        second_add_rs2,
                        sizeof(second_add_rs2)) ||
                    strcmp(second_add_rd, first_add_rd) != 0 ||
                    strcmp(second_add_rs1, first_add_rs1) != 0 ||
                    strcmp(second_add_rs2, first_add_rs2) != 0) {
                    continue;
                }

                for (check_index = index + 3u; check_index < scan_index; ++check_index) {
                    if (compiler_riscv_preview_line_is_label(lines[check_index]) ||
                        compiler_riscv_preview_line_is_control_transfer_barrier(lines[check_index]) ||
                        compiler_riscv_preview_line_defines_reg(lines[check_index], first_shift_rd) ||
                        compiler_riscv_preview_line_defines_reg(lines[check_index], first_shift_rs) ||
                        compiler_riscv_preview_line_defines_reg(lines[check_index], first_load_rd) ||
                        compiler_riscv_preview_line_defines_reg(lines[check_index], first_load_base) ||
                        (compiler_riscv_preview_line_is_sw_reg_offset(
                                lines[check_index],
                                store_reg,
                                sizeof(store_reg),
                                &store_offset,
                                store_base,
                                sizeof(store_base)) &&
                            store_offset == first_load_offset &&
                            strcmp(store_base, first_load_base) == 0) ||
                        (strcmp(first_load_base, "sp") == 0 &&
                            check_index + 1u < scan_index &&
                            compiler_riscv_preview_two_line_is_sw_materialized_stack_slot(
                                lines,
                                check_index,
                                &materialized_stack_offset) &&
                            materialized_stack_offset == first_load_offset)) {
                        invalidated = 1;
                        break;
                    }
                }
                if (invalidated) {
                    break;
                }

                if (!compiler_builder_appendf(&builder, "%s\n", lines[index]) ||
                    !compiler_builder_appendf(&builder, "%s\n", lines[index + 1u]) ||
                    !compiler_builder_appendf(&builder, "%s\n", lines[index + 2u])) {
                    goto cleanup;
                }
                index += 3u;
                while (index < scan_index) {
                    if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
                        goto cleanup;
                    }
                    ++index;
                }
                index = scan_index + 3u;
                replaced = 1;
                break;
            }

            if (replaced) {
                continue;
            }
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

static int compiler_optimize_riscv_preview_call_arg_load_swaps(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    if (!strstr(text, "  mv t5, a0\n  mv a0, a1\n  mv a1, t5\n  call ")) {
        return 1;
    }
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char first_reg[32];
        char first_base[32];
        char second_reg[32];
        char second_base[32];
        int first_offset = 0;
        int second_offset = 0;

        if (index + 5u < line_count &&
            compiler_riscv_preview_line_is_lw_reg_offset(
                lines[index], first_reg, sizeof(first_reg), &first_offset, first_base, sizeof(first_base)) &&
            compiler_riscv_preview_line_is_lw_reg_offset(
                lines[index + 1u], second_reg, sizeof(second_reg), &second_offset, second_base, sizeof(second_base)) &&
            strcmp(first_reg, "a1") == 0 &&
            strcmp(second_reg, "a0") == 0 &&
            strcmp(first_base, "a0") != 0 &&
            strcmp(second_base, "a0") != 0 &&
            strcmp(second_base, "a1") != 0 &&
            strcmp(lines[index + 2u], "  mv t5, a0") == 0 &&
            strcmp(lines[index + 3u], "  mv a0, a1") == 0 &&
            strcmp(lines[index + 4u], "  mv a1, t5") == 0 &&
            strncmp(lines[index + 5u], "  call ", 7u) == 0) {
            if (!compiler_builder_appendf(&builder, "  lw a0, %d(%s)\n", first_offset, first_base) ||
                !compiler_builder_appendf(&builder, "  lw a1, %d(%s)\n", second_offset, second_base) ||
                !compiler_builder_appendf(&builder, "%s\n", lines[index + 5u])) {
                goto cleanup;
            }
            index += 6u;
            continue;
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

int compiler_test_optimize_riscv_preview_call_arg_load_swaps(char **io_text) {
    return compiler_optimize_riscv_preview_call_arg_load_swaps(io_text);
}

int compiler_test_optimize_riscv_preview_repeated_indexed_addr_triples(char **io_text) {
    return compiler_optimize_riscv_preview_repeated_indexed_addr_triples(io_text);
}

int compiler_test_optimize_riscv_preview_repeated_indexed_addr_sequences(char **io_text) {
    return compiler_optimize_riscv_preview_repeated_indexed_addr_sequences(io_text);
}

static int compiler_optimize_riscv_preview_stack_staged_call_args(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    if (!strstr(text, "  call ")) {
        return 1;
    }
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char stage0_reg[32];
        char stage1_reg[32];
        char store0_reg[32];
        char store1_reg[32];
        char load0_reg[32];
        char load1_reg[32];
        int src0_offset = 0;
        int src1_offset = 0;
        int dst0_offset = 0;
        int dst1_offset = 0;
        int load0_offset = 0;
        int load1_offset = 0;

        if (index + 6u < line_count &&
            compiler_riscv_preview_line_is_lw_sp_reg(lines[index], stage0_reg, sizeof(stage0_reg), &src0_offset) &&
            compiler_riscv_preview_line_is_sw_sp_reg(lines[index + 1u], store0_reg, sizeof(store0_reg), &dst0_offset) &&
            compiler_riscv_preview_line_is_lw_sp_reg(lines[index + 2u], stage1_reg, sizeof(stage1_reg), &src1_offset) &&
            compiler_riscv_preview_line_is_sw_sp_reg(lines[index + 3u], store1_reg, sizeof(store1_reg), &dst1_offset) &&
            compiler_riscv_preview_line_is_lw_sp_reg(lines[index + 4u], load0_reg, sizeof(load0_reg), &load0_offset) &&
            compiler_riscv_preview_line_is_lw_sp_reg(lines[index + 5u], load1_reg, sizeof(load1_reg), &load1_offset) &&
            strcmp(stage0_reg, store0_reg) == 0 &&
            strcmp(stage1_reg, store1_reg) == 0 &&
            strcmp(load0_reg, "a0") == 0 &&
            strcmp(load1_reg, "a1") == 0 &&
            strcmp(stage0_reg, "a0") != 0 &&
            strcmp(stage0_reg, "a1") != 0 &&
            strcmp(stage1_reg, "a0") != 0 &&
            strcmp(stage1_reg, "a1") != 0 &&
            dst0_offset != dst1_offset &&
            load0_offset == dst0_offset &&
            load1_offset == dst1_offset &&
            strncmp(lines[index + 6u], "  call ", 7u) == 0) {
            if (!compiler_builder_appendf(&builder, "  lw a0, %d(sp)\n", src0_offset) ||
                !compiler_builder_appendf(&builder, "  sw a0, %d(sp)\n", dst0_offset) ||
                !compiler_builder_appendf(&builder, "  lw a1, %d(sp)\n", src1_offset) ||
                !compiler_builder_appendf(&builder, "  sw a1, %d(sp)\n", dst1_offset) ||
                !compiler_builder_appendf(&builder, "%s\n", lines[index + 6u])) {
                goto cleanup;
            }
            index += 7u;
            continue;
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

int compiler_test_optimize_riscv_preview_stack_staged_call_args(char **io_text) {
    return compiler_optimize_riscv_preview_stack_staged_call_args(io_text);
}

static int compiler_riscv_preview_is_temp_reg_name(const char *reg_name) {
    return reg_name && reg_name[0] == 't' && reg_name[1] != '\0';
}

static int compiler_optimize_riscv_preview_same_block_temp_stack_reload_to_mv(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char store_reg[32];
        int store_offset = 0;
        size_t scan_index;
        int replaced = 0;

        if (compiler_riscv_preview_line_is_sw_sp_reg(lines[index], store_reg, sizeof(store_reg), &store_offset) &&
            compiler_riscv_preview_is_temp_reg_name(store_reg)) {
            for (scan_index = index + 1u; scan_index < line_count; ++scan_index) {
                char load_reg[32];
                char overwrite_reg[32];
                int load_offset = 0;
                int materialized_stack_offset = 0;
                char sp_adjust_rd[32];
                int sp_adjust_imm = 0;

                if (compiler_riscv_preview_line_is_label(lines[scan_index]) ||
                    compiler_riscv_preview_line_is_control_transfer_barrier(lines[scan_index]) ||
                    compiler_riscv_preview_line_defines_reg(lines[scan_index], store_reg)) {
                    break;
                }
                if (compiler_riscv_preview_line_is_addi_sp_imm(
                        lines[scan_index], sp_adjust_rd, sizeof(sp_adjust_rd), &sp_adjust_imm) ||
                    compiler_riscv_preview_line_defines_reg(lines[scan_index], "sp")) {
                    break;
                }
                if (compiler_riscv_preview_line_is_sw_sp_reg(
                        lines[scan_index], overwrite_reg, sizeof(overwrite_reg), &load_offset) &&
                    load_offset == store_offset) {
                    break;
                }
                if (scan_index + 1u < line_count &&
                    compiler_riscv_preview_two_line_is_sw_materialized_stack_slot(
                        lines,
                        scan_index,
                        &materialized_stack_offset) &&
                    materialized_stack_offset == store_offset) {
                    break;
                }
                if (!compiler_riscv_preview_line_is_lw_sp_reg(
                        lines[scan_index], load_reg, sizeof(load_reg), &load_offset) ||
                    load_offset != store_offset ||
                    !compiler_riscv_preview_is_temp_reg_name(load_reg)) {
                    continue;
                }

                if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
                    goto cleanup;
                }
                for (++index; index < scan_index; ++index) {
                    if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
                        goto cleanup;
                    }
                }
                if (strcmp(load_reg, store_reg) != 0) {
                    if (!compiler_builder_appendf(&builder, "  mv %s, %s\n", load_reg, store_reg)) {
                        goto cleanup;
                    }
                }
                index = scan_index + 1u;
                replaced = 1;
                break;
            }
        }

        if (replaced) {
            continue;
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

int compiler_test_optimize_riscv_preview_same_block_temp_stack_reload_to_mv(char **io_text) {
    return compiler_optimize_riscv_preview_same_block_temp_stack_reload_to_mv(io_text);
}

static int compiler_optimize_riscv_preview_indexed_local_base_offsets(char **io_text) {
    char *text = NULL;
    char *copy = NULL;
    char **lines = NULL;
    size_t line_count = 0u;
    size_t line_capacity = 0u;
    char *cursor = NULL;
    CompilerStringBuilder builder;
    size_t index = 0u;
    int ok = 0;

    if (!io_text || !*io_text) {
        return 1;
    }

    text = *io_text;
    copy = (char *)malloc(strlen(text) + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, text, strlen(text) + 1u);

    cursor = copy;
    while (*cursor != '\0') {
        char *line_start = cursor;
        char *newline = strchr(cursor, '\n');

        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor += strlen(cursor);
        }

        if (line_count == line_capacity) {
            size_t new_capacity = line_capacity > 0u ? line_capacity * 2u : 128u;
            char **new_lines = (char **)realloc(lines, new_capacity * sizeof(char *));

            if (!new_lines) {
                goto cleanup;
            }
            lines = new_lines;
            line_capacity = new_capacity;
        }
        lines[line_count++] = line_start;
    }

    compiler_builder_init(&builder);
    while (index < line_count) {
        char base_reg[32];
        int base_imm = 0;
        size_t scan_index = 0u;
        size_t end_index = 0u;
        int replaced_any = 0;
        int blocked = 0;

        if (index + 2u < line_count &&
            (compiler_riscv_preview_line_is_addi_sp_imm(lines[index], base_reg, sizeof(base_reg), &base_imm) ||
                (compiler_riscv_preview_line_is_mv_sp(lines[index], base_reg, sizeof(base_reg)) && (base_imm = 0, 1))) &&
            base_reg[0] == 't' &&
            !compiler_riscv_preview_reg_may_be_needed_past_label_before_redefinition(
                lines, line_count, index + 1u, base_reg)) {
            end_index = line_count;
            for (scan_index = index + 1u; scan_index < line_count;) {
                char addr_reg[32];
                char add_rs1[32];
                char add_rs2[32];
                char mem_reg[32];
                char mem_base[32];
                const char *index_reg = NULL;
                int mem_offset = 0;
                int materialized_stack_offset = 0;

                if (compiler_riscv_preview_line_is_label(lines[scan_index]) ||
                    compiler_riscv_preview_line_is_control_transfer_barrier(lines[scan_index]) ||
                    compiler_riscv_preview_line_defines_reg(lines[scan_index], "sp") ||
                    (compiler_riscv_preview_line_is_sw_sp_reg(
                            lines[scan_index],
                            mem_reg,
                            sizeof(mem_reg),
                            &mem_offset) &&
                        mem_offset == base_imm) ||
                    (scan_index + 1u < line_count &&
                        compiler_riscv_preview_two_line_is_sw_materialized_stack_slot(
                            lines,
                            scan_index,
                            &materialized_stack_offset) &&
                        materialized_stack_offset == base_imm) ||
                    compiler_riscv_preview_line_defines_reg(lines[scan_index], base_reg)) {
                    end_index = scan_index;
                    break;
                }
                if (scan_index + 1u < line_count &&
                    compiler_riscv_preview_line_is_add_regs(
                        lines[scan_index], addr_reg, sizeof(addr_reg), add_rs1, sizeof(add_rs1), add_rs2, sizeof(add_rs2))) {
                    if (strcmp(add_rs1, base_reg) == 0) {
                        index_reg = add_rs2;
                    } else if (strcmp(add_rs2, base_reg) == 0) {
                        index_reg = add_rs1;
                    }
                    if (index_reg &&
                        strcmp(index_reg, base_reg) != 0 &&
                        compiler_riscv_preview_line_is_lw_reg_offset(
                            lines[scan_index + 1u],
                            mem_reg,
                            sizeof(mem_reg),
                            &mem_offset,
                            mem_base,
                            sizeof(mem_base)) &&
                        mem_offset == 0 &&
                        strcmp(mem_base, addr_reg) == 0 &&
                        !compiler_riscv_preview_reg_is_used_again_before_redefinition(
                            lines, end_index, scan_index + 2u, addr_reg)) {
                        replaced_any = 1;
                        scan_index += 2u;
                        continue;
                    }
                    if (index_reg &&
                        strcmp(index_reg, base_reg) != 0 &&
                        compiler_riscv_preview_line_is_sw_reg_offset(
                            lines[scan_index + 1u],
                            mem_reg,
                            sizeof(mem_reg),
                            &mem_offset,
                            mem_base,
                            sizeof(mem_base)) &&
                        mem_offset == 0 &&
                        strcmp(mem_base, addr_reg) == 0 &&
                        !compiler_riscv_preview_reg_is_used_again_before_redefinition(
                            lines, end_index, scan_index + 2u, addr_reg)) {
                        replaced_any = 1;
                        scan_index += 2u;
                        continue;
                    }
                }
                if (compiler_riscv_preview_line_uses_reg(lines[scan_index], base_reg)) {
                    blocked = 1;
                    break;
                }
                ++scan_index;
            }

            if (!blocked && replaced_any) {
                for (scan_index = index + 1u; scan_index < end_index;) {
                    char addr_reg[32];
                    char add_rs1[32];
                    char add_rs2[32];
                    char mem_reg[32];
                    char mem_base[32];
                    const char *index_reg = NULL;
                    int mem_offset = 0;

                    if (scan_index + 1u < end_index &&
                        compiler_riscv_preview_line_is_add_regs(
                            lines[scan_index], addr_reg, sizeof(addr_reg), add_rs1, sizeof(add_rs1), add_rs2, sizeof(add_rs2))) {
                        if (strcmp(add_rs1, base_reg) == 0) {
                            index_reg = add_rs2;
                        } else if (strcmp(add_rs2, base_reg) == 0) {
                            index_reg = add_rs1;
                        }
                    }

                    if (index_reg &&
                        strcmp(index_reg, base_reg) != 0 &&
                        compiler_riscv_preview_line_is_lw_reg_offset(
                            lines[scan_index + 1u],
                            mem_reg,
                            sizeof(mem_reg),
                            &mem_offset,
                            mem_base,
                            sizeof(mem_base)) &&
                        mem_offset == 0 &&
                        strcmp(mem_base, addr_reg) == 0 &&
                        !compiler_riscv_preview_reg_is_used_again_before_redefinition(
                            lines, end_index, scan_index + 2u, addr_reg)) {
                        if (!compiler_builder_appendf(&builder, "  add %s, sp, %s\n", addr_reg, index_reg) ||
                            !compiler_builder_appendf(&builder, "  lw %s, %d(%s)\n", mem_reg, base_imm, addr_reg)) {
                            goto cleanup;
                        }
                        scan_index += 2u;
                        continue;
                    }
                    if (index_reg &&
                        strcmp(index_reg, base_reg) != 0 &&
                        compiler_riscv_preview_line_is_sw_reg_offset(
                            lines[scan_index + 1u],
                            mem_reg,
                            sizeof(mem_reg),
                            &mem_offset,
                            mem_base,
                            sizeof(mem_base)) &&
                        mem_offset == 0 &&
                        strcmp(mem_base, addr_reg) == 0 &&
                        !compiler_riscv_preview_reg_is_used_again_before_redefinition(
                            lines, end_index, scan_index + 2u, addr_reg)) {
                        if (!compiler_builder_appendf(&builder, "  add %s, sp, %s\n", addr_reg, index_reg) ||
                            !compiler_builder_appendf(&builder, "  sw %s, %d(%s)\n", mem_reg, base_imm, addr_reg)) {
                            goto cleanup;
                        }
                        scan_index += 2u;
                        continue;
                    }
                    if (!compiler_builder_appendf(&builder, "%s\n", lines[scan_index])) {
                        goto cleanup;
                    }
                    ++scan_index;
                }
                index = end_index;
                continue;
            }
        }

        if (!compiler_builder_appendf(&builder, "%s\n", lines[index])) {
            goto cleanup;
        }
        ++index;
    }

    free(*io_text);
    *io_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    free(lines);
    free(copy);
    return ok;
}

int compiler_test_optimize_riscv_preview_indexed_local_base_offsets(char **io_text) {
    return compiler_optimize_riscv_preview_indexed_local_base_offsets(io_text);
}

static int compiler_append_caller_save_sequence(
    CompilerStringBuilder *builder,
    size_t call_save_area_offset,
    int is_store,
    int include_a0) {
    static const char *const ordered_regs[] = {
        "t5",
        "a0",
        "a1", "a2", "a3", "a4", "a5", "a6", "a7",
        "t0", "t1", "t2", "t3", "t4",
        "t6",
    };
    size_t reg_index;

    if (!builder) {
        return 0;
    }

    for (reg_index = 0u; reg_index < sizeof(ordered_regs) / sizeof(ordered_regs[0]); ++reg_index) {
        const char *reg = ordered_regs[reg_index];
        size_t logical_index;
        const char *scratch;

        if (!include_a0 && strcmp(reg, "a0") == 0) {
            continue;
        }
        if (!include_a0 && strcmp(reg, "t5") == 0) {
            logical_index = 12u;
        } else if (!include_a0 && strcmp(reg, "t6") == 0) {
            logical_index = 13u;
        } else if (include_a0 && strcmp(reg, "t5") == 0) {
            logical_index = 13u;
        } else if (include_a0 && strcmp(reg, "t6") == 0) {
            logical_index = 14u;
        } else if (strcmp(reg, "a0") == 0) {
            logical_index = 0u;
        } else if (reg[0] == 'a') {
            logical_index = include_a0
                ? (size_t)(reg[1] - '0')
                : (size_t)(reg[1] - '1');
        } else {
            logical_index = (size_t)(reg[1] - '0') + (include_a0 ? 8u : 7u);
        }

        if (strcmp(reg, "t6") == 0) {
            scratch = include_a0 ? "t5" : "a0";
        } else {
            scratch = "t6";
        }
        if (!compiler_append_stack_access(builder,
                is_store ? "sw" : "lw",
                reg,
                "s11",
                call_save_area_offset + (logical_index * 4u),
                scratch)) {
            return 0;
        }
    }
    return 1;
}

static int compiler_append_riscv_preview_instruction(
    CompilerStringBuilder *builder,
    const MachineBytesReport *report,
    const CompilerRiscvPreviewCache *cache,
    size_t program_byte_offset,
    uint32_t word,
    size_t epilogue_stack_size,
    size_t epilogue_ra_offset,
    int epilogue_restores_ra,
    size_t call_save_area_offset,
    int save_caller_regs_around_call) {
    uint32_t opcode = word & 0x7Fu;
    uint32_t rd = (word >> 7u) & 0x1Fu;
    uint32_t funct3 = (word >> 12u) & 0x7u;
    uint32_t rs1 = (word >> 15u) & 0x1Fu;
    uint32_t rs2 = (word >> 20u) & 0x1Fu;
    uint32_t funct7 = (word >> 25u) & 0x7Fu;
    const MachineBytesFixupSummary *fixup = compiler_find_fixup_at_patch_offset_cached(cache, program_byte_offset);
    const char *target_name = NULL;

    (void)call_save_area_offset;
    (void)save_caller_regs_around_call;

    switch (opcode) {
        case 0x03u:
            if (funct3 == 0x2u) {
                const MachineBytesFixupSummary *load_fixup =
                    compiler_find_fixup_at_patch_offset_and_kind_cached(
                        cache, program_byte_offset, MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET);

                if (load_fixup &&
                    load_fixup->target_name && load_fixup->target_name[0] != '\0') {
                    return compiler_builder_appendf(
                        builder,
                        "  lw %s, %%lo(%s)(%s)\n",
                        compiler_riscv_register_name(rd),
                        load_fixup->target_name,
                        compiler_riscv_register_name(rs1));
                }
                {
                    const char *global_name = report
                        ? compiler_find_global_name_for_gp_offset(
                              &report->program, compiler_riscv_decode_i_imm(word))
                        : NULL;

                    if (rs1 == 3u && global_name) {
                        return compiler_builder_appendf(
                            builder,
                            "  lw %s, %d(gp) # %s\n",
                            compiler_riscv_register_name(rd),
                            compiler_riscv_decode_i_imm(word),
                            global_name);
                    }
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
            const MachineBytesFixupSummary *load_fixup =
                compiler_find_fixup_at_patch_offset_and_kind_cached(
                    cache, program_byte_offset, MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET);

            if (load_fixup &&
                load_fixup->target_name && load_fixup->target_name[0] != '\0' &&
                funct3 == 0x0u) {
                return compiler_builder_appendf(
                    builder,
                    "  addi %s, %s, %%lo(%s)\n",
                    compiler_riscv_register_name(rd),
                    compiler_riscv_register_name(rs1),
                    load_fixup->target_name);
            }

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
                        if (rd == rs1) {
                            return 1;
                        }
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
                const MachineBytesFixupSummary *store_fixup =
                    compiler_find_fixup_at_patch_offset_and_kind_cached(
                        cache, program_byte_offset, MACHINE_BYTES_FIXUP_DATA_STORE_TARGET);

                if (store_fixup &&
                    store_fixup->target_name && store_fixup->target_name[0] != '\0') {
                    return compiler_builder_appendf(
                        builder,
                        "  sw %s, %%lo(%s)(%s)\n",
                        compiler_riscv_register_name(rs2),
                        store_fixup->target_name,
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
            {
                const MachineBytesFixupSummary *addr_fixup =
                    compiler_find_fixup_at_patch_offset_and_kind_cached(
                        cache, program_byte_offset, MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET);

                if (addr_fixup &&
                    addr_fixup->target_name && addr_fixup->target_name[0] != '\0') {
                    return compiler_builder_appendf(
                        builder,
                        "  lui %s, %%hi(%s)\n",
                        compiler_riscv_register_name(rd),
                        addr_fixup->target_name);
                }
            }
            return compiler_builder_appendf(
                builder,
                "  lui %s, 0x%x\n",
                compiler_riscv_register_name(rd),
                (unsigned)(word >> 12u));
        case 0x17u:
            if (fixup &&
                fixup->kind == MACHINE_BYTES_FIXUP_CALL_TARGET &&
                compiler_should_collapse_call_fixup(report, fixup, save_caller_regs_around_call)) {
                if (!compiler_builder_appendf(builder, "  call ") ||
                    !compiler_append_pretty_symbol_name(builder, report, fixup->target_name) ||
                    !compiler_builder_appendf(builder, "\n")) {
                    return 0;
                }
                return 1;
            }
            return compiler_builder_appendf(
                builder,
                "  auipc %s, 0x%x\n",
                compiler_riscv_register_name(rd),
                (unsigned)(word >> 12u));
        case 0x63u: {
            const CompilerBlockLabelCacheEntry *cached_label = NULL;
            const char *mnemonic = NULL;
            int32_t imm = compiler_riscv_decode_b_imm(word);

            if (fixup && fixup->target_name && fixup->target_name[0] != '\0') {
                target_name = fixup->target_name;
            } else {
                cached_label = compiler_find_block_label_at_program_byte_offset_cached(
                    cache, program_byte_offset + (size_t)imm);
                if (!cached_label) {
                    target_name = compiler_find_label_at_program_byte_offset(report, program_byte_offset + (size_t)imm);
                }
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
            if (mnemonic && (target_name || cached_label)) {
                if (!compiler_builder_appendf(
                        builder,
                        "  %s %s, %s, ",
                        mnemonic,
                        compiler_riscv_register_name(rs1),
                        compiler_riscv_register_name(rs2)) ||
                    !((cached_label &&
                        compiler_builder_appendf(builder, ".L%s_%zu", cached_label->function_name, cached_label->block_index)) ||
                        compiler_append_pretty_symbol_name(builder, report, target_name)) ||
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
                if (epilogue_stack_size > 0u) {
                    if (epilogue_restores_ra) {
                        return compiler_append_stack_access(builder, "lw", "s11", "sp", epilogue_ra_offset - 4u, "t6") &&
                            compiler_append_stack_access(builder, "lw", "ra", "sp", epilogue_ra_offset, "t6") &&
                            compiler_append_stack_adjust(builder, (long long)epilogue_stack_size) &&
                            compiler_builder_appendf(builder, "  ret\n");
                    }
                    return compiler_append_stack_adjust(builder, (long long)epilogue_stack_size) &&
                        compiler_builder_appendf(builder, "  ret\n");
                }
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
            const CompilerBlockLabelCacheEntry *cached_label = NULL;
            int32_t imm = compiler_riscv_decode_j_imm(word);

            if (fixup && fixup->target_name && fixup->target_name[0] != '\0') {
                target_name = fixup->target_name;
            } else {
                cached_label = compiler_find_block_label_at_program_byte_offset_cached(
                    cache, program_byte_offset + (size_t)imm);
                if (!cached_label) {
                    target_name = compiler_find_label_at_program_byte_offset(report, program_byte_offset + (size_t)imm);
                }
            }
            if (rd == 0u) {
                if (target_name || cached_label) {
                    if (!compiler_builder_appendf(builder, "  j ") ||
                        !((cached_label &&
                            compiler_builder_appendf(
                                builder, ".L%s_%zu", cached_label->function_name, cached_label->block_index)) ||
                            compiler_append_pretty_symbol_name(builder, report, target_name)) ||
                        !compiler_builder_appendf(builder, "\n")) {
                        return 0;
                    }
                    return 1;
                }
                return compiler_builder_appendf(builder, "  jal zero, %d\n", imm);
            }
            if (target_name || cached_label) {
                if (!compiler_builder_appendf(builder, "  jal %s, ", compiler_riscv_register_name(rd)) ||
                    !((cached_label &&
                        compiler_builder_appendf(builder, ".L%s_%zu", cached_label->function_name, cached_label->block_index)) ||
                        compiler_append_pretty_symbol_name(builder, report, target_name)) ||
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

static int compiler_build_riscv_preview_bytes_report_from_machine_ir_report(
    const MachineIrAllocateRewriteReport *report,
    CompilerMode mode,
    MachineBytesReport *bytes_report,
    CompilerError *error) {
    MachineSelectProgram select_program;
    MachineLayoutProgram layout_program;
    MachineEmitProgram emit_program;
    MachineSelectError select_error;
    MachineLayoutError layout_error;
    MachineEmitError emit_error;
    MachineBytesError bytes_error;
    int ok = 0;

    if (!report || !bytes_report) {
        compiler_set_error(error, 0, 0, "COMPILER-109: invalid riscv bytes export contract");
        return 0;
    }

    memset(&select_error, 0, sizeof(select_error));
    memset(&layout_error, 0, sizeof(layout_error));
    memset(&emit_error, 0, sizeof(emit_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);
    machine_emit_program_init(&emit_program);
    machine_bytes_report_init(bytes_report);

    ok = machine_select_build_program_from_machine_ir_report(report, &select_program, &select_error);
    if (!ok) {
        compiler_copy_stage_error(error, select_error.line, select_error.column, select_error.message);
        goto cleanup;
    }
    ok = machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error);
    if (!ok) {
        compiler_copy_stage_error(error, layout_error.line, layout_error.column, layout_error.message);
        goto cleanup;
    }
    ok = machine_emit_lower_program_from_machine_layout(&layout_program, &emit_program, &emit_error);
    if (!ok) {
        compiler_copy_stage_error(error, emit_error.line, emit_error.column, emit_error.message);
        goto cleanup;
    }
    ok = machine_bytes_build_report_from_machine_emit_program_with_profile(
        &emit_program,
        compiler_backend_profile_for_mode(mode),
        bytes_report,
        &bytes_error);
    if (!ok) {
        compiler_copy_stage_error(error, bytes_error.line, bytes_error.column, bytes_error.message);
        goto cleanup;
    }

cleanup:
    machine_emit_program_free(&emit_program);
    machine_layout_program_free(&layout_program);
    machine_select_program_free(&select_program);
    if (!ok) {
        machine_bytes_report_free(bytes_report);
    }
    return ok;
}

int compiler_emit_riscv_preview_text_from_report(const MachineIrAllocateRewriteReport *report,
    CompilerMode mode,
    char **out_text,
    CompilerError *error) {
    MachineBytesReport bytes_report;
    CompilerRiscvPreviewCache preview_cache;
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

    compiler_riscv_preview_cache_init(&preview_cache);
    compiler_builder_init(&builder);

    ok = compiler_build_riscv_preview_bytes_report_from_machine_ir_report(report, mode, &bytes_report, error);
    if (!ok) {
        goto cleanup;
    }
    if (!compiler_riscv_preview_cache_build(&bytes_report, &preview_cache)) {
        compiler_set_error(error, 0, 0, "COMPILER-110: failed to build riscv preview lookup cache");
        ok = 0;
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
        (void)block_count;
        (void)spill_slot_count;
    }
    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        const MachineBytesFunction *function = NULL;
        const char *function_name = NULL;
        int has_body = 0;
        size_t parameter_count = 0u;
        size_t local_count = 0u;
        size_t block_count = 0u;
        size_t spill_slot_count = 0u;
        const MachineBytesFunctionSummary *function_summary = NULL;
        size_t frame_bytes = 0u;
        size_t local_storage_bytes = 0u;
        size_t ra_save_offset = 0u;
        size_t s11_save_offset = 0u;
        size_t call_save_area_offset = 0u;
        int frame_restores_ra = 0;
        int save_caller_regs_around_call = 0;
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
        if (!machine_bytes_report_get_function_summary_artifact(&bytes_report, function_index, &function_summary) ||
            !function_summary) {
            compiler_set_error(error, 0, 0, "COMPILER-116: missing bytes function summary artifact");
            ok = 0;
            goto cleanup;
        }
        if (!has_body || !function_name || function_name[0] == '\0') {
            continue;
        }
        local_storage_bytes = (local_count + spill_slot_count) * 4u;
        frame_bytes = local_storage_bytes > parameter_count * 4u
            ? local_storage_bytes
            : parameter_count * 4u;
        if (function_summary->call_count > 0u) {
            if (compiler_use_caller_save_text_enabled()) {
                frame_bytes += compiler_preview_caller_save_area_size();
                save_caller_regs_around_call = 1;
            }
            frame_bytes += 4u;   /* ra */
            frame_bytes += 4u;   /* s11 */
            frame_restores_ra = 1;
        }
        if (frame_bytes > 0u && (frame_bytes % 16u) != 0u) {
            frame_bytes = ((frame_bytes + 15u) / 16u) * 16u;
        }
        if (function_summary->call_count > 0u) {
            ra_save_offset = frame_bytes - 4u;
            s11_save_offset = frame_bytes - 8u;
            call_save_area_offset = local_storage_bytes > parameter_count * 4u
                ? local_storage_bytes
                : parameter_count * 4u;
        }

        if (!compiler_builder_appendf(&builder, ".globl %s\n.type %s, @function\n%s:\n", function_name, function_name, function_name)) {
            compiler_set_error(error, 0, 0, "COMPILER-117: out of memory writing function header");
            ok = 0;
            goto cleanup;
        }
        if (frame_bytes > 0u) {
            size_t param_index;

            if (!compiler_append_stack_adjust(&builder, -(long long)frame_bytes)) {
                compiler_set_error(error, 0, 0, "COMPILER-117: out of memory writing function prologue");
                ok = 0;
                goto cleanup;
            }
            if (function_summary->call_count > 0u &&
                (!compiler_append_stack_access(&builder, "sw", "ra", "sp", ra_save_offset, "t6") ||
                    !compiler_append_stack_access(&builder, "sw", "s11", "sp", s11_save_offset, "t6") ||
                    !compiler_builder_appendf(&builder, "  mv s11, sp\n"))) {
                compiler_set_error(error, 0, 0, "COMPILER-117: out of memory writing function prologue");
                ok = 0;
                goto cleanup;
            }

            for (param_index = 0u; param_index < parameter_count; ++param_index) {
                if (param_index < 8u) {
                    if (!compiler_append_stack_access(
                            &builder,
                            "sw",
                            compiler_riscv_register_name((uint32_t)(10u + param_index)),
                            "sp",
                            param_index * 4u,
                            "t6")) {
                        compiler_set_error(error, 0, 0, "COMPILER-117: out of memory writing function prologue");
                        ok = 0;
                        goto cleanup;
                    }
                } else {
                    size_t caller_stack_offset = frame_bytes + ((param_index - 8u) * 4u);

                    if (!compiler_append_stack_access(&builder, "lw", "t5", "sp", caller_stack_offset, "t6") ||
                        !compiler_append_stack_access(&builder, "sw", "t5", "sp", param_index * 4u, "t6")) {
                        compiler_set_error(error, 0, 0, "COMPILER-117: out of memory writing function prologue");
                        ok = 0;
                        goto cleanup;
                    }
                }
            }
        }

        for (block_index = 0u; block_index < block_count; ++block_index) {
            const MachineBytesBlock *block = NULL;
            MachineBytesBlockSummary block_summary;
            const unsigned char *block_bytes = NULL;
            size_t block_byte_count = 0u;
            size_t function_start = 0u;
            size_t byte_offset;
            size_t skip_until_program_byte_offset = (size_t)-1;

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
                if (!compiler_builder_appendf(&builder, ".L%s_%zu", function_name, block_index) ||
                    !compiler_builder_appendf(&builder, ":\n")) {
                    compiler_set_error(error, 0, 0, "COMPILER-120: out of memory writing block label");
                    ok = 0;
                    goto cleanup;
                }
            }
            for (byte_offset = 0u; byte_offset < block_byte_count; byte_offset += 4u) {
                uint32_t word = compiler_read_u32_le(block_bytes + byte_offset);
                size_t program_byte_offset = function_start + block_summary.start_byte_offset + byte_offset;
                const MachineBytesFixupSummary *call_fixup = NULL;
                const MachineBytesFixupSummary *current_fixup = NULL;
                const CompilerBlockLabelCacheEntry *tail_call_cached_label = NULL;
                const char *tail_call_target_name = NULL;
                int include_a0 = 0;
                size_t tail_call_skip_byte_count = 0u;

                if (skip_until_program_byte_offset != (size_t)-1 &&
                    program_byte_offset < skip_until_program_byte_offset) {
                    continue;
                }

                if (save_caller_regs_around_call) {
                    call_fixup = compiler_find_call_fixup_covering_offset_cached(&preview_cache, program_byte_offset);
                    if (call_fixup && program_byte_offset == call_fixup->owner_byte_offset) {
                        include_a0 = compiler_call_requires_a0_preservation(
                            &bytes_report,
                            function_index,
                            program_byte_offset);
                    }
                    (void)block;
                }
                current_fixup = compiler_find_fixup_at_patch_offset_cached(&preview_cache, program_byte_offset);
                if (compiler_riscv_preview_detect_tail_call_epilogue(
                        block_bytes,
                        block_byte_count,
                        byte_offset,
                        word,
                        frame_bytes,
                        ra_save_offset,
                        frame_restores_ra,
                        &tail_call_skip_byte_count)) {
                    if (current_fixup &&
                        current_fixup->kind == MACHINE_BYTES_FIXUP_CALL_TARGET &&
                        current_fixup->target_name && current_fixup->target_name[0] != '\0') {
                        tail_call_target_name = current_fixup->target_name;
                    } else if ((word & 0x7Fu) == 0x6Fu && ((word >> 7u) & 0x1Fu) == 1u) {
                        int32_t imm = compiler_riscv_decode_j_imm(word);
                        size_t target_program_byte_offset = program_byte_offset + (size_t)imm;

                        tail_call_cached_label = compiler_find_block_label_at_program_byte_offset_cached(
                            &preview_cache, target_program_byte_offset);
                        if (!tail_call_cached_label) {
                            tail_call_target_name = compiler_find_label_at_program_byte_offset(
                                &bytes_report, target_program_byte_offset);
                        }
                    }
                }
                if ((tail_call_target_name || tail_call_cached_label) &&
                    ((word & 0x7Fu) == 0x6Fu) && (((word >> 7u) & 0x1Fu) == 1u) &&
                    compiler_riscv_preview_detect_tail_call_epilogue(
                        block_bytes,
                        block_byte_count,
                        byte_offset,
                        word,
                        frame_bytes,
                        ra_save_offset,
                        frame_restores_ra,
                        &tail_call_skip_byte_count)) {
                    if (!compiler_append_riscv_preview_tail_call(
                            &builder,
                            &bytes_report,
                            tail_call_target_name,
                            tail_call_cached_label,
                            frame_bytes,
                            ra_save_offset)) {
                        compiler_set_error(error, 0, 0, "COMPILER-121: out of memory writing tail call");
                        ok = 0;
                        goto cleanup;
                    }
                    skip_until_program_byte_offset = program_byte_offset + tail_call_skip_byte_count;
                    continue;
                }
                if (call_fixup && program_byte_offset == call_fixup->owner_byte_offset &&
                    !compiler_append_caller_save_sequence(&builder, call_save_area_offset, 1, include_a0)) {
                    compiler_set_error(error, 0, 0, "COMPILER-121: out of memory writing caller-save prologue");
                    ok = 0;
                    goto cleanup;
                }

                if (!compiler_append_riscv_preview_instruction(
                        &builder,
                        &bytes_report,
                        &preview_cache,
                        program_byte_offset,
                        word,
                        frame_bytes,
                        ra_save_offset,
                        frame_restores_ra,
                        call_save_area_offset,
                        save_caller_regs_around_call)) {
                    compiler_set_error(error, 0, 0, "COMPILER-121: out of memory writing riscv instruction");
                    ok = 0;
                    goto cleanup;
                }
                if (current_fixup &&
                    current_fixup->kind == MACHINE_BYTES_FIXUP_CALL_TARGET &&
                    compiler_should_collapse_call_fixup(
                        &bytes_report,
                        current_fixup,
                        save_caller_regs_around_call) &&
                    (word & 0x7Fu) == 0x17u) {
                    skip_until_program_byte_offset = program_byte_offset + 8u;
                } else {
                    skip_until_program_byte_offset = (size_t)-1;
                }
                if (compiler_is_call_restore_point(
                        call_fixup,
                        program_byte_offset,
                        save_caller_regs_around_call) &&
                    !compiler_append_caller_save_sequence(&builder, call_save_area_offset, 0, include_a0)) {
                    compiler_set_error(error, 0, 0, "COMPILER-121: out of memory writing caller-save epilogue");
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
    if (compiler_use_final_text_peepholes_enabled() &&
        (!compiler_optimize_riscv_preview_tail_calls(out_text) ||
            !compiler_optimize_riscv_preview_zero_adds(out_text) ||
            !compiler_optimize_riscv_preview_reuse_repeated_lui_addi_constants(out_text) ||
            !compiler_optimize_riscv_preview_sub_by_one(out_text) ||
            !compiler_optimize_riscv_preview_mul_by_four(out_text) ||
            !compiler_optimize_riscv_preview_stack_addr_reuse(out_text) ||
            !compiler_optimize_riscv_preview_repeated_indexed_addr_triples(out_text) ||
            !compiler_optimize_riscv_preview_repeated_indexed_addr_sequences(out_text) ||
            !compiler_optimize_riscv_preview_call_arg_load_swaps(out_text))) {
        compiler_set_error(error, 0, 0, "COMPILER-123: out of memory optimizing tail calls");
        ok = 0;
        goto cleanup;
    }
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    compiler_riscv_preview_cache_free(&preview_cache);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

static int compiler_simple_machine_ir_register_name(size_t machine_register_id, const char **out_name) {
    if (out_name) {
        *out_name = NULL;
    }
    if (!out_name || machine_register_id >= 8u) {
        return 0;
    }
    *out_name = compiler_riscv_register_name((uint32_t)(10u + machine_register_id));
    return 1;
}

static size_t compiler_simple_machine_ir_local_offset(const MachineIrFunction *function, size_t local_id) {
    (void)function;
    return local_id * 4u;
}

static size_t compiler_simple_machine_ir_spill_offset(const MachineIrFunction *function, size_t spill_slot) {
    return (function->local_count + spill_slot) * 4u;
}

static int compiler_simple_machine_ir_emit_global_sections(
    CompilerStringBuilder *builder,
    const MachineIrProgram *program) {
    size_t global_index;
    int wrote_bss = 0;
    int wrote_data = 0;

    if (!builder || !program) {
        return 0;
    }

    for (global_index = 0u; global_index < program->global_count; ++global_index) {
        const MachineIrGlobal *global = &program->globals[global_index];
        const char *name = global->name;
        size_t byte_size = global->byte_size > 0u ? global->byte_size : 4u;

        if (!name || name[0] == '\0') {
            continue;
        }

        if (global->has_initializer && !global->has_runtime_initializer && byte_size == 4u) {
            if (!wrote_data) {
                wrote_data = 1;
                if (!compiler_builder_appendf(builder, ".data\n.p2align 2\n")) {
                    return 0;
                }
            }
            if (!compiler_builder_appendf(
                    builder,
                    ".globl %s\n%s:\n  .word %lld\n",
                    name,
                    name,
                    global->initializer_value)) {
                return 0;
            }
        } else {
            if (!wrote_bss) {
                wrote_bss = 1;
                if (!compiler_builder_appendf(builder, ".bss\n.p2align 2\n")) {
                    return 0;
                }
            }
            if (!compiler_builder_appendf(
                    builder,
                    ".globl %s\n%s:\n  .zero %zu\n",
                    name,
                    name,
                    byte_size)) {
                return 0;
            }
        }
    }

    return 1;
}

static int compiler_simple_machine_ir_emit_operand_to_reg(
    CompilerStringBuilder *builder,
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    MachineIrOperand operand,
    const char *target_reg) {
    const char *source_reg = NULL;

    if (!builder || !program || !function || !target_reg) {
        return 0;
    }

    switch (operand.kind) {
        case MACHINE_IR_OPERAND_IMMEDIATE:
            return compiler_builder_appendf(builder, "  li %s, %lld\n", target_reg, operand.immediate);
        case MACHINE_IR_OPERAND_REGISTER:
            if (!compiler_simple_machine_ir_register_name(operand.machine_register_id, &source_reg) || !source_reg) {
                return 0;
            }
            if (strcmp(source_reg, target_reg) == 0) {
                return 1;
            }
            return compiler_builder_appendf(builder, "  mv %s, %s\n", target_reg, source_reg);
        case MACHINE_IR_OPERAND_SPILL_SLOT:
            return compiler_append_stack_access(
                builder,
                "lw",
                target_reg,
                "s11",
                compiler_simple_machine_ir_spill_offset(function, operand.spill_slot),
                "t3");
        default:
            return 0;
    }
}

static int compiler_simple_machine_ir_store_reg_to_operand(
    CompilerStringBuilder *builder,
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    const char *source_reg,
    MachineIrOperand operand) {
    const char *target_reg = NULL;

    if (!builder || !program || !function || !source_reg) {
        return 0;
    }

    switch (operand.kind) {
        case MACHINE_IR_OPERAND_REGISTER:
            if (!compiler_simple_machine_ir_register_name(operand.machine_register_id, &target_reg) || !target_reg) {
                return 0;
            }
            if (strcmp(target_reg, source_reg) == 0) {
                return 1;
            }
            return compiler_builder_appendf(builder, "  mv %s, %s\n", target_reg, source_reg);
        case MACHINE_IR_OPERAND_SPILL_SLOT:
            return compiler_append_stack_access(
                builder,
                "sw",
                source_reg,
                "s11",
                compiler_simple_machine_ir_spill_offset(function, operand.spill_slot),
                "t3");
        default:
            return 0;
    }
}

static int compiler_simple_machine_ir_emit_binary(
    CompilerStringBuilder *builder,
    MachineIrBinaryOp op,
    const char *dst_reg,
    const char *lhs_reg,
    const char *rhs_reg) {
    if (!builder || !dst_reg || !lhs_reg || !rhs_reg) {
        return 0;
    }

    switch (op) {
        case MACHINE_IR_BINARY_ADD:
            return compiler_builder_appendf(builder, "  add %s, %s, %s\n", dst_reg, lhs_reg, rhs_reg);
        case MACHINE_IR_BINARY_SUB:
            return compiler_builder_appendf(builder, "  sub %s, %s, %s\n", dst_reg, lhs_reg, rhs_reg);
        case MACHINE_IR_BINARY_MUL:
            return compiler_builder_appendf(builder, "  mul %s, %s, %s\n", dst_reg, lhs_reg, rhs_reg);
        case MACHINE_IR_BINARY_DIV:
            return compiler_builder_appendf(builder, "  div %s, %s, %s\n", dst_reg, lhs_reg, rhs_reg);
        case MACHINE_IR_BINARY_MOD:
            return compiler_builder_appendf(builder, "  rem %s, %s, %s\n", dst_reg, lhs_reg, rhs_reg);
        case MACHINE_IR_BINARY_BIT_AND:
            return compiler_builder_appendf(builder, "  and %s, %s, %s\n", dst_reg, lhs_reg, rhs_reg);
        case MACHINE_IR_BINARY_BIT_XOR:
            return compiler_builder_appendf(builder, "  xor %s, %s, %s\n", dst_reg, lhs_reg, rhs_reg);
        case MACHINE_IR_BINARY_BIT_OR:
            return compiler_builder_appendf(builder, "  or %s, %s, %s\n", dst_reg, lhs_reg, rhs_reg);
        case MACHINE_IR_BINARY_SHIFT_LEFT:
            return compiler_builder_appendf(builder, "  sll %s, %s, %s\n", dst_reg, lhs_reg, rhs_reg);
        case MACHINE_IR_BINARY_SHIFT_RIGHT:
            return compiler_builder_appendf(builder, "  sra %s, %s, %s\n", dst_reg, lhs_reg, rhs_reg);
        case MACHINE_IR_BINARY_EQ:
            return compiler_builder_appendf(builder, "  sub %s, %s, %s\n  seqz %s, %s\n", dst_reg, lhs_reg, rhs_reg, dst_reg, dst_reg);
        case MACHINE_IR_BINARY_NE:
            return compiler_builder_appendf(builder, "  sub %s, %s, %s\n  snez %s, %s\n", dst_reg, lhs_reg, rhs_reg, dst_reg, dst_reg);
        case MACHINE_IR_BINARY_LT:
            return compiler_builder_appendf(builder, "  slt %s, %s, %s\n", dst_reg, lhs_reg, rhs_reg);
        case MACHINE_IR_BINARY_LE:
            return compiler_builder_appendf(builder, "  slt %s, %s, %s\n  xori %s, %s, 1\n", dst_reg, rhs_reg, lhs_reg, dst_reg, dst_reg);
        case MACHINE_IR_BINARY_GT:
            return compiler_builder_appendf(builder, "  slt %s, %s, %s\n", dst_reg, rhs_reg, lhs_reg);
        case MACHINE_IR_BINARY_GE:
            return compiler_builder_appendf(builder, "  slt %s, %s, %s\n  xori %s, %s, 1\n", dst_reg, lhs_reg, rhs_reg, dst_reg, dst_reg);
        default:
            return 0;
    }
}

static int compiler_simple_machine_ir_function_has_call(
    const MachineIrFunction *function,
    size_t *out_max_call_arg_count) {
    size_t block_index;
    size_t max_call_arg_count = 0u;
    int has_call = 0;

    if (!function) {
        return 0;
    }

    for (block_index = 0u; block_index < function->block_count; ++block_index) {
        const MachineIrBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0u; instruction_index < block->instruction_count; ++instruction_index) {
            const MachineIrInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == MACHINE_IR_INSTR_CALL) {
                has_call = 1;
                if (instruction->as.call.arg_count > max_call_arg_count) {
                    max_call_arg_count = instruction->as.call.arg_count;
                }
            }
        }
    }

    if (out_max_call_arg_count) {
        *out_max_call_arg_count = max_call_arg_count;
    }
    return has_call;
}

static int compiler_emit_riscv_preview_text_from_machine_ir_program_simple(
    const MachineIrProgram *program,
    char **out_text,
    CompilerError *error) {
    CompilerStringBuilder builder;
    size_t function_index;
    int has_main = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        compiler_set_error(error, 0, 0, "COMPILER-126: invalid simple machine-ir text contract");
        return 0;
    }

    compiler_builder_init(&builder);
    if (!compiler_builder_appendf(&builder, ".attribute arch, \"rv32im\"\n") ||
        !compiler_simple_machine_ir_emit_global_sections(&builder, program) ||
        !compiler_builder_appendf(&builder, ".text\n.p2align 2\n\n")) {
        compiler_builder_free(&builder);
        compiler_set_error(error, 0, 0, "COMPILER-127: failed to start simple machine-ir text");
        return 0;
    }

    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        const MachineIrFunction *function = &program->functions[function_index];
        size_t block_index;
        size_t max_call_arg_count = 0u;
        compiler_simple_machine_ir_function_has_call(function, &max_call_arg_count);
        size_t outgoing_arg_bytes = max_call_arg_count > 8u ? (max_call_arg_count - 8u) * 4u : 0u;
        size_t local_storage_bytes = (function->local_count + function->spill_slot_count) * 4u;
        size_t frame_bytes = local_storage_bytes > function->parameter_count * 4u
            ? local_storage_bytes
            : function->parameter_count * 4u;
        size_t ra_save_offset;
        size_t s11_save_offset;

        if (!function->has_body || !function->name || function->name[0] == '\0') {
            continue;
        }
        if (strcmp(function->name, "main") == 0) {
            has_main = 1;
        }

        frame_bytes += 8u;
        if ((frame_bytes % 16u) != 0u) {
            frame_bytes = ((frame_bytes + 15u) / 16u) * 16u;
        }
        ra_save_offset = frame_bytes - 4u;
        s11_save_offset = frame_bytes - 8u;

        if (!compiler_builder_appendf(
                &builder,
                ".globl %s\n.type %s, @function\n%s:\n",
                function->name,
                function->name,
                function->name) ||
            !compiler_append_stack_adjust(&builder, -(long long)frame_bytes) ||
            !compiler_append_stack_access(&builder, "sw", "ra", "sp", ra_save_offset, "t6") ||
            !compiler_append_stack_access(&builder, "sw", "s11", "sp", s11_save_offset, "t6") ||
            !compiler_builder_appendf(&builder, "  mv s11, sp\n")) {
            compiler_builder_free(&builder);
            compiler_set_error(error, 0, 0, "COMPILER-128: failed to emit simple function prologue");
            return 0;
        }

        {
            size_t param_index;

            for (param_index = 0u; param_index < function->parameter_count; ++param_index) {
                size_t offset = compiler_simple_machine_ir_local_offset(function, param_index);

                if (param_index < 8u) {
                    if (!compiler_append_stack_access(
                            &builder,
                            "sw",
                            compiler_riscv_register_name((uint32_t)(10u + param_index)),
                            "s11",
                            offset,
                            "t6")) {
                        compiler_builder_free(&builder);
                        compiler_set_error(error, 0, 0, "COMPILER-129: failed to spill simple parameters");
                        return 0;
                    }
                } else {
                    size_t caller_stack_offset = frame_bytes + ((param_index - 8u) * 4u);

                    if (!compiler_append_stack_access(&builder, "lw", "t5", "s11", caller_stack_offset, "t6") ||
                        !compiler_append_stack_access(&builder, "sw", "t5", "s11", offset, "t6")) {
                        compiler_builder_free(&builder);
                        compiler_set_error(error, 0, 0, "COMPILER-130: failed to load simple stack parameters");
                        return 0;
                    }
                }
            }
        }

        for (block_index = 0u; block_index < function->block_count; ++block_index) {
            const MachineIrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            if (!compiler_builder_appendf(&builder, ".L%s_%zu:\n", function->name, block->id)) {
                compiler_builder_free(&builder);
                compiler_set_error(error, 0, 0, "COMPILER-131: failed to emit simple block label");
                return 0;
            }

            for (instruction_index = 0u; instruction_index < block->instruction_count; ++instruction_index) {
                const MachineIrInstruction *instruction = &block->instructions[instruction_index];

                switch (instruction->kind) {
                    case MACHINE_IR_INSTR_MOV:
                        if (!compiler_simple_machine_ir_emit_operand_to_reg(
                                &builder, program, function, instruction->as.mov_value, "t4") ||
                            !compiler_simple_machine_ir_store_reg_to_operand(
                                &builder, program, function, "t4", instruction->result)) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-132: unsupported simple mov emission");
                            return 0;
                        }
                        break;
                    case MACHINE_IR_INSTR_BINARY:
                        if (!compiler_simple_machine_ir_emit_operand_to_reg(
                                &builder, program, function, instruction->as.binary.lhs, "t5") ||
                            !compiler_simple_machine_ir_emit_operand_to_reg(
                                &builder, program, function, instruction->as.binary.rhs, "t6") ||
                            !compiler_simple_machine_ir_emit_binary(
                                &builder, instruction->as.binary.op, "t4", "t5", "t6") ||
                            !compiler_simple_machine_ir_store_reg_to_operand(
                                &builder, program, function, "t4", instruction->result)) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-133: unsupported simple binary emission");
                            return 0;
                        }
                        break;
                    case MACHINE_IR_INSTR_CALL: {
                        size_t arg_index;

                        if (outgoing_arg_bytes > 0u &&
                            !compiler_append_stack_adjust(&builder, -(long long)outgoing_arg_bytes)) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-134: failed to reserve simple call arg area");
                            return 0;
                        }
                        for (arg_index = 0u; arg_index < instruction->as.call.arg_count; ++arg_index) {
                            if (!compiler_simple_machine_ir_emit_operand_to_reg(
                                    &builder, program, function, instruction->as.call.args[arg_index], "t5")) {
                                compiler_builder_free(&builder);
                                compiler_set_error(error, 0, 0, "COMPILER-135: failed to materialize simple call arg");
                                return 0;
                            }
                            if (arg_index < 8u) {
                                if (!compiler_builder_appendf(
                                        &builder,
                                        "  mv %s, t5\n",
                                        compiler_riscv_register_name((uint32_t)(10u + arg_index)))) {
                                    compiler_builder_free(&builder);
                                    compiler_set_error(error, 0, 0, "COMPILER-136: failed to move simple call arg");
                                    return 0;
                                }
                            } else {
                                if (!compiler_append_stack_access(
                                        &builder, "sw", "t5", "sp", (arg_index - 8u) * 4u, "t6")) {
                                    compiler_builder_free(&builder);
                                    compiler_set_error(error, 0, 0, "COMPILER-137: failed to stage simple stack arg");
                                    return 0;
                                }
                            }
                        }
                        if (!compiler_builder_appendf(
                                &builder,
                                "  call %s\n",
                                instruction->as.call.callee_name ? instruction->as.call.callee_name : "<null>")) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-138: failed to emit simple call");
                            return 0;
                        }
                        if (outgoing_arg_bytes > 0u &&
                            !compiler_append_stack_adjust(&builder, (long long)outgoing_arg_bytes)) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-139: failed to release simple call arg area");
                            return 0;
                        }
                        if (instruction->has_result &&
                            !compiler_simple_machine_ir_store_reg_to_operand(
                                &builder, program, function, "a0", instruction->result)) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-140: failed to store simple call result");
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_IR_INSTR_ADDR_LOCAL: {
                        size_t offset = compiler_simple_machine_ir_local_offset(function, instruction->as.addr_slot.id);

                        if (!compiler_builder_appendf(&builder, "  li t5, %zu\n  add t4, s11, t5\n", offset) ||
                            !compiler_simple_machine_ir_store_reg_to_operand(
                                &builder, program, function, "t4", instruction->result)) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-141: failed to emit simple addr_local");
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_IR_INSTR_ADDR_GLOBAL: {
                        const MachineIrGlobal *global = instruction->as.addr_slot.id < program->global_count
                            ? &program->globals[instruction->as.addr_slot.id]
                            : NULL;

                        if (!global || !global->name ||
                            !compiler_builder_appendf(&builder, "  la t4, %s\n", global->name) ||
                            !compiler_simple_machine_ir_store_reg_to_operand(
                                &builder, program, function, "t4", instruction->result)) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-142: failed to emit simple addr_global");
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_IR_INSTR_LOAD_LOCAL: {
                        size_t offset = compiler_simple_machine_ir_local_offset(function, instruction->as.load_slot.id);

                        if (!compiler_append_stack_access(&builder, "lw", "t4", "s11", offset, "t6") ||
                            !compiler_simple_machine_ir_store_reg_to_operand(
                                &builder, program, function, "t4", instruction->result)) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-143: failed to emit simple load_local");
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_IR_INSTR_STORE_LOCAL: {
                        size_t offset = compiler_simple_machine_ir_local_offset(function, instruction->as.store.slot.id);

                        if (!compiler_simple_machine_ir_emit_operand_to_reg(
                                &builder, program, function, instruction->as.store.value, "t5") ||
                            !compiler_append_stack_access(&builder, "sw", "t5", "s11", offset, "t6")) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-144: failed to emit simple store_local");
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_IR_INSTR_LOAD_GLOBAL: {
                        const MachineIrGlobal *global = instruction->as.load_slot.id < program->global_count
                            ? &program->globals[instruction->as.load_slot.id]
                            : NULL;

                        if (!global || !global->name ||
                            !compiler_builder_appendf(&builder, "  la t6, %s\n  lw t4, 0(t6)\n", global->name) ||
                            !compiler_simple_machine_ir_store_reg_to_operand(
                                &builder, program, function, "t4", instruction->result)) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-145: failed to emit simple load_global");
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_IR_INSTR_STORE_GLOBAL: {
                        const MachineIrGlobal *global = instruction->as.store.slot.id < program->global_count
                            ? &program->globals[instruction->as.store.slot.id]
                            : NULL;

                        if (!global || !global->name ||
                            !compiler_simple_machine_ir_emit_operand_to_reg(
                                &builder, program, function, instruction->as.store.value, "t5") ||
                            !compiler_builder_appendf(&builder, "  la t6, %s\n  sw t5, 0(t6)\n", global->name)) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-146: failed to emit simple store_global");
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_IR_INSTR_LOAD_INDIRECT:
                        if (!compiler_simple_machine_ir_emit_operand_to_reg(
                                &builder, program, function, instruction->as.load_indirect_addr, "t6") ||
                            !compiler_builder_appendf(&builder, "  lw t4, 0(t6)\n") ||
                            !compiler_simple_machine_ir_store_reg_to_operand(
                                &builder, program, function, "t4", instruction->result)) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-147: failed to emit simple load_indirect");
                            return 0;
                        }
                        break;
                    case MACHINE_IR_INSTR_STORE_INDIRECT:
                        if (!compiler_simple_machine_ir_emit_operand_to_reg(
                                &builder, program, function, instruction->as.store_indirect.addr, "t6") ||
                            !compiler_simple_machine_ir_emit_operand_to_reg(
                                &builder, program, function, instruction->as.store_indirect.value, "t5") ||
                            !compiler_builder_appendf(&builder, "  sw t5, 0(t6)\n")) {
                            compiler_builder_free(&builder);
                            compiler_set_error(error, 0, 0, "COMPILER-148: failed to emit simple store_indirect");
                            return 0;
                        }
                        break;
                    default:
                        compiler_builder_free(&builder);
                        compiler_set_error(error, 0, 0, "COMPILER-149: unsupported simple machine-ir instruction");
                        return 0;
                }
            }

            switch (block->terminator.kind) {
                case MACHINE_IR_TERM_RETURN:
                    if (block->terminator.as.return_value.kind != MACHINE_IR_OPERAND_NONE &&
                        !compiler_simple_machine_ir_emit_operand_to_reg(
                            &builder, program, function, block->terminator.as.return_value, "a0")) {
                        compiler_builder_free(&builder);
                        compiler_set_error(error, 0, 0, "COMPILER-150: failed to materialize simple return value");
                        return 0;
                    }
                    if (!compiler_append_stack_access(&builder, "lw", "ra", "s11", ra_save_offset, "t6") ||
                        !compiler_append_stack_access(&builder, "lw", "s11", "s11", s11_save_offset, "t6") ||
                        !compiler_append_stack_adjust(&builder, (long long)frame_bytes) ||
                        !compiler_builder_appendf(&builder, "  ret\n")) {
                        compiler_builder_free(&builder);
                        compiler_set_error(error, 0, 0, "COMPILER-151: failed to emit simple return");
                        return 0;
                    }
                    break;
                case MACHINE_IR_TERM_JUMP:
                    if (!compiler_builder_appendf(
                            &builder, "  j .L%s_%zu\n", function->name, block->terminator.as.jump_target)) {
                        compiler_builder_free(&builder);
                        compiler_set_error(error, 0, 0, "COMPILER-152: failed to emit simple jump");
                        return 0;
                    }
                    break;
                case MACHINE_IR_TERM_BRANCH:
                    if (!compiler_simple_machine_ir_emit_operand_to_reg(
                            &builder, program, function, block->terminator.as.branch.condition, "t5") ||
                        !compiler_builder_appendf(
                            &builder,
                            "  bne t5, zero, .L%s_%zu\n  j .L%s_%zu\n",
                            function->name,
                            block->terminator.as.branch.then_target,
                            function->name,
                            block->terminator.as.branch.else_target)) {
                        compiler_builder_free(&builder);
                        compiler_set_error(error, 0, 0, "COMPILER-153: failed to emit simple branch");
                        return 0;
                    }
                    break;
                default:
                    compiler_builder_free(&builder);
                    compiler_set_error(error, 0, 0, "COMPILER-154: unsupported simple terminator");
                    return 0;
            }
        }

        if (!compiler_builder_appendf(&builder, ".size %s, .-%s\n\n", function->name, function->name)) {
            compiler_builder_free(&builder);
            compiler_set_error(error, 0, 0, "COMPILER-155: failed to finalize simple function");
            return 0;
        }
    }

    if (has_main && compiler_use_runtime_startup_stub_enabled()) {
        if (!compiler_builder_appendf(&builder,
                ".weak _start\n"
                ".type _start, @function\n"
                "_start:\n"
                "  call main\n"
                "  li a7, 93\n"
                "  ecall\n"
                ".size _start, .-_start\n")) {
            compiler_builder_free(&builder);
            compiler_set_error(error, 0, 0, "COMPILER-156: failed to emit simple startup");
            return 0;
        }
    }

    *out_text = builder.data;
    builder.data = NULL;
    compiler_builder_free(&builder);
    return 1;
}

int compiler_emit_riscv_preview_text_from_report_simple(const MachineIrAllocateRewriteReport *report,
    CompilerMode mode,
    char **out_text,
    CompilerError *error) {
    MachineBytesReport bytes_report;
    CompilerRiscvPreviewCache preview_cache;
    const MachineBytesProgram *program = NULL;
    size_t symbol_count = 0u;
    CompilerStringBuilder builder;
    MachineIrProgram simple_program;
    MachineIrError machine_error;
    int ok = 0;
    size_t symbol_index;
    size_t function_index;
    int has_main = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        compiler_set_error(error, 0, 0, "COMPILER-109: invalid simple riscv text export contract");
        return 0;
    }

    machine_ir_program_init(&simple_program);
    memset(&machine_error, 0, sizeof(machine_error));
    if (compiler_use_direct_simple_text_enabled()) {
        if (machine_ir_clone_program(&report->program, &simple_program, &machine_error) &&
            machine_ir_eliminate_phi_nodes(&simple_program, &machine_error) &&
            machine_ir_cleanup_after_phi_elimination(&simple_program, &machine_error) &&
            compiler_emit_riscv_preview_text_from_machine_ir_program_simple(&simple_program, out_text, error)) {
            machine_ir_program_free(&simple_program);
            return 1;
        }
    }
    machine_ir_program_free(&simple_program);

    compiler_riscv_preview_cache_init(&preview_cache);
    compiler_builder_init(&builder);
    memset(&bytes_report, 0, sizeof(bytes_report));

    ok = compiler_build_riscv_preview_bytes_report_from_machine_ir_report(report, mode, &bytes_report, error);
    if (!ok) {
        goto cleanup;
    }
    if (!compiler_riscv_preview_cache_build(&bytes_report, &preview_cache)) {
        compiler_set_error(error, 0, 0, "COMPILER-110: failed to build riscv preview lookup cache");
        ok = 0;
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
    if (program->function_count > 0u && !compiler_builder_appendf(&builder, "\n")) {
        compiler_set_error(error, 0, 0, "COMPILER-115: out of memory separating text header");
        ok = 0;
        goto cleanup;
    }
    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        const MachineBytesFunction *function = NULL;
        const char *function_name = NULL;
        int has_body = 0;
        size_t parameter_count = 0u;
        size_t local_count = 0u;
        size_t block_count = 0u;
        size_t spill_slot_count = 0u;
        const MachineBytesFunctionSummary *function_summary = NULL;
        size_t frame_bytes = 0u;
        size_t local_storage_bytes = 0u;
        size_t ra_save_offset = 0u;
        size_t s11_save_offset = 0u;
        size_t block_index;

        if (!machine_bytes_report_get_function(&bytes_report, function_index, &function) || !function ||
            !machine_bytes_function_get_summary(function, &function_name, &has_body, &parameter_count, &local_count, &block_count, &spill_slot_count) ||
            !machine_bytes_report_get_function_summary_artifact(&bytes_report, function_index, &function_summary) ||
            !function_summary) {
            compiler_set_error(error, 0, 0, "COMPILER-116: malformed bytes function surface");
            ok = 0;
            goto cleanup;
        }
        if (!has_body || !function_name || function_name[0] == '\0') {
            continue;
        }
        if (strcmp(function_name, "main") == 0) {
            has_main = 1;
        }
        local_storage_bytes = (local_count + spill_slot_count) * 4u;
        frame_bytes = local_storage_bytes > parameter_count * 4u ? local_storage_bytes : parameter_count * 4u;
        if (function_summary->call_count > 0u) {
            frame_bytes += 8u;
        }
        if (frame_bytes > 0u && (frame_bytes % 16u) != 0u) {
            frame_bytes = ((frame_bytes + 15u) / 16u) * 16u;
        }
        if (function_summary->call_count > 0u) {
            ra_save_offset = frame_bytes - 4u;
            s11_save_offset = frame_bytes - 8u;
        }
        if (!compiler_builder_appendf(&builder, ".globl %s\n.type %s, @function\n%s:\n", function_name, function_name, function_name)) {
            compiler_set_error(error, 0, 0, "COMPILER-117: out of memory writing function header");
            ok = 0;
            goto cleanup;
        }
        if (frame_bytes > 0u) {
            size_t param_index;

            if (!compiler_append_stack_adjust(&builder, -(long long)frame_bytes)) {
                compiler_set_error(error, 0, 0, "COMPILER-117: out of memory writing function prologue");
                ok = 0;
                goto cleanup;
            }
            if (function_summary->call_count > 0u &&
                (!compiler_append_stack_access(&builder, "sw", "ra", "sp", ra_save_offset, "t6") ||
                    !compiler_append_stack_access(&builder, "sw", "s11", "sp", s11_save_offset, "t6") ||
                    !compiler_builder_appendf(&builder, "  mv s11, sp\n"))) {
                compiler_set_error(error, 0, 0, "COMPILER-117: out of memory writing function prologue");
                ok = 0;
                goto cleanup;
            }
            for (param_index = 0u; param_index < parameter_count; ++param_index) {
                if (param_index < 8u) {
                    if (!compiler_append_stack_access(&builder,
                            "sw",
                            compiler_riscv_register_name((uint32_t)(10u + param_index)),
                            "sp",
                            param_index * 4u,
                            "t6")) {
                        compiler_set_error(error, 0, 0, "COMPILER-117: out of memory writing function prologue");
                        ok = 0;
                        goto cleanup;
                    }
                } else {
                    size_t caller_stack_offset = frame_bytes + ((param_index - 8u) * 4u);

                    if (!compiler_append_stack_access(&builder, "lw", "t5", "sp", caller_stack_offset, "t6") ||
                        !compiler_append_stack_access(&builder, "sw", "t5", "sp", param_index * 4u, "t6")) {
                        compiler_set_error(error, 0, 0, "COMPILER-117: out of memory writing function prologue");
                        ok = 0;
                        goto cleanup;
                    }
                }
            }
        }

        for (block_index = 0u; block_index < block_count; ++block_index) {
            const MachineBytesBlock *block = NULL;
            MachineBytesBlockSummary block_summary;
            const unsigned char *block_bytes = NULL;
            size_t block_byte_count = 0u;
            size_t function_start = 0u;
            size_t byte_offset;
            size_t skip_until_program_byte_offset = (size_t)-1;

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
                if (!compiler_builder_appendf(&builder, ".L%s_%zu:\n", function_name, block_index)) {
                    compiler_set_error(error, 0, 0, "COMPILER-120: out of memory writing block label");
                    ok = 0;
                    goto cleanup;
                }
            }
            for (byte_offset = 0u; byte_offset < block_byte_count; byte_offset += 4u) {
                uint32_t word = compiler_read_u32_le(block_bytes + byte_offset);
                size_t program_byte_offset = function_start + block_summary.start_byte_offset + byte_offset;

                if (skip_until_program_byte_offset != (size_t)-1 &&
                    program_byte_offset < skip_until_program_byte_offset) {
                    continue;
                }
                if (!compiler_append_riscv_preview_instruction(
                        &builder,
                        &bytes_report,
                        &preview_cache,
                        program_byte_offset,
                        word,
                        frame_bytes,
                        ra_save_offset,
                        function_summary->call_count > 0u,
                        0u,
                        0)) {
                    compiler_set_error(error, 0, 0, "COMPILER-121: out of memory writing riscv instruction");
                    ok = 0;
                    goto cleanup;
                }
                {
                    const MachineBytesFixupSummary *current_fixup =
                        compiler_find_fixup_at_patch_offset_cached(&preview_cache, program_byte_offset);

                    if (current_fixup &&
                        current_fixup->kind == MACHINE_BYTES_FIXUP_CALL_TARGET &&
                        compiler_should_collapse_call_fixup(&bytes_report, current_fixup, 0) &&
                        (word & 0x7Fu) == 0x17u) {
                        skip_until_program_byte_offset = program_byte_offset + 8u;
                    } else {
                        skip_until_program_byte_offset = (size_t)-1;
                    }
                }
            }
        }
        if (!compiler_builder_appendf(&builder, ".size %s, .-%s\n\n", function_name, function_name)) {
            compiler_set_error(error, 0, 0, "COMPILER-122: out of memory separating functions");
            ok = 0;
            goto cleanup;
        }
    }

    if (has_main && compiler_use_runtime_startup_stub_enabled()) {
        if (!compiler_builder_appendf(&builder,
                ".weak _start\n"
                ".type _start, @function\n"
                "_start:\n"
                "  call main\n"
                "  li a7, 93\n"
                "  ecall\n"
                ".size _start, .-_start\n")) {
            compiler_set_error(error, 0, 0, "COMPILER-123: out of memory writing riscv startup");
            ok = 0;
            goto cleanup;
        }
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    compiler_builder_free(&builder);
    compiler_riscv_preview_cache_free(&preview_cache);
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

int compiler_compile_source_text_with_options(const char *source,
    CompilerMode mode,
    const CompilerOptions *options,
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
    SemanticOptions semantic_options;
    IrLowerOptions ir_options;
    LowerIrOptions lower_options;
    CompilerSavedEnv simple_backend_saved_envs[] = {
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
    };
    const char *simple_backend_env_names[] = {
        "COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS",
        "COMPILER_USE_CALLER_SAVE_TEXT",
        "COMPILER_USE_FINAL_TEXT_PEEPHOLES",
        "COMPILER_USE_SMALL_DATA_SECTIONS",
        "COMPILER_USE_DEFAULT_SSA_BUILD",
        "COMPILER_USE_PERF_HOTSPOTS",
        "MACHINE_SELECT_SKIP_REUSE_ADDR_ROOTS",
        "MACHINE_SELECT_SKIP_CLEANUP_PURE",
        "MACHINE_SELECT_SKIP_REUSE_INTERNAL_PURE_CALLS",
        "MACHINE_SELECT_SKIP_REUSE_UNIQUE_PREDECESSOR_PURE_CALLS",
        "MACHINE_SELECT_SKIP_FORWARD_SAME_BLOCK_INDIRECT_LOADS",
        "MACHINE_SELECT_SKIP_REUSE_SPILL_PURE_EXPR",
        "MACHINE_SELECT_SKIP_FULL_CLEANUP",
    };
    size_t simple_backend_env_index = 0u;
    int simple_backend_profile_applied = 0;
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
    memset(&semantic_options, 0, sizeof(semantic_options));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&ir_options, 0, sizeof(ir_options));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&lower_options, 0, sizeof(lower_options));
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
    semantic_options.skip_all_paths_return_check =
        options && options->skip_all_paths_return_check ? 1 : 0;
    ok = semantic_analyze_program_with_options(&ast_program, &semantic_options, &semantic_error);
    if (!ok) {
        compiler_copy_stage_error(error, semantic_error.line, semantic_error.column, semantic_error.message);
        goto cleanup;
    }
    ir_options.allow_implicit_fallthrough_return =
        options && options->skip_all_paths_return_check ? 1 : 0;
    ok = ir_lower_program(&ast_program, &ir_options, &ir_program, &ir_error);
    if (!ok) {
        compiler_copy_stage_error(error, ir_error.line, ir_error.column, ir_error.message);
        goto cleanup;
    }
    lower_options.allow_implicit_fallthrough_return =
        options && options->skip_all_paths_return_check ? 1 : 0;
    ok = lower_ir_lower_from_ir(&ir_program, &lower_options, &lower_program, &lower_error);
    if (!ok) {
        compiler_copy_stage_error(error, lower_error.line, lower_error.column, lower_error.message);
        goto cleanup;
    }
    if (compiler_use_simple_backend_enabled()) {
        for (simple_backend_env_index = 0u;
             simple_backend_env_index < sizeof(simple_backend_saved_envs) / sizeof(simple_backend_saved_envs[0]);
             ++simple_backend_env_index) {
            compiler_saved_env_init(
                &simple_backend_saved_envs[simple_backend_env_index],
                simple_backend_env_names[simple_backend_env_index]);
            if (!compiler_save_env(&simple_backend_saved_envs[simple_backend_env_index])) {
                compiler_set_error(error, 0, 0, "COMPILER-124: failed to save simple-backend env");
                goto cleanup;
            }
        }
        if (!compiler_apply_simple_backend_profile()) {
            compiler_set_error(error, 0, 0, "COMPILER-125: failed to apply simple-backend profile");
            goto cleanup;
        }
        simple_backend_profile_applied = 1;
    }
    if (compiler_use_default_ssa_build_enabled()) {
        ok = value_ssa_build_default_from_lower_ir(&lower_program, &value_program, &value_error);
    } else {
        ok = value_ssa_build_from_lower_ir(&lower_program, &value_program, &value_error);
    }
    if (!ok) {
        compiler_copy_stage_error(error, value_error.line, value_error.column, value_error.message);
        goto cleanup;
    }
    if (mode == COMPILER_MODE_PERF) {
        if (!memory_ssa_pass_scalar_replace_local_slots(&value_program, &value_error)) {
            compiler_copy_stage_error(error, value_error.line, value_error.column, value_error.message);
            goto cleanup;
        }
        if (!memory_ssa_pass_scalar_replace_global_slots(&value_program, &value_error)) {
            compiler_copy_stage_error(error, value_error.line, value_error.column, value_error.message);
            goto cleanup;
        }
    }
    if (compiler_use_perf_hotspots_enabled()) {
        if (!value_ssa_optimize_perf_hotspots(&value_program, &value_error)) {
            compiler_copy_stage_error(error, value_error.line, value_error.column, value_error.message);
            goto cleanup;
        }
    }
    if (compiler_use_simple_backend_enabled()) {
        ok = compiler_simple_backend_emit_from_value_ssa(&value_program, out_text, error);
        if (!ok) {
            goto cleanup;
        }
    } else {
        ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_program_only_report(
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
    }

cleanup:
    if (simple_backend_profile_applied) {
        for (simple_backend_env_index = 0u;
             simple_backend_env_index < sizeof(simple_backend_saved_envs) / sizeof(simple_backend_saved_envs[0]);
             ++simple_backend_env_index) {
            compiler_restore_env(&simple_backend_saved_envs[simple_backend_env_index]);
        }
    }
    machine_ir_allocate_rewrite_report_free(&machine_report);
    value_ssa_program_free(&value_program);
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}

int compiler_compile_source_text(const char *source,
    CompilerMode mode,
    char **out_text,
    CompilerError *error) {
    CompilerOptions options;

    memset(&options, 0, sizeof(options));
    options.skip_all_paths_return_check = 1;
    return compiler_compile_source_text_with_options(source, mode, &options, out_text, error);
}

int compiler_compile_file_with_options(const char *input_path,
    CompilerMode mode,
    const CompilerOptions *options,
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
    ok = compiler_compile_source_text_with_options(source_text, mode, options, out_text, error);
    free(source_text);
    return ok;
}

int compiler_compile_file(const char *input_path,
    CompilerMode mode,
    char **out_text,
    CompilerError *error) {
    CompilerOptions options;

    memset(&options, 0, sizeof(options));
    options.skip_all_paths_return_check = 1;
    return compiler_compile_file_with_options(input_path, mode, &options, out_text, error);
}
