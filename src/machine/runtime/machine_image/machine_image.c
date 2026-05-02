#include "machine/image.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineImageStringBuilder;

static void machine_image_set_error(MachineImageError *error, int line, int column, const char *fmt, ...);
static char *machine_image_strdup(const char *text);
static int machine_image_append_format(MachineImageStringBuilder *builder, const char *fmt, ...);
static size_t machine_image_base_virtual_address(MachineElfTargetProfile profile);
static size_t machine_image_segment_alignment(MachineElfTargetProfile profile);
static const char *machine_image_relocation_kind_name(MachineRelocKind kind);
static int machine_image_apply_profile(MachineImageFile *image_file,
    MachineElfTargetProfile profile,
    MachineImageError *error);
static int machine_image_report_get_symbol_filter_storage(const MachineImageReport *report,
    MachineImageReportSymbolFilterKind filter_kind,
    const size_t **out_indices,
    size_t *out_count);
static int machine_image_report_get_relocation_filter_storage(const MachineImageReport *report,
    MachineImageReportRelocationFilterKind filter_kind,
    const size_t **out_indices,
    size_t *out_count);
static const char *machine_image_symbol_filter_kind_name(MachineImageReportSymbolFilterKind filter_kind);
static const char *machine_image_relocation_filter_kind_name(MachineImageReportRelocationFilterKind filter_kind);

