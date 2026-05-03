#include "machine/reloc.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineRelocStringBuilder;

static void machine_reloc_set_error(MachineRelocError *error, int line, int column, const char *fmt, ...);
static char *machine_reloc_strdup(const char *text);
static int machine_reloc_append_format(MachineRelocStringBuilder *builder, const char *fmt, ...);
static int machine_reloc_clone_object_file(const MachineObjectFile *source,
    MachineObjectFile *out_object_file,
    MachineRelocError *error);
static int machine_reloc_clone_file(const MachineRelocFile *source,
    MachineRelocFile *out_reloc_file,
    MachineRelocError *error);
static ptrdiff_t machine_reloc_preview_addend_for_fixup(const MachineObjectFile *object_file,
    const MachineObjectFixup *fixup);

static void machine_reloc_set_error(MachineRelocError *error, int line, int column, const char *fmt, ...) {
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

static char *machine_reloc_strdup(const char *text) {
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

static int machine_reloc_append_format(MachineRelocStringBuilder *builder, const char *fmt, ...) {
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
        size_t next_capacity = builder->capacity == 0 ? 128u : builder->capacity;

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

    vsnprintf(builder->data + builder->length, builder->capacity - builder->length, fmt, args);
    builder->length += (size_t)needed;
    va_end(args);
    return 1;
}

static ptrdiff_t machine_reloc_preview_addend_for_fixup(const MachineObjectFile *object_file,
    const MachineObjectFixup *fixup) {
    if (!object_file || !fixup ||
        object_file->target_profile != MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW ||
        !fixup->has_target_byte_offset) {
        return 0;
    }
    if (fixup->kind == MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET) {
        return 0;
    }
    if (fixup->kind == MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET ||
        fixup->kind == MACHINE_BYTES_FIXUP_DATA_STORE_TARGET) {
        return (ptrdiff_t)fixup->target_byte_offset;
    }
    return (ptrdiff_t)fixup->target_byte_offset - (ptrdiff_t)fixup->patch_byte_offset;
}

static const char *machine_reloc_kind_name(MachineRelocKind kind) {
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

static int machine_reloc_clone_object_file(const MachineObjectFile *source,
    MachineObjectFile *out_object_file,
    MachineRelocError *error) {
    size_t section_index;
    size_t symbol_index;
    size_t fixup_index;

    if (!source || !out_object_file) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-100: invalid object clone contract");
        return 0;
    }

    machine_object_file_free(out_object_file);

    if (!machine_object_verify_file(source, NULL)) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-101: source object file failed verification");
        return 0;
    }
    out_object_file->target_profile = source->target_profile;

    if (source->section_count > 0) {
        out_object_file->sections = (MachineObjectSection *)calloc(source->section_count, sizeof(MachineObjectSection));
        if (!out_object_file->sections) {
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-102: out of memory cloning object sections");
            return 0;
        }
        out_object_file->section_count = source->section_count;
        out_object_file->section_capacity = source->section_count;
        for (section_index = 0; section_index < source->section_count; ++section_index) {
            const MachineObjectSection *src = &source->sections[section_index];
            MachineObjectSection *dest = &out_object_file->sections[section_index];

            *dest = *src;
            dest->name = machine_reloc_strdup(src->name);
            dest->bytes = NULL;
            if (src->name && !dest->name) {
                machine_object_file_free(out_object_file);
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-103: out of memory cloning section name");
                return 0;
            }
            if (src->byte_count > 0) {
                dest->bytes = (unsigned char *)malloc(src->byte_count);
                if (!dest->bytes) {
                    machine_object_file_free(out_object_file);
                    machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-104: out of memory cloning section bytes");
                    return 0;
                }
                memcpy(dest->bytes, src->bytes, src->byte_count);
            }
        }
    }

    if (source->symbol_count > 0) {
        out_object_file->symbols = (MachineObjectSymbol *)calloc(source->symbol_count, sizeof(MachineObjectSymbol));
        if (!out_object_file->symbols) {
            machine_object_file_free(out_object_file);
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-105: out of memory cloning object symbols");
            return 0;
        }
        out_object_file->symbol_count = source->symbol_count;
        out_object_file->symbol_capacity = source->symbol_count;
        for (symbol_index = 0; symbol_index < source->symbol_count; ++symbol_index) {
            const MachineObjectSymbol *src = &source->symbols[symbol_index];
            MachineObjectSymbol *dest = &out_object_file->symbols[symbol_index];

            *dest = *src;
            dest->name = machine_reloc_strdup(src->name);
            if (src->name && !dest->name) {
                machine_object_file_free(out_object_file);
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-106: out of memory cloning symbol name");
                return 0;
            }
        }
    }

    if (source->fixup_count > 0) {
        out_object_file->fixups = (MachineObjectFixup *)calloc(source->fixup_count, sizeof(MachineObjectFixup));
        if (!out_object_file->fixups) {
            machine_object_file_free(out_object_file);
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-107: out of memory cloning object fixups");
            return 0;
        }
        out_object_file->fixup_count = source->fixup_count;
        out_object_file->fixup_capacity = source->fixup_count;
        for (fixup_index = 0; fixup_index < source->fixup_count; ++fixup_index) {
            const MachineObjectFixup *src = &source->fixups[fixup_index];
            MachineObjectFixup *dest = &out_object_file->fixups[fixup_index];

            *dest = *src;
            dest->source_label_name = machine_reloc_strdup(src->source_label_name);
            dest->target_name = machine_reloc_strdup(src->target_name);
            if ((src->source_label_name && !dest->source_label_name) ||
                (src->target_name && !dest->target_name)) {
                machine_object_file_free(out_object_file);
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-108: out of memory cloning fixup text");
                return 0;
            }
        }
    }

    out_object_file->total_byte_count = source->total_byte_count;
    return 1;
}

static int machine_reloc_clone_file(const MachineRelocFile *source,
    MachineRelocFile *out_reloc_file,
    MachineRelocError *error) {
    size_t section_index;
    size_t relocation_index;

    if (!source || !out_reloc_file) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-129: invalid reloc clone contract");
        return 0;
    }

    machine_reloc_file_free(out_reloc_file);
    if (!machine_reloc_verify_file(source, NULL)) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-130: source reloc file failed verification");
        return 0;
    }
    if (!machine_reloc_clone_object_file(&source->object_file, &out_reloc_file->object_file, error)) {
        return 0;
    }

    if (source->section_count > 0u) {
        out_reloc_file->sections = (MachineRelocSection *)calloc(source->section_count, sizeof(MachineRelocSection));
        if (!out_reloc_file->sections) {
            machine_reloc_file_free(out_reloc_file);
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-131: out of memory cloning reloc sections");
            return 0;
        }
        out_reloc_file->section_count = source->section_count;
        out_reloc_file->section_capacity = source->section_count;
        for (section_index = 0u; section_index < source->section_count; ++section_index) {
            out_reloc_file->sections[section_index] = source->sections[section_index];
            out_reloc_file->sections[section_index].name =
                machine_reloc_strdup(source->sections[section_index].name);
            if (source->sections[section_index].name && !out_reloc_file->sections[section_index].name) {
                machine_reloc_file_free(out_reloc_file);
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-132: out of memory cloning reloc section name");
                return 0;
            }
        }
    }

    if (source->relocation_count > 0u) {
        out_reloc_file->relocations = (MachineRelocation *)calloc(source->relocation_count, sizeof(MachineRelocation));
        if (!out_reloc_file->relocations) {
            machine_reloc_file_free(out_reloc_file);
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-133: out of memory cloning relocations");
            return 0;
        }
        out_reloc_file->relocation_count = source->relocation_count;
        out_reloc_file->relocation_capacity = source->relocation_count;
        for (relocation_index = 0u; relocation_index < source->relocation_count; ++relocation_index) {
            out_reloc_file->relocations[relocation_index] = source->relocations[relocation_index];
            out_reloc_file->relocations[relocation_index].source_label_name =
                machine_reloc_strdup(source->relocations[relocation_index].source_label_name);
            out_reloc_file->relocations[relocation_index].target_name =
                machine_reloc_strdup(source->relocations[relocation_index].target_name);
            if ((source->relocations[relocation_index].source_label_name &&
                    !out_reloc_file->relocations[relocation_index].source_label_name) ||
                (source->relocations[relocation_index].target_name &&
                    !out_reloc_file->relocations[relocation_index].target_name)) {
                machine_reloc_file_free(out_reloc_file);
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-134: out of memory cloning relocation text");
                return 0;
            }
        }
    }

    return 1;
}

