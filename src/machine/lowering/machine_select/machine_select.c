#include "machine/select.h"

#include "machine/layout.h"

#include <stdint.h>
#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineSelectStringBuilder;

static long long machine_select_normalize_sysy_int_value(long long value) {
    uint32_t bits = (uint32_t)value;
    return (long long)(int32_t)bits;
}

static double machine_select_now_s(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

static int machine_select_trace_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_TRACE_TIMING");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_trace_pure_iter_hash_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_TRACE_PURE_ITER_HASH");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_trace_pure_iter_dump_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_TRACE_PURE_ITER_DUMP");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_pure_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_CLEANUP_PURE");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_reuse_addr_roots_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_REUSE_ADDR_ROOTS");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_reuse_internal_pure_calls_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_REUSE_INTERNAL_PURE_CALLS");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_reuse_unique_predecessor_pure_calls_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_REUSE_UNIQUE_PREDECESSOR_PURE_CALLS");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_reuse_unique_predecessor_small_pure_exprs_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_REUSE_UNIQUE_PREDECESSOR_SMALL_PURE_EXPRS");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_forward_same_block_indirect_loads_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_FORWARD_SAME_BLOCK_INDIRECT_LOADS");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_reuse_spill_pure_expr_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_REUSE_SPILL_PURE_EXPR");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_full_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_FULL_CLEANUP");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_forward_trivial_defs_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_FORWARD_TRIVIAL_DEFS");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_remove_dead_defs_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_REMOVE_DEAD_DEFS");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_canonicalize_ops_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_CANONICALIZE_OPS");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_remove_copy_self_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_REMOVE_COPY_SELF");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_fold_addr_copy_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_FOLD_ADDR_COPY");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_skip_cleanup_fold_pure_producer_copy_enabled(void) {
    const char *flag = getenv("MACHINE_SELECT_SKIP_FOLD_PURE_PRODUCER_COPY");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static int machine_select_aggressive_opt_mode_enabled(void) {
    const char *flag = getenv("COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS");

    return !flag || flag[0] == '\0' || strcmp(flag, "0") != 0;
}

static void machine_select_trace_timing(const char *stage, double elapsed_s) {
    if (!stage || !machine_select_trace_enabled()) {
        return;
    }
    fprintf(stderr, "[machine-select-timing] %s %.3f\n", stage, elapsed_s);
}

static void machine_select_trace_marker(const char *stage) {
    if (!stage || !machine_select_trace_enabled()) {
        return;
    }
    fprintf(stderr, "[machine-select-stage] %s\n", stage);
    fflush(stderr);
}

static void machine_select_set_error(MachineSelectError *error, int line, int column, const char *fmt, ...);
static char *machine_select_strdup(const char *text);
static int machine_select_append_format(MachineSelectStringBuilder *builder, const char *fmt, ...);
static void machine_select_op_free(MachineSelectOp *op);
static void machine_select_block_free(MachineSelectBasicBlock *block);
static void machine_select_function_free(MachineSelectFunction *function);
static void machine_select_global_free(MachineSelectGlobal *global);
static void machine_select_register_desc_free(MachineSelectRegisterDesc *desc);
static int machine_select_op_is_call_kind(MachineSelectOpKind kind);
static int machine_select_binary_is_compare(MachineIrBinaryOp op);

static void machine_select_set_error(MachineSelectError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (!error || !fmt) {
        return;
    }

    error->line = line;
    error->column = column;
    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

static char *machine_select_strdup(const char *text) {
    char *copy;
    size_t length;

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

static int machine_select_binary_is_compare(MachineIrBinaryOp op) {
    switch (op) {
        case MACHINE_IR_BINARY_EQ:
        case MACHINE_IR_BINARY_NE:
        case MACHINE_IR_BINARY_LT:
        case MACHINE_IR_BINARY_LE:
        case MACHINE_IR_BINARY_GT:
        case MACHINE_IR_BINARY_GE:
            return 1;
        default:
            return 0;
    }
}

static int machine_select_op_is_call_kind(MachineSelectOpKind kind) {
    switch (kind) {
        case MACHINE_SELECT_OP_CALL:
        case MACHINE_SELECT_OP_CALL_IMM:
        case MACHINE_SELECT_OP_CALL_SPILL:
        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
        case MACHINE_SELECT_OP_CALL_VOID:
        case MACHINE_SELECT_OP_CALL_VOID_IMM:
            return 1;
        default:
            return 0;
    }
}

int machine_select_get_target_policy_summary(MachineSelectTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->current_riscv32_preview_logical_register_cap = 8u;
    out_summary->supports_early_immediate_legalization = 1;
    out_summary->supports_compare_branch_fusion = 1;
    out_summary->preserves_spill_operands_for_later_materialization = 1;
    out_summary->preserves_global_slot_ops_for_later_address_formation = 1;
    return 1;
}

int machine_select_verify_current_riscv32_preview_compatibility(const MachineSelectProgram *program,
    MachineSelectError *error) {
    MachineSelectTargetPolicySummary policy_summary;

    if (!program) {
        machine_select_set_error(
            error, 0, 0, "MACHINE-SELECT-418: invalid riscv32-preview compatibility contract");
        return 0;
    }
    if (!machine_select_verify_program(program, error)) {
        return 0;
    }
    if (!machine_select_program_get_target_policy_summary(program, &policy_summary)) {
        machine_select_set_error(
            error, 0, 0, "MACHINE-SELECT-419: missing riscv32-preview compatibility policy");
        return 0;
    }
    if (program->register_bank.register_count > policy_summary.current_riscv32_preview_logical_register_cap) {
        machine_select_set_error(
            error,
            0,
            0,
            "MACHINE-SELECT-420: current riscv32-preview lane supports at most %zu logical machine registers",
            policy_summary.current_riscv32_preview_logical_register_cap);
        return 0;
    }
    return 1;
}

int machine_select_verify_current_riscv32_preview_bytes_compatibility(const MachineSelectProgram *program,
    MachineSelectError *error) {
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    int ok;

    if (!program) {
        machine_select_set_error(
            error, 0, 0, "MACHINE-SELECT-422: invalid riscv32-preview bytes-compatibility contract");
        return 0;
    }
    if (!machine_select_verify_current_riscv32_preview_compatibility(program, error)) {
        return 0;
    }

    memset(&layout_error, 0, sizeof(layout_error));
    machine_layout_program_init(&layout_program);
    ok = machine_layout_lower_program_from_machine_select(program, &layout_program, &layout_error) &&
        machine_layout_verify_current_riscv32_preview_bytes_compatibility(&layout_program, &layout_error);
    machine_layout_program_free(&layout_program);
    if (!ok) {
        machine_select_set_error(
            error,
            layout_error.line,
            layout_error.column,
            "MACHINE-SELECT-423: current riscv32-preview bytes bridge rejected selected program: %s",
            layout_error.message);
        return 0;
    }
    return 1;
}

static int machine_select_append_format(MachineSelectStringBuilder *builder, const char *fmt, ...) {
    va_list args;
    va_list args_copy;
    int needed;
    size_t required_length;
    char *new_data;

    if (!builder || !fmt) {
        return 0;
    }

    va_start(args, fmt);
    va_copy(args_copy, args);
    needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) {
        va_end(args);
        return 0;
    }

    required_length = builder->length + (size_t)needed + 1;
    if (required_length > builder->capacity) {
        size_t next_capacity = builder->capacity == 0 ? 128 : builder->capacity;

        while (next_capacity < required_length) {
            if (next_capacity > ((size_t)-1) / 2) {
                va_end(args);
                return 0;
            }
            next_capacity *= 2;
        }

        new_data = (char *)realloc(builder->data, next_capacity);
        if (!new_data) {
            va_end(args);
            return 0;
        }
        builder->data = new_data;
        builder->capacity = next_capacity;
    }

    vsnprintf(builder->data + builder->length, builder->capacity - builder->length, fmt, args);
    builder->length += (size_t)needed;
    va_end(args);
    return 1;
}

static void machine_select_op_free(MachineSelectOp *op) {
    if (!op) {
        return;
    }
    if (machine_select_op_is_call_kind(op->kind)) {
        free(op->as.call.callee_name);
        free(op->as.call.args);
        op->as.call.callee_name = NULL;
        op->as.call.args = NULL;
        op->as.call.arg_count = 0;
    }
}

static void machine_select_block_free(MachineSelectBasicBlock *block) {
    size_t op_index;

    if (!block) {
        return;
    }
    if (block->ops) {
        for (op_index = 0; op_index < block->op_count; ++op_index) {
            machine_select_op_free(&block->ops[op_index]);
        }
    }
    free(block->ops);
    block->ops = NULL;
    block->op_count = 0;
    block->op_capacity = 0;
    block->has_terminator = 0;
}

static void machine_select_function_free(MachineSelectFunction *function) {
    size_t local_index;
    size_t block_index;

    if (!function) {
        return;
    }
    free(function->name);
    function->name = NULL;
    if (function->locals) {
        for (local_index = 0; local_index < function->local_count; ++local_index) {
            free(function->locals[local_index].source_name);
            function->locals[local_index].source_name = NULL;
        }
    }
    if (function->blocks) {
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            machine_select_block_free(&function->blocks[block_index]);
        }
    }
    free(function->locals);
    free(function->blocks);
    function->locals = NULL;
    function->blocks = NULL;
    function->local_count = 0;
    function->local_capacity = 0;
    function->block_count = 0;
    function->block_capacity = 0;
    function->parameter_count = 0;
    function->spill_slot_count = 0;
    function->has_body = 0;
}

static void machine_select_global_free(MachineSelectGlobal *global) {
    if (!global) {
        return;
    }
    free(global->name);
    global->name = NULL;
}

static void machine_select_register_desc_free(MachineSelectRegisterDesc *desc) {
    if (!desc) {
        return;
    }
    free(desc->name);
    desc->name = NULL;
}

#define MACHINE_SELECT_SPLIT_AGGREGATOR
#include "machine_select_core.inc"
#include "machine_select_query.inc"
#include "machine_select_verify.inc"
#include "machine_select_dump.inc"
#include "machine_select_lower.inc"
#include "machine_select_cleanup.inc"
#include "machine_select_report.inc"
