#include "machine/layout.h"

#include "machine/select.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineLayoutStringBuilder;

static void machine_layout_set_error(MachineLayoutError *error, int line, int column, const char *fmt, ...);
static char *machine_layout_strdup(const char *text);
static int machine_layout_append_format(MachineLayoutStringBuilder *builder, const char *fmt, ...);
static int machine_layout_op_has_call_payload(MachineSelectOpKind kind);
static int machine_layout_op_clone(MachineLayoutOp *dest, const MachineLayoutOp *src);
static void machine_layout_op_free(MachineLayoutOp *op);
static void machine_layout_block_free(MachineLayoutBlock *block);
static void machine_layout_function_free(MachineLayoutFunction *function);
static void machine_layout_global_free(MachineLayoutGlobal *global);
static void machine_layout_register_desc_free(MachineLayoutRegisterDesc *desc);

static void machine_layout_set_error(MachineLayoutError *error, int line, int column, const char *fmt, ...) {
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

static char *machine_layout_strdup(const char *text) {
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

static int machine_layout_append_format(MachineLayoutStringBuilder *builder, const char *fmt, ...) {
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

static int machine_layout_op_has_call_payload(MachineSelectOpKind kind) {
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

static int machine_layout_op_clone(MachineLayoutOp *dest, const MachineLayoutOp *src) {
    if (!dest || !src) {
        return 0;
    }

    *dest = *src;
    if (!machine_layout_op_has_call_payload(src->kind)) {
        return 1;
    }

    dest->as.call.callee_name = machine_layout_strdup(src->as.call.callee_name);
    if (src->as.call.callee_name && !dest->as.call.callee_name) {
        return 0;
    }
    if (src->as.call.arg_count > 0) {
        dest->as.call.args =
            (MachineLayoutOperand *)calloc(src->as.call.arg_count, sizeof(MachineLayoutOperand));
        if (!dest->as.call.args) {
            free(dest->as.call.callee_name);
            dest->as.call.callee_name = NULL;
            return 0;
        }
        memcpy(dest->as.call.args, src->as.call.args, src->as.call.arg_count * sizeof(MachineLayoutOperand));
    } else {
        dest->as.call.args = NULL;
    }
    return 1;
}

static void machine_layout_op_free(MachineLayoutOp *op) {
    if (!op || !machine_layout_op_has_call_payload(op->kind)) {
        return;
    }

    free(op->as.call.callee_name);
    free(op->as.call.args);
    op->as.call.callee_name = NULL;
    op->as.call.args = NULL;
    op->as.call.arg_count = 0;
}

static void machine_layout_block_free(MachineLayoutBlock *block) {
    size_t op_index;

    if (!block) {
        return;
    }
    for (op_index = 0; op_index < block->op_count; ++op_index) {
        machine_layout_op_free(&block->ops[op_index]);
    }
    free(block->ops);
    block->ops = NULL;
    block->op_count = 0;
    block->op_capacity = 0;
    block->has_terminator = 0;
}

static void machine_layout_function_free(MachineLayoutFunction *function) {
    size_t block_index;

    if (!function) {
        return;
    }
    free(function->name);
    function->name = NULL;
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        machine_layout_block_free(&function->blocks[block_index]);
    }
    free(function->blocks);
    function->blocks = NULL;
    function->block_count = 0;
    function->block_capacity = 0;
    function->parameter_count = 0;
    function->local_count = 0;
    function->spill_slot_count = 0;
    function->has_body = 0;
}

static void machine_layout_global_free(MachineLayoutGlobal *global) {
    if (!global) {
        return;
    }
    free(global->name);
    global->name = NULL;
}

static void machine_layout_register_desc_free(MachineLayoutRegisterDesc *desc) {
    if (!desc) {
        return;
    }
    free(desc->name);
    desc->name = NULL;
}

#define MACHINE_LAYOUT_SPLIT_AGGREGATOR
#include "machine_layout_core.inc"
#include "machine_layout_verify.inc"
#include "machine_layout_dump.inc"
#include "machine_layout_lower.inc"
