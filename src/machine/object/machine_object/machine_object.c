#include "machine/object.h"

#include <stdarg.h>
#include <stdint.h>
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
static int machine_object_target_profile_is_valid(MachineBytesTargetProfile profile);
static const char *machine_object_target_profile_name(MachineBytesTargetProfile profile);
static int machine_object_clone_file(const MachineObjectFile *source,
    MachineObjectFile *out_object_file,
    MachineObjectError *error);
static const MachineEmitGlobal *machine_object_find_global_by_name(const MachineBytesProgram *program, const char *name);
static const char *machine_object_symbol_kind_name(MachineObjectSymbolKind kind);
static int machine_object_copy_section_bytes_from_report(const MachineBytesReport *report,
    const MachineBytesSectionSummary *section,
    unsigned char **out_bytes,
    MachineObjectError *error);

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

static int machine_object_target_profile_is_valid(MachineBytesTargetProfile profile) {
    switch (profile) {
        case MACHINE_BYTES_TARGET_PROFILE_GENERIC:
        case MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW:
        case MACHINE_BYTES_TARGET_PROFILE_I386_PREVIEW:
            return 1;
        default:
            return 0;
    }
}

static const char *machine_object_target_profile_name(MachineBytesTargetProfile profile) {
    switch (profile) {
        case MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW:
            return "riscv32-preview";
        case MACHINE_BYTES_TARGET_PROFILE_I386_PREVIEW:
            return "i386-preview";
        case MACHINE_BYTES_TARGET_PROFILE_GENERIC:
        default:
            return "generic";
    }
}

static int machine_object_clone_file(const MachineObjectFile *source,
    MachineObjectFile *out_object_file,
    MachineObjectError *error) {
    size_t section_index;
    size_t symbol_index;
    size_t fixup_index;

    if (!source || !out_object_file) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-136: invalid object clone contract");
        return 0;
    }

    machine_object_file_free(out_object_file);
    if (!machine_object_verify_file(source, NULL)) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-137: source object file failed verification");
        return 0;
    }
    out_object_file->target_profile = source->target_profile;
    out_object_file->total_byte_count = source->total_byte_count;

    if (source->section_count > 0u) {
        out_object_file->sections = (MachineObjectSection *)calloc(source->section_count, sizeof(MachineObjectSection));
        if (!out_object_file->sections) {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-138: out of memory cloning sections");
            machine_object_file_free(out_object_file);
            return 0;
        }
        out_object_file->section_count = source->section_count;
        out_object_file->section_capacity = source->section_count;
        for (section_index = 0u; section_index < source->section_count; ++section_index) {
            out_object_file->sections[section_index] = source->sections[section_index];
            out_object_file->sections[section_index].name =
                machine_object_strdup(source->sections[section_index].name);
            out_object_file->sections[section_index].bytes = NULL;
            if (source->sections[section_index].name && !out_object_file->sections[section_index].name) {
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-139: out of memory cloning section name");
                machine_object_file_free(out_object_file);
                return 0;
            }
            if (source->sections[section_index].byte_count > 0u) {
                out_object_file->sections[section_index].bytes =
                    (unsigned char *)malloc(source->sections[section_index].byte_count);
                if (!out_object_file->sections[section_index].bytes) {
                    machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-140: out of memory cloning section bytes");
                    machine_object_file_free(out_object_file);
                    return 0;
                }
                memcpy(out_object_file->sections[section_index].bytes,
                    source->sections[section_index].bytes,
                    source->sections[section_index].byte_count);
            }
        }
    }

    if (source->symbol_count > 0u) {
        out_object_file->symbols = (MachineObjectSymbol *)calloc(source->symbol_count, sizeof(MachineObjectSymbol));
        if (!out_object_file->symbols) {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-141: out of memory cloning symbols");
            machine_object_file_free(out_object_file);
            return 0;
        }
        out_object_file->symbol_count = source->symbol_count;
        out_object_file->symbol_capacity = source->symbol_count;
        for (symbol_index = 0u; symbol_index < source->symbol_count; ++symbol_index) {
            out_object_file->symbols[symbol_index] = source->symbols[symbol_index];
            out_object_file->symbols[symbol_index].name = machine_object_strdup(source->symbols[symbol_index].name);
            if (source->symbols[symbol_index].name && !out_object_file->symbols[symbol_index].name) {
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-142: out of memory cloning symbol name");
                machine_object_file_free(out_object_file);
                return 0;
            }
        }
    }

    if (source->fixup_count > 0u) {
        out_object_file->fixups = (MachineObjectFixup *)calloc(source->fixup_count, sizeof(MachineObjectFixup));
        if (!out_object_file->fixups) {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-143: out of memory cloning fixups");
            machine_object_file_free(out_object_file);
            return 0;
        }
        out_object_file->fixup_count = source->fixup_count;
        out_object_file->fixup_capacity = source->fixup_count;
        for (fixup_index = 0u; fixup_index < source->fixup_count; ++fixup_index) {
            out_object_file->fixups[fixup_index] = source->fixups[fixup_index];
            out_object_file->fixups[fixup_index].source_label_name =
                machine_object_strdup(source->fixups[fixup_index].source_label_name);
            out_object_file->fixups[fixup_index].target_name =
                machine_object_strdup(source->fixups[fixup_index].target_name);
            if ((source->fixups[fixup_index].source_label_name &&
                    !out_object_file->fixups[fixup_index].source_label_name) ||
                (source->fixups[fixup_index].target_name &&
                    !out_object_file->fixups[fixup_index].target_name)) {
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-144: out of memory cloning fixup text");
                machine_object_file_free(out_object_file);
                return 0;
            }
        }
    }

    return 1;
}

