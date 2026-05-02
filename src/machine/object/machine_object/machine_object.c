#include "machine/object.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineObjectStringBuilder;

static void machine_object_set_error(MachineObjectError *error, int line, int column, const char *fmt, ...);
static char *machine_object_strdup(const char *text);
static int machine_object_append_format(MachineObjectStringBuilder *builder, const char *fmt, ...);

static void machine_object_set_error(MachineObjectError *error, int line, int column, const char *fmt, ...) {
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

static char *machine_object_strdup(const char *text) {
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

static int machine_object_append_format(MachineObjectStringBuilder *builder, const char *fmt, ...) {
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

void machine_object_file_init(MachineObjectFile *object_file) {
    if (!object_file) {
        return;
    }
    object_file->sections = NULL;
    object_file->section_count = 0;
    object_file->section_capacity = 0;
    object_file->symbols = NULL;
    object_file->symbol_count = 0;
    object_file->symbol_capacity = 0;
    object_file->fixups = NULL;
    object_file->fixup_count = 0;
    object_file->fixup_capacity = 0;
    object_file->total_byte_count = 0;
}

void machine_object_file_free(MachineObjectFile *object_file) {
    size_t section_index;
    size_t symbol_index;
    size_t fixup_index;

    if (!object_file) {
        return;
    }
    for (section_index = 0; section_index < object_file->section_count; ++section_index) {
        free(object_file->sections[section_index].name);
        free(object_file->sections[section_index].bytes);
    }
    for (symbol_index = 0; symbol_index < object_file->symbol_count; ++symbol_index) {
        free(object_file->symbols[symbol_index].name);
    }
    for (fixup_index = 0; fixup_index < object_file->fixup_count; ++fixup_index) {
        free(object_file->fixups[fixup_index].source_label_name);
        free(object_file->fixups[fixup_index].target_name);
    }
    free(object_file->sections);
    free(object_file->symbols);
    free(object_file->fixups);
    machine_object_file_init(object_file);
}

int machine_object_file_get_summary(const MachineObjectFile *object_file,
    size_t *out_total_byte_count,
    size_t *out_section_count,
    size_t *out_symbol_count,
    size_t *out_fixup_count) {
    if (!object_file) {
        return 0;
    }
    if (out_total_byte_count) {
        *out_total_byte_count = object_file->total_byte_count;
    }
    if (out_section_count) {
        *out_section_count = object_file->section_count;
    }
    if (out_symbol_count) {
        *out_symbol_count = object_file->symbol_count;
    }
    if (out_fixup_count) {
        *out_fixup_count = object_file->fixup_count;
    }
    return 1;
}

int machine_object_file_get_section(const MachineObjectFile *object_file,
    size_t section_index,
    const MachineObjectSection **out_section) {
    if (!object_file || !out_section || section_index >= object_file->section_count || !object_file->sections) {
        return 0;
    }
    *out_section = &object_file->sections[section_index];
    return 1;
}

int machine_object_file_find_section_by_name(const MachineObjectFile *object_file,
    const char *section_name,
    size_t *out_section_index,
    const MachineObjectSection **out_section) {
    size_t section_index;

    if (!object_file || !section_name || !object_file->sections) {
        return 0;
    }
    for (section_index = 0; section_index < object_file->section_count; ++section_index) {
        if (object_file->sections[section_index].name &&
            strcmp(object_file->sections[section_index].name, section_name) == 0) {
            if (out_section_index) {
                *out_section_index = section_index;
            }
            if (out_section) {
                *out_section = &object_file->sections[section_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_object_file_copy_section_bytes(const MachineObjectFile *object_file,
    size_t section_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineObjectError *error) {
    const MachineObjectSection *section;
    unsigned char *bytes;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0;
    }
    if (!object_file || !out_bytes || !machine_object_file_get_section(object_file, section_index, &section) || !section) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-100: invalid section byte-copy contract");
        return 0;
    }
    if (section->byte_count == 0) {
        return 1;
    }
    bytes = (unsigned char *)malloc(section->byte_count);
    if (!bytes) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-101: out of memory copying section bytes");
        return 0;
    }
    memcpy(bytes, section->bytes, section->byte_count);
    *out_bytes = bytes;
    if (out_byte_count) {
        *out_byte_count = section->byte_count;
    }
    return 1;
}

int machine_object_file_get_section_symbols(const MachineObjectFile *object_file,
    size_t section_index,
    size_t *out_count,
    const MachineObjectSymbol **out_symbols) {
    const MachineObjectSection *section;

    if (out_count) {
        *out_count = 0;
    }
    if (out_symbols) {
        *out_symbols = NULL;
    }
    if (!object_file || !machine_object_file_get_section(object_file, section_index, &section) || !section ||
        !object_file->symbols) {
        return 0;
    }
    if (out_count) {
        *out_count = section->symbol_count;
    }
    if (out_symbols) {
        *out_symbols = object_file->symbols + section->symbol_start_index;
    }
    return 1;
}

int machine_object_file_get_section_fixups(const MachineObjectFile *object_file,
    size_t section_index,
    size_t *out_count,
    const MachineObjectFixup **out_fixups) {
    const MachineObjectSection *section;

    if (out_count) {
        *out_count = 0;
    }
    if (out_fixups) {
        *out_fixups = NULL;
    }
    if (!object_file || !machine_object_file_get_section(object_file, section_index, &section) || !section ||
        !object_file->fixups) {
        return 0;
    }
    if (out_count) {
        *out_count = section->fixup_count;
    }
    if (out_fixups) {
        *out_fixups = object_file->fixups + section->fixup_start_index;
    }
    return 1;
}

int machine_object_file_get_symbol(const MachineObjectFile *object_file,
    size_t symbol_index,
    const MachineObjectSymbol **out_symbol) {
    if (!object_file || !out_symbol || symbol_index >= object_file->symbol_count || !object_file->symbols) {
        return 0;
    }
    *out_symbol = &object_file->symbols[symbol_index];
    return 1;
}

int machine_object_file_find_symbol_by_name(const MachineObjectFile *object_file,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineObjectSymbol **out_symbol) {
    size_t symbol_index;

    if (!object_file || !symbol_name || !object_file->symbols) {
        return 0;
    }
    for (symbol_index = 0; symbol_index < object_file->symbol_count; ++symbol_index) {
        if (object_file->symbols[symbol_index].name &&
            strcmp(object_file->symbols[symbol_index].name, symbol_name) == 0) {
            if (out_symbol_index) {
                *out_symbol_index = symbol_index;
            }
            if (out_symbol) {
                *out_symbol = &object_file->symbols[symbol_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_object_file_get_fixup(const MachineObjectFile *object_file,
    size_t fixup_index,
    const MachineObjectFixup **out_fixup) {
    if (!object_file || !out_fixup || fixup_index >= object_file->fixup_count || !object_file->fixups) {
        return 0;
    }
    *out_fixup = &object_file->fixups[fixup_index];
    return 1;
}

int machine_object_section_get_summary(const MachineObjectSection *section,
    MachineObjectSectionSummary *out_summary) {
    if (!section || !out_summary) {
        return 0;
    }
    out_summary->kind = section->kind;
    out_summary->name = section->name;
    out_summary->start_byte_offset = section->start_byte_offset;
    out_summary->end_byte_offset = section->end_byte_offset;
    out_summary->byte_count = section->byte_count;
    out_summary->function_count = section->function_count;
    out_summary->block_count = section->block_count;
    out_summary->symbol_count = section->symbol_count;
    out_summary->fixup_count = section->fixup_count;
    return 1;
}

int machine_object_symbol_get_summary(const MachineObjectSymbol *symbol,
    MachineObjectSymbolSummary *out_summary) {
    if (!symbol || !out_summary) {
        return 0;
    }
    out_summary->kind = symbol->kind;
    out_summary->name = symbol->name;
    out_summary->is_defined = symbol->is_defined;
    out_summary->has_section_index = symbol->has_section_index;
    out_summary->section_index = symbol->section_index;
    out_summary->byte_offset_in_section = symbol->byte_offset_in_section;
    out_summary->byte_count = symbol->byte_count;
    out_summary->incoming_fixup_count = symbol->incoming_fixup_count;
    out_summary->has_function_index = symbol->has_function_index;
    out_summary->function_index = symbol->function_index;
    out_summary->has_emit_index = symbol->has_emit_index;
    out_summary->emit_index = symbol->emit_index;
    return 1;
}

int machine_object_fixup_get_summary(const MachineObjectFixup *fixup,
    MachineObjectFixupSummary *out_summary) {
    if (!fixup || !out_summary) {
        return 0;
    }
    out_summary->kind = fixup->kind;
    out_summary->target_kind = fixup->target_kind;
    out_summary->section_index = fixup->section_index;
    out_summary->source_label_name = fixup->source_label_name;
    out_summary->owner_byte_offset = fixup->owner_byte_offset;
    out_summary->owner_byte_count = fixup->owner_byte_count;
    out_summary->patch_byte_offset = fixup->patch_byte_offset;
    out_summary->patch_byte_count = fixup->patch_byte_count;
    out_summary->target_name = fixup->target_name;
    out_summary->has_target_function_index = fixup->has_target_function_index;
    out_summary->target_function_index = fixup->target_function_index;
    out_summary->has_target_emit_index = fixup->has_target_emit_index;
    out_summary->target_emit_index = fixup->target_emit_index;
    out_summary->has_target_byte_offset = fixup->has_target_byte_offset;
    out_summary->target_byte_offset = fixup->target_byte_offset;
    out_summary->has_target_symbol_index = fixup->has_target_symbol_index;
    out_summary->target_symbol_index = fixup->target_symbol_index;
    return 1;
}

int machine_object_build_from_machine_bytes_report(const MachineBytesReport *report,
    MachineObjectFile *out_object_file,
    MachineObjectError *error) {
    unsigned char *program_bytes = NULL;
    size_t program_byte_count = 0;
    size_t section_index;
    size_t symbol_index;
    size_t fixup_index;

    if (!report || !out_object_file) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-102: invalid bytes-report object build contract");
        return 0;
    }

    machine_object_file_free(out_object_file);

    if (!machine_bytes_report_copy_bytes(report, &program_bytes, &program_byte_count, NULL)) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-103: failed to flatten byte image");
        return 0;
    }

    if (report->total_section_summary_count > 0) {
        out_object_file->sections =
            (MachineObjectSection *)calloc(report->total_section_summary_count, sizeof(MachineObjectSection));
        if (!out_object_file->sections) {
            free(program_bytes);
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-104: out of memory copying sections");
            return 0;
        }
        out_object_file->section_count = report->total_section_summary_count;
        out_object_file->section_capacity = report->total_section_summary_count;
        for (section_index = 0; section_index < report->total_section_summary_count; ++section_index) {
            const MachineBytesSectionSummary *src = &report->section_summaries[section_index];
            MachineObjectSection *dest = &out_object_file->sections[section_index];

            dest->kind = src->kind;
            dest->name = machine_object_strdup(src->name);
            dest->start_byte_offset = src->start_byte_offset;
            dest->end_byte_offset = src->end_byte_offset;
            dest->byte_count = src->byte_count;
            dest->function_count = src->function_count;
            dest->block_count = src->block_count;
            dest->symbol_start_index = src->symbol_start_index;
            dest->symbol_count = src->symbol_count;
            dest->fixup_start_index = src->fixup_start_index;
            dest->fixup_count = src->fixup_count;
            if (src->name && !dest->name) {
                free(program_bytes);
                machine_object_file_free(out_object_file);
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-105: out of memory copying section name");
                return 0;
            }
            if (src->byte_count > 0) {
                dest->bytes = (unsigned char *)malloc(src->byte_count);
                if (!dest->bytes) {
                    free(program_bytes);
                    machine_object_file_free(out_object_file);
                    machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-106: out of memory copying section bytes");
                    return 0;
                }
                memcpy(dest->bytes, program_bytes + src->start_byte_offset, src->byte_count);
            }
        }
    }

    if (report->total_symbol_summary_count > 0) {
        out_object_file->symbols =
            (MachineObjectSymbol *)calloc(report->total_symbol_summary_count, sizeof(MachineObjectSymbol));
        if (!out_object_file->symbols) {
            free(program_bytes);
            machine_object_file_free(out_object_file);
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-107: out of memory copying symbols");
            return 0;
        }
        out_object_file->symbol_count = report->total_symbol_summary_count;
        out_object_file->symbol_capacity = report->total_symbol_summary_count;
        for (symbol_index = 0; symbol_index < report->total_symbol_summary_count; ++symbol_index) {
            const MachineBytesSymbolSummary *src = &report->symbol_summaries[symbol_index];
            MachineObjectSymbol *dest = &out_object_file->symbols[symbol_index];

            dest->kind = src->kind;
            dest->name = machine_object_strdup(src->name);
            dest->is_defined = src->is_defined;
            dest->has_section_index = src->is_defined;
            dest->section_index = src->is_defined ? 0u : 0u;
            dest->byte_offset_in_section = src->has_byte_offset ? src->byte_offset : 0;
            dest->byte_count = src->byte_count;
            dest->incoming_fixup_count = src->incoming_fixup_count;
            dest->has_function_index = src->has_function_index;
            dest->function_index = src->function_index;
            dest->has_emit_index = src->has_emit_index;
            dest->emit_index = src->emit_index;
            if (src->name && !dest->name) {
                free(program_bytes);
                machine_object_file_free(out_object_file);
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-108: out of memory copying symbol name");
                return 0;
            }
        }
    }

    if (report->total_fixup_summary_count > 0) {
        out_object_file->fixups =
            (MachineObjectFixup *)calloc(report->total_fixup_summary_count, sizeof(MachineObjectFixup));
        if (!out_object_file->fixups) {
            free(program_bytes);
            machine_object_file_free(out_object_file);
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-109: out of memory copying fixups");
            return 0;
        }
        out_object_file->fixup_count = report->total_fixup_summary_count;
        out_object_file->fixup_capacity = report->total_fixup_summary_count;
        for (fixup_index = 0; fixup_index < report->total_fixup_summary_count; ++fixup_index) {
            const MachineBytesFixupSummary *src = &report->fixup_summaries[fixup_index];
            MachineObjectFixup *dest = &out_object_file->fixups[fixup_index];

            dest->kind = src->kind;
            dest->target_kind = src->target_kind;
            dest->section_index = 0;
            dest->source_label_name = machine_object_strdup(src->source_label_name);
            dest->owner_byte_offset = src->owner_byte_offset;
            dest->owner_byte_count = src->owner_byte_count;
            dest->patch_byte_offset = src->patch_byte_offset;
            dest->patch_byte_count = src->patch_byte_count;
            dest->target_name = machine_object_strdup(src->target_name);
            dest->has_target_function_index = src->has_target_function_index;
            dest->target_function_index = src->target_function_index;
            dest->has_target_emit_index = src->has_target_emit_index;
            dest->target_emit_index = src->target_emit_index;
            dest->has_target_byte_offset = src->has_target_byte_offset;
            dest->target_byte_offset = src->target_byte_offset;
            dest->has_target_symbol_index = src->has_target_symbol_index;
            dest->target_symbol_index = src->target_symbol_index;
            if ((src->source_label_name && !dest->source_label_name) || (src->target_name && !dest->target_name)) {
                free(program_bytes);
                machine_object_file_free(out_object_file);
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-110: out of memory copying fixup text");
                return 0;
            }
        }
    }

    out_object_file->total_byte_count = report->total_program_byte_count;
    free(program_bytes);
    return machine_object_verify_file(out_object_file, error);
}

int machine_object_build_from_machine_bytes_program(const MachineBytesProgram *program,
    MachineObjectFile *out_object_file,
    MachineObjectError *error) {
    MachineBytesReport report;
    MachineBytesError bytes_error;
    int ok;

    if (!program || !out_object_file) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-111: invalid bytes-program object build contract");
        return 0;
    }
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_bytes_report_init(&report);
    ok = machine_bytes_build_report_from_program(program, &report, &bytes_error);
    if (!ok) {
        machine_object_set_error(error, bytes_error.line, bytes_error.column, "%s", bytes_error.message);
        machine_bytes_report_free(&report);
        return 0;
    }
    ok = machine_object_build_from_machine_bytes_report(&report, out_object_file, error);
    machine_bytes_report_free(&report);
    return ok;
}

int machine_object_build_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineBytesTargetProfile profile,
    MachineObjectFile *out_object_file,
    MachineObjectError *error) {
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    int ok;

    if (!report || !out_object_file) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-112: invalid machine-ir report object build contract");
        return 0;
    }
    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_bytes_report_init(&bytes_report);
    ok = machine_bytes_build_report_from_machine_ir_report_with_profile(
        report, profile, &bytes_report, &bytes_error);
    if (!ok) {
        machine_object_set_error(error, bytes_error.line, bytes_error.column, "%s", bytes_error.message);
        machine_bytes_report_free(&bytes_report);
        return 0;
    }
    ok = machine_object_build_from_machine_bytes_report(&bytes_report, out_object_file, error);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

int machine_object_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineObjectFile *out_object_file,
    MachineObjectError *error) {
    return machine_object_build_from_machine_ir_report_with_profile(
        report,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_object_file,
        error);
}

int machine_object_verify_file(const MachineObjectFile *object_file, MachineObjectError *error) {
    size_t section_index;
    size_t symbol_index;
    size_t fixup_index;
    size_t running_offset = 0;

    if (!object_file) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-113: invalid object verifier contract");
        return 0;
    }
    if ((object_file->section_count > 0 && !object_file->sections) ||
        object_file->section_count > object_file->section_capacity ||
        (object_file->symbol_count > 0 && !object_file->symbols) ||
        object_file->symbol_count > object_file->symbol_capacity ||
        (object_file->fixup_count > 0 && !object_file->fixups) ||
        object_file->fixup_count > object_file->fixup_capacity) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-124: invalid object table metadata");
        return 0;
    }
    for (section_index = 0; section_index < object_file->section_count; ++section_index) {
        const MachineObjectSection *section = &object_file->sections[section_index];

        if (!section->name || section->name[0] == '\0') {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-114: section requires name");
            return 0;
        }
        if (section->start_byte_offset != running_offset ||
            section->end_byte_offset != section->start_byte_offset + section->byte_count ||
            (section->byte_count > 0 && !section->bytes)) {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-115: invalid section span/bytes");
            return 0;
        }
        if (section->symbol_start_index + section->symbol_count > object_file->symbol_count ||
            section->fixup_start_index + section->fixup_count > object_file->fixup_count) {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-125: section slice exceeds object tables");
            return 0;
        }
        running_offset = section->end_byte_offset;
    }
    if (running_offset != object_file->total_byte_count) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-116: total byte count mismatch");
        return 0;
    }
    for (symbol_index = 0; symbol_index < object_file->symbol_count; ++symbol_index) {
        const MachineObjectSymbol *symbol = &object_file->symbols[symbol_index];

        if (!symbol->name || symbol->name[0] == '\0') {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-117: symbol requires name");
            return 0;
        }
        if (symbol->is_defined) {
            if (!symbol->has_section_index || symbol->section_index >= object_file->section_count) {
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-118: defined symbol requires section");
                return 0;
            }
            if (symbol->byte_offset_in_section + symbol->byte_count >
                object_file->sections[symbol->section_index].byte_count) {
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-119: symbol exceeds section span");
                return 0;
            }
        }
    }
    for (fixup_index = 0; fixup_index < object_file->fixup_count; ++fixup_index) {
        const MachineObjectFixup *fixup = &object_file->fixups[fixup_index];
        const MachineObjectSection *section;

        if (!fixup->target_name || fixup->target_name[0] == '\0' ||
            fixup->section_index >= object_file->section_count) {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-120: invalid fixup target/section");
            return 0;
        }
        section = &object_file->sections[fixup->section_index];
        if (fixup->owner_byte_offset + fixup->owner_byte_count > section->byte_count ||
            fixup->patch_byte_offset + fixup->patch_byte_count > section->byte_count) {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-121: fixup exceeds section span");
            return 0;
        }
        if (fixup->has_target_symbol_index && fixup->target_symbol_index >= object_file->symbol_count) {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-122: fixup target symbol out of range");
            return 0;
        }
    }
    return 1;
}

int machine_object_dump_file(const MachineObjectFile *object_file,
    char **out_text,
    MachineObjectError *error) {
    MachineObjectStringBuilder builder;
    size_t section_index;
    size_t symbol_index;
    size_t fixup_index;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!object_file || !out_text) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-123: invalid object dump contract");
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_object_append_format(
            &builder,
            "machine_object total_bytes=%zu sections=%zu symbols=%zu fixups=%zu\nsections:\n",
            object_file->total_byte_count,
            object_file->section_count,
            object_file->symbol_count,
            object_file->fixup_count)) {
        goto cleanup;
    }
    for (section_index = 0; section_index < object_file->section_count; ++section_index) {
        const MachineObjectSection *section = &object_file->sections[section_index];

        if (!machine_object_append_format(
                &builder,
                "  sec.%zu %s span=%zu..%zu bytes=%zu symbols=%zu fixups=%zu\n",
                section_index,
                section->name ? section->name : "<null>",
                section->start_byte_offset,
                section->end_byte_offset,
                section->byte_count,
                section->symbol_count,
                section->fixup_count)) {
            goto cleanup;
        }
    }
    if (!machine_object_append_format(&builder, "symbols:\n")) {
        goto cleanup;
    }
    for (symbol_index = 0; symbol_index < object_file->symbol_count; ++symbol_index) {
        const MachineObjectSymbol *symbol = &object_file->symbols[symbol_index];

        if (!machine_object_append_format(
                &builder,
                "  sym.%zu %s %s defined=%d sec=%s offset=%zu bytes=%zu incoming_fixups=%zu\n",
                symbol_index,
                symbol->kind == MACHINE_BYTES_SYMBOL_FUNCTION ? "function"
                    : (symbol->kind == MACHINE_BYTES_SYMBOL_BLOCK ? "block" : "external"),
                symbol->name ? symbol->name : "<null>",
                symbol->is_defined,
                symbol->has_section_index ? "yes" : "no",
                symbol->byte_offset_in_section,
                symbol->byte_count,
                symbol->incoming_fixup_count)) {
            goto cleanup;
        }
    }
    if (!machine_object_append_format(&builder, "fixups:\n")) {
        goto cleanup;
    }
    for (fixup_index = 0; fixup_index < object_file->fixup_count; ++fixup_index) {
        const MachineObjectFixup *fixup = &object_file->fixups[fixup_index];

        if (!machine_object_append_format(
                &builder,
                "  fixup.%zu %s sec=%zu patch@%zu bytes=%zu target=%s sym=%s\n",
                fixup_index,
                fixup->kind == MACHINE_BYTES_FIXUP_CALL_TARGET ? "call"
                    : (fixup->kind == MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ? "ctrl-primary" : "ctrl-secondary"),
                fixup->section_index,
                fixup->patch_byte_offset,
                fixup->patch_byte_count,
                fixup->target_name ? fixup->target_name : "<null>",
                fixup->has_target_symbol_index ? "yes" : "no")) {
            goto cleanup;
        }
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(builder.data);
    return ok;
}

int machine_object_build_dump_from_machine_bytes_report(const MachineBytesReport *report,
    char **out_text,
    MachineObjectError *error) {
    MachineObjectFile object_file;
    int ok;

    machine_object_file_init(&object_file);
    ok = machine_object_build_from_machine_bytes_report(report, &object_file, error) &&
        machine_object_dump_file(&object_file, out_text, error);
    machine_object_file_free(&object_file);
    return ok;
}

int machine_object_build_dump_from_machine_bytes_program(const MachineBytesProgram *program,
    char **out_text,
    MachineObjectError *error) {
    MachineObjectFile object_file;
    int ok;

    machine_object_file_init(&object_file);
    ok = machine_object_build_from_machine_bytes_program(program, &object_file, error) &&
        machine_object_dump_file(&object_file, out_text, error);
    machine_object_file_free(&object_file);
    return ok;
}

int machine_object_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineObjectError *error) {
    MachineObjectFile object_file;
    int ok;

    machine_object_file_init(&object_file);
    ok = machine_object_build_from_machine_ir_report(report, &object_file, error) &&
        machine_object_dump_file(&object_file, out_text, error);
    machine_object_file_free(&object_file);
    return ok;
}