void machine_reloc_file_init(MachineRelocFile *reloc_file) {
    if (!reloc_file) {
        return;
    }
    machine_object_file_init(&reloc_file->object_file);
    reloc_file->sections = NULL;
    reloc_file->section_count = 0;
    reloc_file->section_capacity = 0;
    reloc_file->relocations = NULL;
    reloc_file->relocation_count = 0;
    reloc_file->relocation_capacity = 0;
}

void machine_reloc_file_free(MachineRelocFile *reloc_file) {
    size_t section_index;
    size_t relocation_index;

    if (!reloc_file) {
        return;
    }
    for (section_index = 0; section_index < reloc_file->section_count; ++section_index) {
        free(reloc_file->sections[section_index].name);
    }
    for (relocation_index = 0; relocation_index < reloc_file->relocation_count; ++relocation_index) {
        free(reloc_file->relocations[relocation_index].source_label_name);
        free(reloc_file->relocations[relocation_index].target_name);
    }
    free(reloc_file->sections);
    free(reloc_file->relocations);
    machine_object_file_free(&reloc_file->object_file);
    machine_reloc_file_init(reloc_file);
}

void machine_reloc_report_init(MachineRelocReport *report) {
    if (!report) {
        return;
    }
    machine_reloc_file_init(&report->file);
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    memset(&report->relocation_family_summary, 0, sizeof(report->relocation_family_summary));
    report->section_summaries = NULL;
    report->section_summary_count = 0u;
    report->relocation_summaries = NULL;
    report->relocation_summary_count = 0u;
}