static const MachineEmitGlobal *machine_object_find_global_by_name(const MachineBytesProgram *program, const char *name) {
    size_t global_index;

    if (!program || !name || !program->globals) {
        return NULL;
    }
    for (global_index = 0u; global_index < program->global_count; ++global_index) {
        const MachineEmitGlobal *global = &program->globals[global_index];

        if (global->name && strcmp(global->name, name) == 0) {
            return global;
        }
    }
    return NULL;
}

static const char *machine_object_symbol_kind_name(MachineObjectSymbolKind kind) {
    switch (kind) {
        case MACHINE_BYTES_SYMBOL_FUNCTION:
            return "function";
        case MACHINE_BYTES_SYMBOL_BLOCK:
            return "block";
        case MACHINE_BYTES_SYMBOL_GLOBAL_OBJECT:
            return "global";
        case MACHINE_BYTES_SYMBOL_EXTERNAL:
        default:
            return "external";
    }
}

static const char *machine_object_fixup_kind_name(MachineObjectFixupKind kind) {
    switch (kind) {
        case MACHINE_BYTES_FIXUP_CALL_TARGET:
            return "call";
        case MACHINE_BYTES_FIXUP_CONTROL_PRIMARY:
            return "ctrl-primary";
        case MACHINE_BYTES_FIXUP_CONTROL_SECONDARY:
            return "ctrl-secondary";
        case MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET:
            return "data-addr";
        case MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET:
            return "data-load";
        case MACHINE_BYTES_FIXUP_DATA_STORE_TARGET:
            return "data-store";
        default:
            return "unknown";
    }
}

static int machine_object_copy_section_bytes_from_report(const MachineBytesReport *report,
    const MachineBytesSectionSummary *section,
    unsigned char **out_bytes,
    MachineObjectError *error) {
    unsigned char *bytes = NULL;
    size_t section_index = 0u;
    size_t symbol_count = 0u;
    const MachineBytesSymbolSummary *symbols = NULL;
    size_t symbol_index;
    size_t byte_cursor = 0u;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (!report || !section || !out_bytes) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-131: invalid section-byte synthesis contract");
        return 0;
    }
    if (section->byte_count == 0u) {
        return 1;
    }
    bytes = (unsigned char *)calloc(section->byte_count, 1u);
    if (!bytes) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-132: out of memory copying section bytes");
        return 0;
    }
    if (section->kind == MACHINE_BYTES_SECTION_TEXT) {
        unsigned char *program_bytes = NULL;
        size_t program_byte_count = 0u;

        if (!machine_bytes_report_copy_bytes(report, &program_bytes, &program_byte_count, NULL) ||
            !program_bytes || section->start_byte_offset + section->byte_count > program_byte_count) {
            free(program_bytes);
            free(bytes);
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-133: invalid text section byte slice");
            return 0;
        }
        memcpy(bytes, program_bytes + section->start_byte_offset, section->byte_count);
        free(program_bytes);
        *out_bytes = bytes;
        return 1;
    }

    section_index = (size_t)(section - report->section_summaries);
    if (!machine_bytes_report_get_section_symbol_summaries(report, section_index, &symbol_count, &symbols) ||
        (symbol_count > 0u && !symbols)) {
        free(bytes);
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-134: invalid global section symbol slice");
        return 0;
    }
    for (symbol_index = 0u; symbol_index < symbol_count; ++symbol_index) {
        const MachineBytesSymbolSummary *symbol = &symbols[symbol_index];
        const MachineEmitGlobal *global = machine_object_find_global_by_name(&report->program, symbol->name);
        uint32_t value;

        if (!global || byte_cursor + 4u > section->byte_count) {
            free(bytes);
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-135: invalid global section materialization");
            return 0;
        }
        value = global->has_initializer ? (uint32_t)global->initializer_value : 0u;
        bytes[byte_cursor + 0u] = (unsigned char)(value & 0xFFu);
        bytes[byte_cursor + 1u] = (unsigned char)((value >> 8u) & 0xFFu);
        bytes[byte_cursor + 2u] = (unsigned char)((value >> 16u) & 0xFFu);
        bytes[byte_cursor + 3u] = (unsigned char)((value >> 24u) & 0xFFu);
        byte_cursor += 4u;
    }
    *out_bytes = bytes;
    return 1;
}

