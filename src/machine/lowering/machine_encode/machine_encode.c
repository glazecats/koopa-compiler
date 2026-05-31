#include "machine/encode.h"
#include "machine/bytes.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineEncodeStringBuilder;

static void machine_encode_set_error(MachineEncodeError *error, int line, int column, const char *fmt, ...);
static char *machine_encode_strdup(const char *text);
static int machine_encode_append_format(MachineEncodeStringBuilder *builder, const char *fmt, ...);
static void machine_encode_block_free(MachineEncodeBlock *block);
static void machine_encode_function_free(MachineEncodeFunction *function);
static void machine_encode_register_desc_free(MachineEmitRegisterDesc *desc);
static int machine_encode_op_has_call_payload(MachineSelectOpKind kind);
static int machine_encode_op_clone(MachineEmitOp *dest, const MachineEmitOp *src);
static void machine_encode_op_free(MachineEmitOp *op);
static int machine_encode_clone_block_ops(MachineEncodeBlock *dest, const MachineEmitBlock *src);

static void machine_encode_set_error(MachineEncodeError *error, int line, int column, const char *fmt, ...) {
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

static char *machine_encode_strdup(const char *text) {
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

static int machine_encode_append_format(MachineEncodeStringBuilder *builder, const char *fmt, ...) {
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

static void machine_encode_block_free(MachineEncodeBlock *block) {
    size_t op_index;

    if (!block) {
        return;
    }
    free(block->label_name);
    block->label_name = NULL;
    free(block->block.label_name);
    block->block.label_name = NULL;
    for (op_index = 0; op_index < block->block.op_count; ++op_index) {
        machine_encode_op_free(&block->block.ops[op_index]);
    }
    free(block->block.ops);
    block->block.ops = NULL;
    block->block.op_count = 0;
    block->block.op_capacity = 0;
    block->block.has_terminator = 0;
}

static void machine_encode_function_free(MachineEncodeFunction *function) {
    size_t block_index;

    if (!function) {
        return;
    }
    free(function->name);
    function->name = NULL;
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        machine_encode_block_free(&function->blocks[block_index]);
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

static void machine_encode_register_desc_free(MachineEmitRegisterDesc *desc) {
    if (!desc) {
        return;
    }
    free(desc->name);
    desc->name = NULL;
}

static int machine_encode_op_has_call_payload(MachineSelectOpKind kind) {
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

static int machine_encode_op_clone(MachineEmitOp *dest, const MachineEmitOp *src) {
    if (!dest || !src) {
        return 0;
    }

    *dest = *src;
    if (src->kind == MACHINE_SELECT_OP_ADDR_FUNCTION) {
        dest->as.addr_function_name = machine_encode_strdup(src->as.addr_function_name);
        return !src->as.addr_function_name || dest->as.addr_function_name != NULL;
    }
    if (!machine_encode_op_has_call_payload(src->kind)) {
        return 1;
    }

    dest->as.call.callee_name = machine_encode_strdup(src->as.call.callee_name);
    if (src->as.call.callee_name && !dest->as.call.callee_name) {
        return 0;
    }
    if (src->as.call.arg_count > 0) {
        dest->as.call.args = (MachineEmitOperand *)calloc(src->as.call.arg_count, sizeof(MachineEmitOperand));
        if (!dest->as.call.args) {
            free(dest->as.call.callee_name);
            dest->as.call.callee_name = NULL;
            return 0;
        }
        memcpy(dest->as.call.args, src->as.call.args, src->as.call.arg_count * sizeof(MachineEmitOperand));
    } else {
        dest->as.call.args = NULL;
    }
    return 1;
}

static void machine_encode_op_free(MachineEmitOp *op) {
    if (!op) {
        return;
    }
    if (op->kind == MACHINE_SELECT_OP_ADDR_FUNCTION) {
        free(op->as.addr_function_name);
        op->as.addr_function_name = NULL;
        return;
    }
    if (!machine_encode_op_has_call_payload(op->kind)) {
        return;
    }

    free(op->as.call.callee_name);
    free(op->as.call.args);
    op->as.call.callee_name = NULL;
    op->as.call.args = NULL;
    op->as.call.arg_count = 0;
}

static int machine_encode_clone_block_ops(MachineEncodeBlock *dest, const MachineEmitBlock *src) {
    size_t op_index;

    if (!dest || !src) {
        return 0;
    }
    dest->block = *src;
    dest->block.label_name = machine_encode_strdup(src->label_name);
    if (src->label_name && !dest->block.label_name) {
        return 0;
    }
    if (src->op_count > 0) {
        dest->block.ops = (MachineEmitOp *)calloc(src->op_count, sizeof(MachineEmitOp));
        if (!dest->block.ops) {
            machine_encode_block_free(dest);
            return 0;
        }
        for (op_index = 0; op_index < src->op_count; ++op_index) {
            if (!machine_encode_op_clone(&dest->block.ops[op_index], &src->ops[op_index])) {
                machine_encode_block_free(dest);
                return 0;
            }
        }
    }
    return 1;
}

#define MACHINE_ENCODE_SPLIT_AGGREGATOR
#include "machine_encode_core.inc"
#include "machine_encode_query.inc"
#include "machine_encode_verify.inc"
#include "machine_encode_dump.inc"
#include "machine_encode_lower.inc"
#include "machine_encode_report.inc"