void machine_reloc_report_free(MachineRelocReport *report) {
    if (!report) {
        return;
    }
    machine_reloc_file_free(&report->file);
    free(report->section_summaries);
    free(report->relocation_summaries);
    machine_reloc_report_init(report);
}

int machine_reloc_file_get_summary(const MachineRelocFile *reloc_file,
    size_t *out_total_byte_count,
    size_t *out_object_section_count,
    size_t *out_symbol_count,
    size_t *out_relocation_section_count,
    size_t *out_relocation_count) {
    if (!reloc_file) {
        return 0;
    }
    if (out_total_byte_count) {
        *out_total_byte_count = reloc_file->object_file.total_byte_count;
    }
    if (out_object_section_count) {
        *out_object_section_count = reloc_file->object_file.section_count;
    }
    if (out_symbol_count) {
        *out_symbol_count = reloc_file->object_file.symbol_count;
    }
    if (out_relocation_section_count) {
        *out_relocation_section_count = reloc_file->section_count;
    }
    if (out_relocation_count) {
        *out_relocation_count = reloc_file->relocation_count;
    }
    return 1;
}

int machine_reloc_get_target_policy_summary(MachineBytesTargetProfile profile,
    MachineRelocTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->target_profile = profile;
    if (!machine_bytes_get_target_policy_summary(profile, &out_summary->bytes_policy)) {
        return 0;
    }
    out_summary->preserves_preview_pc_relative_addends =
        profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW;
    out_summary->preserves_direct_fallthrough_honesty =
        profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW;
    return 1;
}

int machine_reloc_file_get_target_policy_summary(const MachineRelocFile *reloc_file,
    MachineRelocTargetPolicySummary *out_summary) {
    if (!reloc_file || !out_summary) {
        return 0;
    }
    return machine_reloc_get_target_policy_summary(reloc_file->object_file.target_profile, out_summary);
}

int machine_reloc_file_get_relocation_family_summary(const MachineRelocFile *reloc_file,
    MachineRelocRelocationFamilySummary *out_summary) {
    size_t relocation_index;

    if (!reloc_file || !out_summary) {
        return 0;
    }
    memset(out_summary, 0, sizeof(*out_summary));
    for (relocation_index = 0u; relocation_index < reloc_file->relocation_count; ++relocation_index) {
        switch (reloc_file->relocations[relocation_index].kind) {
            case MACHINE_BYTES_FIXUP_CALL_TARGET:
                out_summary->call_relocation_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_CONTROL_PRIMARY:
                out_summary->primary_control_relocation_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_CONTROL_SECONDARY:
                out_summary->secondary_control_relocation_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET:
                out_summary->data_address_relocation_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET:
                out_summary->data_load_relocation_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_DATA_STORE_TARGET:
                out_summary->data_store_relocation_count += 1u;
                break;
            default:
                break;
        }
    }
    return 1;
}

int machine_reloc_file_get_object_file(const MachineRelocFile *reloc_file,
    const MachineObjectFile **out_object_file) {
    if (!reloc_file || !out_object_file) {
        return 0;
    }
    *out_object_file = &reloc_file->object_file;
    return 1;
}

int machine_reloc_file_get_section(const MachineRelocFile *reloc_file,
    size_t section_index,
    const MachineRelocSection **out_section) {
    if (!reloc_file || !out_section || section_index >= reloc_file->section_count || !reloc_file->sections) {
        return 0;
    }
    *out_section = &reloc_file->sections[section_index];
    return 1;
}