void machine_object_file_init(MachineObjectFile *object_file) {
    if (!object_file) {
        return;
    }
    object_file->target_profile = MACHINE_BYTES_TARGET_PROFILE_GENERIC;
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

void machine_object_report_init(MachineObjectReport *report) {
    if (!report) {
        return;
    }
    machine_object_file_init(&report->file);
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    memset(&report->fixup_family_summary, 0, sizeof(report->fixup_family_summary));
    report->section_summaries = NULL;
    report->section_summary_count = 0u;
    report->symbol_summaries = NULL;
    report->symbol_summary_count = 0u;
    report->fixup_summaries = NULL;
    report->fixup_summary_count = 0u;
}

void machine_object_report_free(MachineObjectReport *report) {
    if (!report) {
        return;
    }
    machine_object_file_free(&report->file);
    free(report->section_summaries);
    free(report->symbol_summaries);
    free(report->fixup_summaries);
    machine_object_report_init(report);
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

int machine_object_get_target_policy_summary(MachineBytesTargetProfile profile,
    MachineObjectTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->target_profile = profile;
    if (!machine_bytes_get_target_policy_summary(profile, &out_summary->bytes_policy)) {
        return 0;
    }
    out_summary->preserves_preview_target_byte_offsets =
        profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW;
    out_summary->preserves_direct_fallthrough_honesty =
        profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW;
    return 1;
}

int machine_object_file_get_target_policy_summary(const MachineObjectFile *object_file,
    MachineObjectTargetPolicySummary *out_summary) {
    if (!object_file || !out_summary) {
        return 0;
    }
    return machine_object_get_target_policy_summary(object_file->target_profile, out_summary);
}

int machine_object_file_get_fixup_family_summary(const MachineObjectFile *object_file,
    MachineObjectFixupFamilySummary *out_summary) {
    size_t fixup_index;

    if (!object_file || !out_summary) {
        return 0;
    }
    memset(out_summary, 0, sizeof(*out_summary));
    for (fixup_index = 0u; fixup_index < object_file->fixup_count; ++fixup_index) {
        switch (object_file->fixups[fixup_index].kind) {
            case MACHINE_BYTES_FIXUP_CALL_TARGET:
                out_summary->call_fixup_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_CONTROL_PRIMARY:
                out_summary->primary_control_fixup_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_CONTROL_SECONDARY:
                out_summary->secondary_control_fixup_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET:
                out_summary->data_address_fixup_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET:
                out_summary->data_load_fixup_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_DATA_STORE_TARGET:
                out_summary->data_store_fixup_count += 1u;
                break;
            default:
                break;
        }
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
    size_t section_index;
    size_t symbol_index;
    size_t fixup_index;

    if (!report || !out_object_file) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-102: invalid bytes-report object build contract");
        return 0;
    }

    machine_object_file_free(out_object_file);
    out_object_file->target_profile = report->program.target_profile;

    if (report->total_section_summary_count > 0) {
        out_object_file->sections =
            (MachineObjectSection *)calloc(report->total_section_summary_count, sizeof(MachineObjectSection));
        if (!out_object_file->sections) {
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
                machine_object_file_free(out_object_file);
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-105: out of memory copying section name");
                return 0;
            }
            if (src->byte_count > 0) {
                if (!machine_object_copy_section_bytes_from_report(report, src, &dest->bytes, error)) {
                    machine_object_file_free(out_object_file);
                    if (!error || error->message[0] == '\0') {
                        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-106: out of memory copying section bytes");
                    }
                    return 0;
                }
            }
        }
    }

    if (report->total_symbol_summary_count > 0) {
        out_object_file->symbols =
            (MachineObjectSymbol *)calloc(report->total_symbol_summary_count, sizeof(MachineObjectSymbol));
        if (!out_object_file->symbols) {
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
            dest->has_section_index = src->has_section_index;
            dest->section_index = src->has_section_index ? src->section_index : 0u;
            dest->byte_offset_in_section = src->has_byte_offset ? src->byte_offset : 0;
            dest->byte_count = src->byte_count;
            dest->incoming_fixup_count = src->incoming_fixup_count;
            dest->has_function_index = src->has_function_index;
            dest->function_index = src->function_index;
            dest->has_emit_index = src->has_emit_index;
            dest->emit_index = src->emit_index;
            if (src->name && !dest->name) {
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
                machine_object_file_free(out_object_file);
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-110: out of memory copying fixup text");
                return 0;
            }
        }
    }

    if (out_object_file->section_count > 0u) {
        out_object_file->total_byte_count =
            out_object_file->sections[out_object_file->section_count - 1u].end_byte_offset;
    }
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

int machine_object_build_report_from_file(const MachineObjectFile *source,
    MachineObjectReport *out_report,
    MachineObjectError *error) {
    size_t section_index;
    size_t symbol_index;
    size_t fixup_index;

    if (!source || !out_report) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-145: invalid object report build contract");
        return 0;
    }

    machine_object_report_free(out_report);
    if (!machine_object_clone_file(source, &out_report->file, error)) {
        return 0;
    }
    if (!machine_object_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_object_file_get_fixup_family_summary(&out_report->file, &out_report->fixup_family_summary)) {
        machine_object_report_free(out_report);
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-146: failed to compute object report summaries");
        return 0;
    }
    if (out_report->file.section_count > 0u) {
        out_report->section_summaries =
            (MachineObjectSectionSummary *)calloc(out_report->file.section_count, sizeof(MachineObjectSectionSummary));
        if (!out_report->section_summaries) {
            machine_object_report_free(out_report);
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-147: out of memory building object section summaries");
            return 0;
        }
        out_report->section_summary_count = out_report->file.section_count;
        for (section_index = 0u; section_index < out_report->file.section_count; ++section_index) {
            if (!machine_object_section_get_summary(&out_report->file.sections[section_index],
                    &out_report->section_summaries[section_index])) {
                machine_object_report_free(out_report);
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-148: failed to summarize object section");
                return 0;
            }
        }
    }
    if (out_report->file.symbol_count > 0u) {
        out_report->symbol_summaries =
            (MachineObjectSymbolSummary *)calloc(out_report->file.symbol_count, sizeof(MachineObjectSymbolSummary));
        if (!out_report->symbol_summaries) {
            machine_object_report_free(out_report);
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-149: out of memory building object symbol summaries");
            return 0;
        }
        out_report->symbol_summary_count = out_report->file.symbol_count;
        for (symbol_index = 0u; symbol_index < out_report->file.symbol_count; ++symbol_index) {
            if (!machine_object_symbol_get_summary(&out_report->file.symbols[symbol_index],
                    &out_report->symbol_summaries[symbol_index])) {
                machine_object_report_free(out_report);
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-150: failed to summarize object symbol");
                return 0;
            }
        }
    }
    if (out_report->file.fixup_count > 0u) {
        out_report->fixup_summaries =
            (MachineObjectFixupSummary *)calloc(out_report->file.fixup_count, sizeof(MachineObjectFixupSummary));
        if (!out_report->fixup_summaries) {
            machine_object_report_free(out_report);
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-151: out of memory building object fixup summaries");
            return 0;
        }
        out_report->fixup_summary_count = out_report->file.fixup_count;
        for (fixup_index = 0u; fixup_index < out_report->file.fixup_count; ++fixup_index) {
            if (!machine_object_fixup_get_summary(&out_report->file.fixups[fixup_index],
                    &out_report->fixup_summaries[fixup_index])) {
                machine_object_report_free(out_report);
                machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-152: failed to summarize object fixup");
                return 0;
            }
        }
    }
    return 1;
}

int machine_object_build_report_from_machine_bytes_report(const MachineBytesReport *report,
    MachineObjectReport *out_report,
    MachineObjectError *error) {
    MachineObjectFile object_file;
    int ok;

    machine_object_file_init(&object_file);
    ok = machine_object_build_from_machine_bytes_report(report, &object_file, error) &&
        machine_object_build_report_from_file(&object_file, out_report, error);
    machine_object_file_free(&object_file);
    return ok;
}

int machine_object_build_report_from_machine_bytes_program(const MachineBytesProgram *program,
    MachineObjectReport *out_report,
    MachineObjectError *error) {
    MachineObjectFile object_file;
    int ok;

    machine_object_file_init(&object_file);
    ok = machine_object_build_from_machine_bytes_program(program, &object_file, error) &&
        machine_object_build_report_from_file(&object_file, out_report, error);
    machine_object_file_free(&object_file);
    return ok;
}

int machine_object_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineObjectReport *out_report,
    MachineObjectError *error) {
    MachineObjectFile object_file;
    int ok;

    machine_object_file_init(&object_file);
    ok = machine_object_build_from_machine_ir_report(report, &object_file, error) &&
        machine_object_build_report_from_file(&object_file, out_report, error);
    machine_object_file_free(&object_file);
    return ok;
}