static void machine_image_set_error(MachineImageError *error, int line, int column, const char *fmt, ...) {
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

static char *machine_image_strdup(const char *text) {
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

static int machine_image_append_format(MachineImageStringBuilder *builder, const char *fmt, ...) {
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

    required_length = builder->length + (size_t)needed + 1u;
    if (required_length > builder->capacity) {
        size_t next_capacity = builder->capacity == 0u ? 128u : builder->capacity;

        while (next_capacity < required_length) {
            if (next_capacity > ((size_t)-1) / 2u) {
                va_end(args);
                return 0;
            }
            next_capacity *= 2u;
        }

        new_data = (char *)realloc(builder->data, next_capacity);
        if (!new_data) {
            va_end(args);
            return 0;
        }
        builder->data = new_data;
        builder->capacity = next_capacity;
    }

    (void)vsnprintf(builder->data + builder->length, builder->capacity - builder->length, fmt, args);
    va_end(args);
    builder->length += (size_t)needed;
    return 1;
}

static size_t machine_image_base_virtual_address(MachineElfTargetProfile profile) {
    switch (profile) {
        case MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW:
            return 0x10000u;
        case MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW:
            return 0x08048000u;
        case MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32:
        default:
            return 0x1000u;
    }
}

static size_t machine_image_segment_alignment(MachineElfTargetProfile profile) {
    (void)profile;
    return 0x1000u;
}

static const char *machine_image_relocation_kind_name(MachineRelocKind kind) {
    switch (kind) {
        case MACHINE_BYTES_FIXUP_CALL_TARGET:
            return "call";
        case MACHINE_BYTES_FIXUP_CONTROL_PRIMARY:
            return "ctrl-primary";
        case MACHINE_BYTES_FIXUP_CONTROL_SECONDARY:
            return "ctrl-secondary";
        default:
            return "unknown";
    }
}

static int machine_image_report_get_symbol_filter_storage(const MachineImageReport *report,
    MachineImageReportSymbolFilterKind filter_kind,
    const size_t **out_indices,
    size_t *out_count) {
    if (out_indices) {
        *out_indices = NULL;
    }
    if (out_count) {
        *out_count = 0u;
    }
    if (!report || !out_indices || !out_count) {
        return 0;
    }
    switch (filter_kind) {
        case MACHINE_IMAGE_SYMBOL_FILTER_DEFINED:
            *out_indices = report->defined_symbol_indices;
            *out_count = report->defined_symbol_count;
            return 1;
        case MACHINE_IMAGE_SYMBOL_FILTER_UNDEFINED:
            *out_indices = report->undefined_symbol_indices;
            *out_count = report->undefined_symbol_count;
            return 1;
        case MACHINE_IMAGE_SYMBOL_FILTER_GLOBAL:
            *out_indices = report->global_symbol_indices;
            *out_count = report->global_symbol_count;
            return 1;
        case MACHINE_IMAGE_SYMBOL_FILTER_LOCAL:
            *out_indices = report->local_symbol_indices;
            *out_count = report->local_symbol_count;
            return 1;
        case MACHINE_IMAGE_SYMBOL_FILTER_DEFINED_GLOBAL:
            *out_indices = report->defined_global_symbol_indices;
            *out_count = report->defined_global_symbol_count;
            return 1;
        case MACHINE_IMAGE_SYMBOL_FILTER_UNDEFINED_GLOBAL:
            *out_indices = report->undefined_global_symbol_indices;
            *out_count = report->undefined_global_symbol_count;
            return 1;
        default:
            return 0;
    }
}

static int machine_image_report_get_relocation_filter_storage(const MachineImageReport *report,
    MachineImageReportRelocationFilterKind filter_kind,
    const size_t **out_indices,
    size_t *out_count) {
    if (out_indices) {
        *out_indices = NULL;
    }
    if (out_count) {
        *out_count = 0u;
    }
    if (!report || !out_indices || !out_count) {
        return 0;
    }
    switch (filter_kind) {
        case MACHINE_IMAGE_RELOCATION_FILTER_RESOLVED:
            *out_indices = report->resolved_relocation_indices;
            *out_count = report->resolved_relocation_count;
            return 1;
        case MACHINE_IMAGE_RELOCATION_FILTER_UNRESOLVED:
            *out_indices = report->unresolved_relocation_indices;
            *out_count = report->unresolved_relocation_count;
            return 1;
        case MACHINE_IMAGE_RELOCATION_FILTER_CALL:
            *out_indices = report->call_relocation_indices;
            *out_count = report->call_relocation_count;
            return 1;
        case MACHINE_IMAGE_RELOCATION_FILTER_CONTROL_PRIMARY:
            *out_indices = report->control_primary_relocation_indices;
            *out_count = report->control_primary_relocation_count;
            return 1;
        case MACHINE_IMAGE_RELOCATION_FILTER_CONTROL_SECONDARY:
            *out_indices = report->control_secondary_relocation_indices;
            *out_count = report->control_secondary_relocation_count;
            return 1;
        default:
            return 0;
    }
}

static const char *machine_image_symbol_filter_kind_name(MachineImageReportSymbolFilterKind filter_kind) {
    switch (filter_kind) {
        case MACHINE_IMAGE_SYMBOL_FILTER_DEFINED:
            return "defined";
        case MACHINE_IMAGE_SYMBOL_FILTER_UNDEFINED:
            return "undefined";
        case MACHINE_IMAGE_SYMBOL_FILTER_GLOBAL:
            return "global";
        case MACHINE_IMAGE_SYMBOL_FILTER_LOCAL:
            return "local";
        case MACHINE_IMAGE_SYMBOL_FILTER_DEFINED_GLOBAL:
            return "defined-global";
        case MACHINE_IMAGE_SYMBOL_FILTER_UNDEFINED_GLOBAL:
            return "undefined-global";
        default:
            return "unknown";
    }
}

static const char *machine_image_relocation_filter_kind_name(MachineImageReportRelocationFilterKind filter_kind) {
    switch (filter_kind) {
        case MACHINE_IMAGE_RELOCATION_FILTER_RESOLVED:
            return "resolved";
        case MACHINE_IMAGE_RELOCATION_FILTER_UNRESOLVED:
            return "unresolved";
        case MACHINE_IMAGE_RELOCATION_FILTER_CALL:
            return "call";
        case MACHINE_IMAGE_RELOCATION_FILTER_CONTROL_PRIMARY:
            return "control-primary";
        case MACHINE_IMAGE_RELOCATION_FILTER_CONTROL_SECONDARY:
            return "control-secondary";
        default:
            return "unknown";
    }
}

#define MACHINE_IMAGE_SPLIT_AGGREGATOR
#include "machine_image_core.inc"
#include "machine_image_build.inc"
#include "machine_image_verify.inc"
#include "machine_image_query_dump.inc"