int machine_reloc_file_find_section_by_name(const MachineRelocFile *reloc_file,
    const char *section_name,
    size_t *out_section_index,
    const MachineRelocSection **out_section) {
    size_t section_index;

    if (!reloc_file || !section_name || !reloc_file->sections) {
        return 0;
    }
    for (section_index = 0; section_index < reloc_file->section_count; ++section_index) {
        if (reloc_file->sections[section_index].name &&
            strcmp(reloc_file->sections[section_index].name, section_name) == 0) {
            if (out_section_index) {
                *out_section_index = section_index;
            }
            if (out_section) {
                *out_section = &reloc_file->sections[section_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_reloc_file_get_section_relocations(const MachineRelocFile *reloc_file,
    size_t section_index,
    size_t *out_count,
    const MachineRelocation **out_relocations) {
    const MachineRelocSection *section;

    if (out_count) {
        *out_count = 0;
    }
    if (out_relocations) {
        *out_relocations = NULL;
    }
    if (!reloc_file || !machine_reloc_file_get_section(reloc_file, section_index, &section) || !section ||
        !reloc_file->relocations) {
        return 0;
    }
    if (out_count) {
        *out_count = section->relocation_count;
    }
    if (out_relocations) {
        *out_relocations = reloc_file->relocations + section->relocation_start_index;
    }
    return 1;
}

int machine_reloc_file_get_relocation(const MachineRelocFile *reloc_file,
    size_t relocation_index,
    const MachineRelocation **out_relocation) {
    if (!reloc_file || !out_relocation || relocation_index >= reloc_file->relocation_count || !reloc_file->relocations) {
        return 0;
    }
    *out_relocation = &reloc_file->relocations[relocation_index];
    return 1;
}

int machine_reloc_section_get_summary(const MachineRelocSection *section,
    MachineRelocSectionSummary *out_summary) {
    if (!section || !out_summary) {
        return 0;
    }
    out_summary->object_section_index = section->object_section_index;
    out_summary->name = section->name;
    out_summary->start_byte_offset = section->start_byte_offset;
    out_summary->end_byte_offset = section->end_byte_offset;
    out_summary->byte_count = section->byte_count;
    out_summary->relocation_count = section->relocation_count;
    return 1;
}

int machine_relocation_get_summary(const MachineRelocation *relocation,
    MachineRelocationSummary *out_summary) {
    if (!relocation || !out_summary) {
        return 0;
    }
    out_summary->kind = relocation->kind;
    out_summary->target_kind = relocation->target_kind;
    out_summary->object_section_index = relocation->object_section_index;
    out_summary->source_label_name = relocation->source_label_name;
    out_summary->owner_byte_offset = relocation->owner_byte_offset;
    out_summary->owner_byte_count = relocation->owner_byte_count;
    out_summary->patch_byte_offset = relocation->patch_byte_offset;
    out_summary->patch_byte_count = relocation->patch_byte_count;
    out_summary->target_name = relocation->target_name;
    out_summary->has_target_symbol_index = relocation->has_target_symbol_index;
    out_summary->target_symbol_index = relocation->target_symbol_index;
    out_summary->has_target_section_index = relocation->has_target_section_index;
    out_summary->target_section_index = relocation->target_section_index;
    out_summary->has_target_byte_offset = relocation->has_target_byte_offset;
    out_summary->target_byte_offset = relocation->target_byte_offset;
    out_summary->addend = relocation->addend;
    return 1;
}

int machine_reloc_build_from_machine_object_file(const MachineObjectFile *object_file,
    MachineRelocFile *out_reloc_file,
    MachineRelocError *error) {
    size_t section_index;
    size_t relocation_index;

    if (!object_file || !out_reloc_file) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-109: invalid reloc build contract");
        return 0;
    }

    machine_reloc_file_free(out_reloc_file);

    if (!machine_reloc_clone_object_file(object_file, &out_reloc_file->object_file, error)) {
        return 0;
    }

    if (object_file->section_count > 0) {
        out_reloc_file->sections = (MachineRelocSection *)calloc(object_file->section_count, sizeof(MachineRelocSection));
        if (!out_reloc_file->sections) {
            machine_reloc_file_free(out_reloc_file);
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-110: out of memory building relocation sections");
            return 0;
        }
        out_reloc_file->section_count = object_file->section_count;
        out_reloc_file->section_capacity = object_file->section_count;
        for (section_index = 0; section_index < object_file->section_count; ++section_index) {
            const MachineObjectSection *src = &object_file->sections[section_index];
            MachineRelocSection *dest = &out_reloc_file->sections[section_index];

            dest->object_section_index = section_index;
            dest->name = machine_reloc_strdup(src->name);
            dest->start_byte_offset = src->start_byte_offset;
            dest->end_byte_offset = src->end_byte_offset;
            dest->byte_count = src->byte_count;
            dest->relocation_start_index = src->fixup_start_index;
            dest->relocation_count = src->fixup_count;
            if (src->name && !dest->name) {
                machine_reloc_file_free(out_reloc_file);
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-111: out of memory copying relocation section name");
                return 0;
            }
        }
    }

    if (object_file->fixup_count > 0) {
        out_reloc_file->relocations = (MachineRelocation *)calloc(object_file->fixup_count, sizeof(MachineRelocation));
        if (!out_reloc_file->relocations) {
            machine_reloc_file_free(out_reloc_file);
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-112: out of memory building relocations");
            return 0;
        }
        out_reloc_file->relocation_count = object_file->fixup_count;
        out_reloc_file->relocation_capacity = object_file->fixup_count;
        for (relocation_index = 0; relocation_index < object_file->fixup_count; ++relocation_index) {
            const MachineObjectFixup *src = &object_file->fixups[relocation_index];
            MachineRelocation *dest = &out_reloc_file->relocations[relocation_index];

            dest->kind = src->kind;
            dest->target_kind = src->target_kind;
            dest->object_section_index = src->section_index;
            dest->source_label_name = machine_reloc_strdup(src->source_label_name);
            dest->owner_byte_offset = src->owner_byte_offset;
            dest->owner_byte_count = src->owner_byte_count;
            dest->patch_byte_offset = src->patch_byte_offset;
            dest->patch_byte_count = src->patch_byte_count;
            dest->target_name = machine_reloc_strdup(src->target_name);
            dest->has_target_symbol_index = src->has_target_symbol_index;
            dest->target_symbol_index = src->target_symbol_index;
            dest->addend = machine_reloc_preview_addend_for_fixup(object_file, src);
            if ((src->source_label_name && !dest->source_label_name) || (src->target_name && !dest->target_name)) {
                machine_reloc_file_free(out_reloc_file);
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-113: out of memory copying relocation text");
                return 0;
            }

            if (src->has_target_symbol_index && src->target_symbol_index < object_file->symbol_count) {
                const MachineObjectSymbol *target_symbol = &object_file->symbols[src->target_symbol_index];

                if (target_symbol->is_defined && target_symbol->has_section_index) {
                    dest->has_target_section_index = 1;
                    dest->target_section_index = target_symbol->section_index;
                    dest->has_target_byte_offset = 1;
                    dest->target_byte_offset = target_symbol->byte_offset_in_section;
                } else if (src->has_target_byte_offset) {
                    dest->has_target_byte_offset = 1;
                    dest->target_byte_offset = src->target_byte_offset;
                }
            } else if (src->has_target_byte_offset) {
                dest->has_target_byte_offset = 1;
                dest->target_byte_offset = src->target_byte_offset;
            }
        }
    }

    return machine_reloc_verify_file(out_reloc_file, error);
}

int machine_reloc_build_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineBytesTargetProfile profile,
    MachineRelocFile *out_reloc_file,
    MachineRelocError *error) {
    MachineObjectFile object_file;
    MachineObjectError object_error;
    int ok;

    if (!report || !out_reloc_file) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-114: invalid machine-ir reloc build contract");
        return 0;
    }

    machine_object_file_init(&object_file);
    memset(&object_error, 0, sizeof(object_error));
    ok = machine_object_build_from_machine_ir_report_with_profile(report, profile, &object_file, &object_error);
    if (!ok) {
        machine_reloc_set_error(error, object_error.line, object_error.column, "%s", object_error.message);
        machine_object_file_free(&object_file);
        return 0;
    }

    ok = machine_reloc_build_from_machine_object_file(&object_file, out_reloc_file, error);
    machine_object_file_free(&object_file);
    return ok;
}

int machine_reloc_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineRelocFile *out_reloc_file,
    MachineRelocError *error) {
    return machine_reloc_build_from_machine_ir_report_with_profile(
        report,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_reloc_file,
        error);
}

int machine_reloc_build_report_from_file(const MachineRelocFile *source,
    MachineRelocReport *out_report,
    MachineRelocError *error) {
    size_t section_index;
    size_t relocation_index;

    if (!source || !out_report) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-135: invalid reloc report build contract");
        return 0;
    }

    machine_reloc_report_free(out_report);
    if (!machine_reloc_clone_file(source, &out_report->file, error)) {
        return 0;
    }
    if (!machine_reloc_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_reloc_file_get_relocation_family_summary(&out_report->file, &out_report->relocation_family_summary)) {
        machine_reloc_report_free(out_report);
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-136: failed to compute reloc report summaries");
        return 0;
    }
    if (out_report->file.section_count > 0u) {
        out_report->section_summaries =
            (MachineRelocSectionSummary *)calloc(out_report->file.section_count, sizeof(MachineRelocSectionSummary));
        if (!out_report->section_summaries) {
            machine_reloc_report_free(out_report);
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-137: out of memory building reloc report sections");
            return 0;
        }
        out_report->section_summary_count = out_report->file.section_count;
        for (section_index = 0u; section_index < out_report->file.section_count; ++section_index) {
            if (!machine_reloc_section_get_summary(&out_report->file.sections[section_index],
                    &out_report->section_summaries[section_index])) {
                machine_reloc_report_free(out_report);
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-138: failed to summarize reloc section");
                return 0;
            }
        }
    }
    if (out_report->file.relocation_count > 0u) {
        out_report->relocation_summaries = (MachineRelocationSummary *)calloc(
            out_report->file.relocation_count, sizeof(MachineRelocationSummary));
        if (!out_report->relocation_summaries) {
            machine_reloc_report_free(out_report);
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-139: out of memory building reloc report rows");
            return 0;
        }
        out_report->relocation_summary_count = out_report->file.relocation_count;
        for (relocation_index = 0u; relocation_index < out_report->file.relocation_count; ++relocation_index) {
            if (!machine_relocation_get_summary(&out_report->file.relocations[relocation_index],
                    &out_report->relocation_summaries[relocation_index])) {
                machine_reloc_report_free(out_report);
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-140: failed to summarize relocation");
                return 0;
            }
        }
    }
    return 1;
}

