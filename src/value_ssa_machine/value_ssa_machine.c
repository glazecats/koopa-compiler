#include "value_ssa_machine.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} ValueSsaMachineStringBuilder;

static void value_ssa_machine_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...);
static int value_ssa_machine_append_format(ValueSsaMachineStringBuilder *builder, const char *fmt, ...);
static char *value_ssa_machine_strdup(const char *text);

static char *value_ssa_machine_strdup(const char *text) {
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

static void value_ssa_machine_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...) {
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

static int value_ssa_machine_append_format(ValueSsaMachineStringBuilder *builder, const char *fmt, ...) {
    va_list args;
    va_list args_copy;
    int needed;
    size_t required;
    size_t new_capacity;
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

    required = builder->length + (size_t)needed + 1;
    if (required > builder->capacity) {
        new_capacity = builder->capacity == 0 ? 64 : builder->capacity;
        while (new_capacity < required) {
            size_t doubled = new_capacity * 2;

            if (doubled <= new_capacity) {
                new_capacity = required;
                break;
            }
            new_capacity = doubled;
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

#define VALUE_SSA_MACHINE_SPLIT_AGGREGATOR
#include "value_ssa_machine_core.inc"
#include "value_ssa_machine_dump.inc"