int machine_object_verify_file(const MachineObjectFile *object_file, MachineObjectError *error) {
    size_t section_index;
    size_t symbol_index;
    size_t fixup_index;
    size_t running_offset = 0;
    MachineObjectTargetPolicySummary target_policy_summary;

    if (!object_file) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-113: invalid object verifier contract");
        return 0;
    }
    if (!machine_object_target_profile_is_valid(object_file->target_profile)) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-126: invalid object target profile");
        return 0;
    }
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    if (!machine_object_file_get_target_policy_summary(object_file, &target_policy_summary)) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-127: invalid object target policy contract");
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
        if (target_policy_summary.preserves_preview_target_byte_offsets &&
            fixup->kind != MACHINE_BYTES_FIXUP_CALL_TARGET &&
            fixup->kind != MACHINE_BYTES_FIXUP_CONTROL_PRIMARY &&
            fixup->kind != MACHINE_BYTES_FIXUP_CONTROL_SECONDARY &&
            fixup->kind != MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET &&
            fixup->kind != MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET &&
            fixup->kind != MACHINE_BYTES_FIXUP_DATA_STORE_TARGET) {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-128: unsupported preview fixup kind");
            return 0;
        }
        if (target_policy_summary.preserves_preview_target_byte_offsets &&
            fixup->has_target_symbol_index &&
            fixup->target_symbol_index < object_file->symbol_count) {
            const MachineObjectSymbol *target_symbol = &object_file->symbols[fixup->target_symbol_index];

            if (target_symbol->is_defined && target_symbol->has_section_index) {
                if (!fixup->has_target_byte_offset ||
                    fixup->target_byte_offset != target_symbol->byte_offset_in_section) {
                    machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-129: preview fixup target byte offset mismatch");
                    return 0;
                }
            }
        }
        if (target_policy_summary.preserves_direct_fallthrough_honesty &&
            fixup->kind == MACHINE_BYTES_FIXUP_CONTROL_SECONDARY &&
            fixup->patch_byte_count == 0u) {
            machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-130: preview secondary fixup must have real patch span");
            return 0;
        }
    }
    return 1;
}