int machine_reloc_build_report_from_machine_object_file(const MachineObjectFile *object_file,
    MachineRelocReport *out_report,
    MachineRelocError *error) {
    MachineRelocFile reloc_file;
    int ok;

    machine_reloc_file_init(&reloc_file);
    ok = machine_reloc_build_from_machine_object_file(object_file, &reloc_file, error) &&
        machine_reloc_build_report_from_file(&reloc_file, out_report, error);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

int machine_reloc_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineRelocReport *out_report,
    MachineRelocError *error) {
    MachineRelocFile reloc_file;
    int ok;

    machine_reloc_file_init(&reloc_file);
    ok = machine_reloc_build_from_machine_ir_report(report, &reloc_file, error) &&
        machine_reloc_build_report_from_file(&reloc_file, out_report, error);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

int machine_reloc_verify_file(const MachineRelocFile *reloc_file, MachineRelocError *error) {
    size_t section_index;
    size_t relocation_index;
    MachineRelocTargetPolicySummary target_policy_summary;

    if (!reloc_file) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-115: invalid reloc verifier contract");
        return 0;
    }
    if (!machine_object_verify_file(&reloc_file->object_file, NULL)) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-116: embedded object file failed verification");
        return 0;
    }
    if ((reloc_file->section_count > 0 && !reloc_file->sections) ||
        reloc_file->section_count > reloc_file->section_capacity ||
        (reloc_file->relocation_count > 0 && !reloc_file->relocations) ||
        reloc_file->relocation_count > reloc_file->relocation_capacity) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-117: invalid relocation table metadata");
        return 0;
    }
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    if (!machine_reloc_file_get_target_policy_summary(reloc_file, &target_policy_summary)) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-126: invalid reloc target policy contract");
        return 0;
    }

    for (section_index = 0; section_index < reloc_file->section_count; ++section_index) {
        const MachineRelocSection *section = &reloc_file->sections[section_index];
        const MachineObjectSection *object_section = NULL;

        if (!section->name || section->name[0] == '\0' ||
            section->object_section_index >= reloc_file->object_file.section_count ||
            !machine_object_file_get_section(&reloc_file->object_file, section->object_section_index, &object_section) ||
            !object_section) {
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-118: invalid relocation section");
            return 0;
        }
        if (section->relocation_start_index + section->relocation_count > reloc_file->relocation_count ||
            section->start_byte_offset != object_section->start_byte_offset ||
            section->end_byte_offset != object_section->end_byte_offset ||
            section->byte_count != object_section->byte_count) {
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-119: relocation section metadata mismatch");
            return 0;
        }
    }

    for (relocation_index = 0; relocation_index < reloc_file->relocation_count; ++relocation_index) {
        const MachineRelocation *relocation = &reloc_file->relocations[relocation_index];
        const MachineObjectSection *section = NULL;

        if (!relocation->target_name || relocation->target_name[0] == '\0' ||
            relocation->object_section_index >= reloc_file->object_file.section_count ||
            !machine_object_file_get_section(&reloc_file->object_file, relocation->object_section_index, &section) ||
            !section) {
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-120: invalid relocation target/section");
            return 0;
        }
        if (relocation->owner_byte_offset + relocation->owner_byte_count > section->byte_count ||
            relocation->patch_byte_offset + relocation->patch_byte_count > section->byte_count) {
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-121: relocation exceeds section span");
            return 0;
        }
        if (relocation->has_target_symbol_index &&
            relocation->target_symbol_index >= reloc_file->object_file.symbol_count) {
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-122: relocation target symbol out of range");
            return 0;
        }
        if (relocation->has_target_section_index) {
            const MachineObjectSection *target_section = NULL;

            if (relocation->target_section_index >= reloc_file->object_file.section_count ||
                !machine_object_file_get_section(&reloc_file->object_file, relocation->target_section_index, &target_section) ||
                !target_section) {
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-123: relocation target section out of range");
                return 0;
            }
            if (relocation->has_target_byte_offset && relocation->target_byte_offset > target_section->byte_count) {
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-124: relocation target offset exceeds section");
                return 0;
            }
        }
        if (target_policy_summary.preserves_preview_pc_relative_addends &&
            relocation->has_target_byte_offset &&
            relocation->kind == MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET &&
            relocation->addend != 0) {
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-127: preview relocation addend mismatch");
            return 0;
        }
        if (target_policy_summary.preserves_preview_pc_relative_addends &&
            relocation->has_target_byte_offset &&
            (relocation->kind == MACHINE_BYTES_FIXUP_CALL_TARGET ||
                relocation->kind == MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ||
                relocation->kind == MACHINE_BYTES_FIXUP_CONTROL_SECONDARY)) {
            ptrdiff_t expected_addend =
                (ptrdiff_t)relocation->target_byte_offset - (ptrdiff_t)relocation->patch_byte_offset;

            if (relocation->addend != expected_addend) {
                machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-127: preview relocation addend mismatch");
                return 0;
            }
        }
        if (target_policy_summary.preserves_preview_pc_relative_addends &&
            relocation->has_target_byte_offset &&
            (relocation->kind == MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET ||
                relocation->kind == MACHINE_BYTES_FIXUP_DATA_STORE_TARGET) &&
            relocation->addend != (ptrdiff_t)relocation->target_byte_offset) {
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-127: preview relocation addend mismatch");
            return 0;
        }
        if (target_policy_summary.preserves_direct_fallthrough_honesty &&
            relocation->kind == MACHINE_BYTES_FIXUP_CONTROL_SECONDARY &&
            relocation->patch_byte_count == 0u) {
            machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-128: preview secondary relocation must have real patch span");
            return 0;
        }
    }
    return 1;
}