int machine_object_report_get_summary(const MachineObjectReport *report,
    size_t *out_total_byte_count,
    size_t *out_section_count,
    size_t *out_symbol_count,
    size_t *out_fixup_count) {
    if (!report) {
        return 0;
    }
    return machine_object_file_get_summary(
        &report->file, out_total_byte_count, out_section_count, out_symbol_count, out_fixup_count);
}

int machine_object_report_get_file(const MachineObjectReport *report,
    const MachineObjectFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_object_report_get_target_policy_summary_artifact(const MachineObjectReport *report,
    const MachineObjectTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_object_report_get_fixup_family_summary_artifact(const MachineObjectReport *report,
    const MachineObjectFixupFamilySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->fixup_family_summary;
    return 1;
}

int machine_object_report_get_section_summary(const MachineObjectReport *report,
    size_t section_index,
    const MachineObjectSectionSummary **out_summary) {
    if (!report || !out_summary || section_index >= report->section_summary_count || !report->section_summaries) {
        return 0;
    }
    *out_summary = &report->section_summaries[section_index];
    return 1;
}

int machine_object_report_find_section_summary_by_name(const MachineObjectReport *report,
    const char *section_name,
    size_t *out_section_index,
    const MachineObjectSectionSummary **out_summary) {
    size_t section_index;

    if (!report || !section_name || !report->section_summaries) {
        return 0;
    }
    for (section_index = 0u; section_index < report->section_summary_count; ++section_index) {
        if (report->section_summaries[section_index].name &&
            strcmp(report->section_summaries[section_index].name, section_name) == 0) {
            if (out_section_index) {
                *out_section_index = section_index;
            }
            if (out_summary) {
                *out_summary = &report->section_summaries[section_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_object_report_get_symbol_summary(const MachineObjectReport *report,
    size_t symbol_index,
    const MachineObjectSymbolSummary **out_summary) {
    if (!report || !out_summary || symbol_index >= report->symbol_summary_count || !report->symbol_summaries) {
        return 0;
    }
    *out_summary = &report->symbol_summaries[symbol_index];
    return 1;
}

int machine_object_report_find_symbol_summary_by_name(const MachineObjectReport *report,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineObjectSymbolSummary **out_summary) {
    size_t symbol_index;

    if (!report || !symbol_name || !report->symbol_summaries) {
        return 0;
    }
    for (symbol_index = 0u; symbol_index < report->symbol_summary_count; ++symbol_index) {
        if (report->symbol_summaries[symbol_index].name &&
            strcmp(report->symbol_summaries[symbol_index].name, symbol_name) == 0) {
            if (out_symbol_index) {
                *out_symbol_index = symbol_index;
            }
            if (out_summary) {
                *out_summary = &report->symbol_summaries[symbol_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_object_report_get_fixup_summary(const MachineObjectReport *report,
    size_t fixup_index,
    const MachineObjectFixupSummary **out_summary) {
    if (!report || !out_summary || fixup_index >= report->fixup_summary_count || !report->fixup_summaries) {
        return 0;
    }
    *out_summary = &report->fixup_summaries[fixup_index];
    return 1;
}

int machine_object_dump_file(const MachineObjectFile *object_file,
    char **out_text,
    MachineObjectError *error) {
    MachineObjectStringBuilder builder;
    MachineObjectTargetPolicySummary target_policy_summary;
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
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    if (!machine_object_file_get_target_policy_summary(object_file, &target_policy_summary)) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-127: invalid object target policy contract");
        return 0;
    }
    if (!machine_object_append_format(
            &builder,
            "machine_object profile=%s total_bytes=%zu sections=%zu symbols=%zu fixups=%zu\npolicy: offsets=%s fallthrough=%s\nsections:\n",
            machine_object_target_profile_name(target_policy_summary.target_profile),
            object_file->total_byte_count,
            object_file->section_count,
            object_file->symbol_count,
            object_file->fixup_count,
            target_policy_summary.preserves_preview_target_byte_offsets ? "preview-target-byte-offsets" : "generic-or-unspecified",
            target_policy_summary.preserves_direct_fallthrough_honesty ? "direct-preview-aware" : "not-guaranteed")) {
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
                machine_object_symbol_kind_name(symbol->kind),
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
                machine_object_fixup_kind_name(fixup->kind),
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

int machine_object_dump_report(const MachineObjectReport *report,
    char **out_text,
    MachineObjectError *error) {
    MachineObjectStringBuilder builder;
    size_t section_index;
    size_t symbol_index;
    size_t fixup_index;
    char *file_dump = NULL;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_object_set_error(error, 0, 0, "MACHINE-OBJECT-153: invalid object report dump contract");
        return 0;
    }
    if (!machine_object_dump_file(&report->file, &file_dump, error)) {
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_object_append_format(
            &builder,
            "machine_object-report total_bytes=%zu sections=%zu symbols=%zu fixups=%zu\n"
            "target_policy profile=%s preview_offsets=%d fallthrough=%d\n"
            "fixup_families: call=%zu primary=%zu secondary=%zu data_addr=%zu data_load=%zu data_store=%zu\n"
            "section_summaries:\n",
            report->file.total_byte_count,
            report->file.section_count,
            report->file.symbol_count,
            report->file.fixup_count,
            machine_object_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.preserves_preview_target_byte_offsets,
            report->target_policy_summary.preserves_direct_fallthrough_honesty,
            report->fixup_family_summary.call_fixup_count,
            report->fixup_family_summary.primary_control_fixup_count,
            report->fixup_family_summary.secondary_control_fixup_count,
            report->fixup_family_summary.data_address_fixup_count,
            report->fixup_family_summary.data_load_fixup_count,
            report->fixup_family_summary.data_store_fixup_count)) {
        goto cleanup;
    }
    for (section_index = 0u; section_index < report->section_summary_count; ++section_index) {
        const MachineObjectSectionSummary *summary = &report->section_summaries[section_index];

        if (!machine_object_append_format(
                &builder,
                "  sec.%zu %s span=%zu..%zu bytes=%zu symbols=%zu fixups=%zu\n",
                section_index,
                summary->name ? summary->name : "<null>",
                summary->start_byte_offset,
                summary->end_byte_offset,
                summary->byte_count,
                summary->symbol_count,
                summary->fixup_count)) {
            goto cleanup;
        }
    }
    if (!machine_object_append_format(&builder, "symbol_summaries:\n")) {
        goto cleanup;
    }
    for (symbol_index = 0u; symbol_index < report->symbol_summary_count; ++symbol_index) {
        const MachineObjectSymbolSummary *summary = &report->symbol_summaries[symbol_index];

        if (!machine_object_append_format(
                &builder,
                "  sym.%zu %s %s defined=%d sec=%d offset=%zu bytes=%zu incoming_fixups=%zu\n",
                symbol_index,
                machine_object_symbol_kind_name(summary->kind),
                summary->name ? summary->name : "<null>",
                summary->is_defined,
                summary->has_section_index,
                summary->byte_offset_in_section,
                summary->byte_count,
                summary->incoming_fixup_count)) {
            goto cleanup;
        }
    }
    if (!machine_object_append_format(&builder, "fixup_summaries:\n")) {
        goto cleanup;
    }
    for (fixup_index = 0u; fixup_index < report->fixup_summary_count; ++fixup_index) {
        const MachineObjectFixupSummary *summary = &report->fixup_summaries[fixup_index];

        if (!machine_object_append_format(
                &builder,
                "  fixup.%zu %s sec=%zu patch@%zu bytes=%zu target=%s\n",
                fixup_index,
                machine_object_fixup_kind_name(summary->kind),
                summary->section_index,
                summary->patch_byte_offset,
                summary->patch_byte_count,
                summary->target_name ? summary->target_name : "<null>")) {
            goto cleanup;
        }
    }
    if (!machine_object_append_format(&builder, "\n%s", file_dump ? file_dump : "")) {
        goto cleanup;
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(file_dump);
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

int machine_object_build_report_dump_from_machine_bytes_report(const MachineBytesReport *report,
    char **out_text,
    MachineObjectError *error) {
    MachineObjectReport object_report;
    int ok;

    machine_object_report_init(&object_report);
    ok = machine_object_build_report_from_machine_bytes_report(report, &object_report, error) &&
        machine_object_dump_report(&object_report, out_text, error);
    machine_object_report_free(&object_report);
    return ok;
}

int machine_object_build_report_dump_from_machine_bytes_program(const MachineBytesProgram *program,
    char **out_text,
    MachineObjectError *error) {
    MachineObjectReport object_report;
    int ok;

    machine_object_report_init(&object_report);
    ok = machine_object_build_report_from_machine_bytes_program(program, &object_report, error) &&
        machine_object_dump_report(&object_report, out_text, error);
    machine_object_report_free(&object_report);
    return ok;
}

int machine_object_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineObjectError *error) {
    MachineObjectReport object_report;
    int ok;

    machine_object_report_init(&object_report);
    ok = machine_object_build_report_from_machine_ir_report(report, &object_report, error) &&
        machine_object_dump_report(&object_report, out_text, error);
    machine_object_report_free(&object_report);
    return ok;
}