int machine_reloc_report_get_summary(const MachineRelocReport *report,
    size_t *out_total_byte_count,
    size_t *out_object_section_count,
    size_t *out_symbol_count,
    size_t *out_relocation_section_count,
    size_t *out_relocation_count) {
    if (!report) {
        return 0;
    }
    return machine_reloc_file_get_summary(
        &report->file,
        out_total_byte_count,
        out_object_section_count,
        out_symbol_count,
        out_relocation_section_count,
        out_relocation_count);
}

int machine_reloc_report_get_file(const MachineRelocReport *report,
    const MachineRelocFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_reloc_report_get_target_policy_summary_artifact(const MachineRelocReport *report,
    const MachineRelocTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_reloc_report_get_relocation_family_summary_artifact(const MachineRelocReport *report,
    const MachineRelocRelocationFamilySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->relocation_family_summary;
    return 1;
}

int machine_reloc_report_get_section_summary(const MachineRelocReport *report,
    size_t section_index,
    const MachineRelocSectionSummary **out_summary) {
    if (!report || !out_summary || section_index >= report->section_summary_count || !report->section_summaries) {
        return 0;
    }
    *out_summary = &report->section_summaries[section_index];
    return 1;
}

int machine_reloc_report_find_section_summary_by_name(const MachineRelocReport *report,
    const char *section_name,
    size_t *out_section_index,
    const MachineRelocSectionSummary **out_summary) {
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

int machine_reloc_report_get_relocation_summary(const MachineRelocReport *report,
    size_t relocation_index,
    const MachineRelocationSummary **out_summary) {
    if (!report || !out_summary || relocation_index >= report->relocation_summary_count ||
        !report->relocation_summaries) {
        return 0;
    }
    *out_summary = &report->relocation_summaries[relocation_index];
    return 1;
}

int machine_reloc_dump_file(const MachineRelocFile *reloc_file,
    char **out_text,
    MachineRelocError *error) {
    MachineRelocStringBuilder builder;
    MachineRelocTargetPolicySummary target_policy_summary;
    size_t section_index;
    size_t relocation_index;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!reloc_file || !out_text) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-125: invalid reloc dump contract");
        return 0;
    }
    if (!machine_reloc_verify_file(reloc_file, error)) {
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    if (!machine_reloc_file_get_target_policy_summary(reloc_file, &target_policy_summary)) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-126: invalid reloc target policy contract");
        return 0;
    }
    if (!machine_reloc_append_format(
            &builder,
            "machine_reloc profile=%s total_bytes=%zu object_sections=%zu symbols=%zu relocation_sections=%zu relocations=%zu\npolicy: addends=%s fallthrough=%s\nreloc-sections:\n",
            target_policy_summary.target_profile == MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW
                ? "riscv32-preview"
                : "generic",
            reloc_file->object_file.total_byte_count,
            reloc_file->object_file.section_count,
            reloc_file->object_file.symbol_count,
            reloc_file->section_count,
            reloc_file->relocation_count,
            target_policy_summary.preserves_preview_pc_relative_addends ? "pc-relative-preview" : "zero-or-generic",
            target_policy_summary.preserves_direct_fallthrough_honesty ? "direct-preview-aware" : "not-guaranteed")) {
        goto cleanup;
    }

    for (section_index = 0; section_index < reloc_file->section_count; ++section_index) {
        const MachineRelocSection *section = &reloc_file->sections[section_index];

        if (!machine_reloc_append_format(
                &builder,
                "  relsec.%zu %s span=%zu..%zu bytes=%zu relocations=%zu\n",
                section_index,
                section->name ? section->name : "<null>",
                section->start_byte_offset,
                section->end_byte_offset,
                section->byte_count,
                section->relocation_count)) {
            goto cleanup;
        }
    }

    if (!machine_reloc_append_format(&builder, "relocations:\n")) {
        goto cleanup;
    }
    for (relocation_index = 0; relocation_index < reloc_file->relocation_count; ++relocation_index) {
        const MachineRelocation *relocation = &reloc_file->relocations[relocation_index];

        if (!machine_reloc_append_format(
                &builder,
                "  reloc.%zu %s sec=%zu patch@%zu bytes=%zu target=%s sym=%s tgt_sec=%s tgt_off=%s addend=%td\n",
                relocation_index,
                machine_reloc_kind_name(relocation->kind),
                relocation->object_section_index,
                relocation->patch_byte_offset,
                relocation->patch_byte_count,
                relocation->target_name ? relocation->target_name : "<null>",
                relocation->has_target_symbol_index ? "yes" : "no",
                relocation->has_target_section_index ? "yes" : "no",
                relocation->has_target_byte_offset ? "yes" : "no",
                relocation->addend)) {
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

int machine_reloc_dump_report(const MachineRelocReport *report,
    char **out_text,
    MachineRelocError *error) {
    MachineRelocStringBuilder builder;
    size_t section_index;
    size_t relocation_index;
    char *file_dump = NULL;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_reloc_set_error(error, 0, 0, "MACHINE-RELOC-141: invalid reloc report dump contract");
        return 0;
    }
    if (!machine_reloc_dump_file(&report->file, &file_dump, error)) {
        return 0;
    }
    memset(&builder, 0, sizeof(builder));
    if (!machine_reloc_append_format(
            &builder,
            "machine_reloc-report total_bytes=%zu object_sections=%zu symbols=%zu relocation_sections=%zu relocations=%zu\n"
            "target_policy profile=%u preview_addends=%d fallthrough=%d\n"
            "relocation_families: call=%zu primary=%zu secondary=%zu data_addr=%zu data_load=%zu data_store=%zu\n"
            "section_summaries:\n",
            report->file.object_file.total_byte_count,
            report->file.object_file.section_count,
            report->file.object_file.symbol_count,
            report->file.section_count,
            report->file.relocation_count,
            (unsigned)report->target_policy_summary.target_profile,
            report->target_policy_summary.preserves_preview_pc_relative_addends,
            report->target_policy_summary.preserves_direct_fallthrough_honesty,
            report->relocation_family_summary.call_relocation_count,
            report->relocation_family_summary.primary_control_relocation_count,
            report->relocation_family_summary.secondary_control_relocation_count,
            report->relocation_family_summary.data_address_relocation_count,
            report->relocation_family_summary.data_load_relocation_count,
            report->relocation_family_summary.data_store_relocation_count)) {
        goto cleanup;
    }
    for (section_index = 0u; section_index < report->section_summary_count; ++section_index) {
        const MachineRelocSectionSummary *summary = &report->section_summaries[section_index];

        if (!machine_reloc_append_format(
                &builder,
                "  relsec.%zu %s span=%zu..%zu bytes=%zu relocations=%zu\n",
                section_index,
                summary->name ? summary->name : "<null>",
                summary->start_byte_offset,
                summary->end_byte_offset,
                summary->byte_count,
                summary->relocation_count)) {
            goto cleanup;
        }
    }
    if (!machine_reloc_append_format(&builder, "relocation_summaries:\n")) {
        goto cleanup;
    }
    for (relocation_index = 0u; relocation_index < report->relocation_summary_count; ++relocation_index) {
        const MachineRelocationSummary *summary = &report->relocation_summaries[relocation_index];

        if (!machine_reloc_append_format(
                &builder,
                "  reloc.%zu %s sec=%zu patch@%zu bytes=%zu target=%s addend=%td\n",
                relocation_index,
                machine_reloc_kind_name(summary->kind),
                summary->object_section_index,
                summary->patch_byte_offset,
                summary->patch_byte_count,
                summary->target_name ? summary->target_name : "<null>",
                summary->addend)) {
            goto cleanup;
        }
    }
    if (!machine_reloc_append_format(&builder, "\n%s", file_dump ? file_dump : "")) {
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

int machine_reloc_build_dump_from_machine_object_file(const MachineObjectFile *object_file,
    char **out_text,
    MachineRelocError *error) {
    MachineRelocFile reloc_file;
    int ok;

    machine_reloc_file_init(&reloc_file);
    ok = machine_reloc_build_from_machine_object_file(object_file, &reloc_file, error) &&
        machine_reloc_dump_file(&reloc_file, out_text, error);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

int machine_reloc_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineRelocError *error) {
    MachineRelocFile reloc_file;
    int ok;

    machine_reloc_file_init(&reloc_file);
    ok = machine_reloc_build_from_machine_ir_report(report, &reloc_file, error) &&
        machine_reloc_dump_file(&reloc_file, out_text, error);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

int machine_reloc_build_report_dump_from_machine_object_file(const MachineObjectFile *object_file,
    char **out_text,
    MachineRelocError *error) {
    MachineRelocReport report;
    int ok;

    machine_reloc_report_init(&report);
    ok = machine_reloc_build_report_from_machine_object_file(object_file, &report, error) &&
        machine_reloc_dump_report(&report, out_text, error);
    machine_reloc_report_free(&report);
    return ok;
}

int machine_reloc_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineRelocError *error) {
    MachineRelocReport reloc_report;
    int ok;

    machine_reloc_report_init(&reloc_report);
    ok = machine_reloc_build_report_from_machine_ir_report(report, &reloc_report, error) &&
        machine_reloc_dump_report(&reloc_report, out_text, error);
    machine_reloc_report_free(&reloc_report);
    return ok;
}
