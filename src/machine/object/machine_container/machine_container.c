#include "machine/container.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MACHINE_CONTAINER_MAGIC_0 ((unsigned char)'M')
#define MACHINE_CONTAINER_MAGIC_1 ((unsigned char)'C')
#define MACHINE_CONTAINER_MAGIC_2 ((unsigned char)'N')
#define MACHINE_CONTAINER_MAGIC_3 ((unsigned char)'1')
#define MACHINE_CONTAINER_VERSION 1u
#define MACHINE_CONTAINER_HEADER_WORD_COUNT 11u
#define MACHINE_CONTAINER_HEADER_SIZE (MACHINE_CONTAINER_HEADER_WORD_COUNT * sizeof(uint32_t))
#define MACHINE_CONTAINER_SECTION_ENTRY_WORD_COUNT 7u
#define MACHINE_CONTAINER_SECTION_ENTRY_SIZE (MACHINE_CONTAINER_SECTION_ENTRY_WORD_COUNT * sizeof(uint32_t))
#define MACHINE_CONTAINER_SYMBOL_ENTRY_WORD_COUNT 9u
#define MACHINE_CONTAINER_SYMBOL_ENTRY_SIZE (MACHINE_CONTAINER_SYMBOL_ENTRY_WORD_COUNT * sizeof(uint32_t))
#define MACHINE_CONTAINER_RELOCATION_ENTRY_WORD_COUNT 13u
#define MACHINE_CONTAINER_RELOCATION_ENTRY_SIZE (MACHINE_CONTAINER_RELOCATION_ENTRY_WORD_COUNT * sizeof(uint32_t))
#define MACHINE_CONTAINER_NONE_U32 0xFFFFFFFFu

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineContainerStringBuilder;

static void machine_container_set_error(MachineContainerError *error, int line, int column, const char *fmt, ...);
static char *machine_container_strdup(const char *text);
static int machine_container_append_format(MachineContainerStringBuilder *builder, const char *fmt, ...);
static void machine_container_write_u32_le(unsigned char *bytes, size_t offset, uint32_t value);
static int machine_container_clone_reloc_file(const MachineRelocFile *source,
    MachineRelocFile *out_reloc_file,
    MachineContainerError *error);
static size_t machine_container_measure_string(const char *text);
static uint32_t machine_container_append_string(unsigned char *bytes,
    size_t string_table_offset,
    size_t string_table_size,
    size_t *io_cursor,
    const char *text);

static void machine_container_set_error(MachineContainerError *error, int line, int column, const char *fmt, ...) {
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

static char *machine_container_strdup(const char *text) {
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

static int machine_container_append_format(MachineContainerStringBuilder *builder, const char *fmt, ...) {
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

static void machine_container_write_u32_le(unsigned char *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (unsigned char)(value & 0xFFu);
    bytes[offset + 1] = (unsigned char)((value >> 8) & 0xFFu);
    bytes[offset + 2] = (unsigned char)((value >> 16) & 0xFFu);
    bytes[offset + 3] = (unsigned char)((value >> 24) & 0xFFu);
}

static size_t machine_container_measure_string(const char *text) {
    return text ? strlen(text) + 1u : 0u;
}

static uint32_t machine_container_append_string(unsigned char *bytes,
    size_t string_table_offset,
    size_t string_table_size,
    size_t *io_cursor,
    const char *text) {
    size_t length;
    size_t start;

    if (!text) {
        return MACHINE_CONTAINER_NONE_U32;
    }
    length = strlen(text) + 1u;
    start = *io_cursor;
    if (start + length > string_table_size) {
        return MACHINE_CONTAINER_NONE_U32;
    }
    memcpy(bytes + string_table_offset + start, text, length);
    *io_cursor = start + length;
    return (uint32_t)(string_table_offset + start);
}

static int machine_container_clone_reloc_file(const MachineRelocFile *source,
    MachineRelocFile *out_reloc_file,
    MachineContainerError *error) {
    size_t section_index;
    size_t relocation_index;

    if (!source || !out_reloc_file) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-100: invalid reloc clone contract");
        return 0;
    }

    machine_reloc_file_free(out_reloc_file);

    if (!machine_object_verify_file(&source->object_file, NULL)) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-101: source relocation object failed verification");
        return 0;
    }
    if (!machine_reloc_verify_file(source, NULL)) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-102: source relocation file failed verification");
        return 0;
    }
    out_reloc_file->object_file.target_profile = source->object_file.target_profile;

    if (source->object_file.section_count > 0) {
        out_reloc_file->object_file.sections =
            (MachineObjectSection *)calloc(source->object_file.section_count, sizeof(MachineObjectSection));
        if (!out_reloc_file->object_file.sections) {
            machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-103: out of memory cloning object sections");
            return 0;
        }
        out_reloc_file->object_file.section_count = source->object_file.section_count;
        out_reloc_file->object_file.section_capacity = source->object_file.section_count;
        for (section_index = 0; section_index < source->object_file.section_count; ++section_index) {
            const MachineObjectSection *src = &source->object_file.sections[section_index];
            MachineObjectSection *dest = &out_reloc_file->object_file.sections[section_index];

            *dest = *src;
            dest->name = machine_container_strdup(src->name);
            dest->bytes = NULL;
            if (src->name && !dest->name) {
                machine_reloc_file_free(out_reloc_file);
                machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-104: out of memory cloning object section name");
                return 0;
            }
            if (src->byte_count > 0) {
                dest->bytes = (unsigned char *)malloc(src->byte_count);
                if (!dest->bytes) {
                    machine_reloc_file_free(out_reloc_file);
                    machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-105: out of memory cloning object section bytes");
                    return 0;
                }
                memcpy(dest->bytes, src->bytes, src->byte_count);
            }
        }
    }

    if (source->object_file.symbol_count > 0) {
        out_reloc_file->object_file.symbols =
            (MachineObjectSymbol *)calloc(source->object_file.symbol_count, sizeof(MachineObjectSymbol));
        if (!out_reloc_file->object_file.symbols) {
            machine_reloc_file_free(out_reloc_file);
            machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-106: out of memory cloning object symbols");
            return 0;
        }
        out_reloc_file->object_file.symbol_count = source->object_file.symbol_count;
        out_reloc_file->object_file.symbol_capacity = source->object_file.symbol_count;
        for (section_index = 0; section_index < source->object_file.symbol_count; ++section_index) {
            const MachineObjectSymbol *src = &source->object_file.symbols[section_index];
            MachineObjectSymbol *dest = &out_reloc_file->object_file.symbols[section_index];

            *dest = *src;
            dest->name = machine_container_strdup(src->name);
            if (src->name && !dest->name) {
                machine_reloc_file_free(out_reloc_file);
                machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-107: out of memory cloning object symbol name");
                return 0;
            }
        }
    }

    if (source->object_file.fixup_count > 0) {
        out_reloc_file->object_file.fixups =
            (MachineObjectFixup *)calloc(source->object_file.fixup_count, sizeof(MachineObjectFixup));
        if (!out_reloc_file->object_file.fixups) {
            machine_reloc_file_free(out_reloc_file);
            machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-108: out of memory cloning object fixups");
            return 0;
        }
        out_reloc_file->object_file.fixup_count = source->object_file.fixup_count;
        out_reloc_file->object_file.fixup_capacity = source->object_file.fixup_count;
        for (section_index = 0; section_index < source->object_file.fixup_count; ++section_index) {
            const MachineObjectFixup *src = &source->object_file.fixups[section_index];
            MachineObjectFixup *dest = &out_reloc_file->object_file.fixups[section_index];

            *dest = *src;
            dest->source_label_name = machine_container_strdup(src->source_label_name);
            dest->target_name = machine_container_strdup(src->target_name);
            if ((src->source_label_name && !dest->source_label_name) ||
                (src->target_name && !dest->target_name)) {
                machine_reloc_file_free(out_reloc_file);
                machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-109: out of memory cloning object fixup text");
                return 0;
            }
        }
    }
    out_reloc_file->object_file.total_byte_count = source->object_file.total_byte_count;

    if (source->section_count > 0) {
        out_reloc_file->sections = (MachineRelocSection *)calloc(source->section_count, sizeof(MachineRelocSection));
        if (!out_reloc_file->sections) {
            machine_reloc_file_free(out_reloc_file);
            machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-111: out of memory cloning relocation sections");
            return 0;
        }
        out_reloc_file->section_count = source->section_count;
        out_reloc_file->section_capacity = source->section_count;
        for (section_index = 0; section_index < source->section_count; ++section_index) {
            const MachineRelocSection *src = &source->sections[section_index];
            MachineRelocSection *dest = &out_reloc_file->sections[section_index];

            *dest = *src;
            dest->name = machine_container_strdup(src->name);
            if (src->name && !dest->name) {
                machine_reloc_file_free(out_reloc_file);
                machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-111: out of memory cloning relocation section name");
                return 0;
            }
        }
    }

    if (source->relocation_count > 0) {
        out_reloc_file->relocations = (MachineRelocation *)calloc(source->relocation_count, sizeof(MachineRelocation));
        if (!out_reloc_file->relocations) {
            machine_reloc_file_free(out_reloc_file);
            machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-112: out of memory cloning relocations");
            return 0;
        }
        out_reloc_file->relocation_count = source->relocation_count;
        out_reloc_file->relocation_capacity = source->relocation_count;
        for (relocation_index = 0; relocation_index < source->relocation_count; ++relocation_index) {
            const MachineRelocation *src = &source->relocations[relocation_index];
            MachineRelocation *dest = &out_reloc_file->relocations[relocation_index];

            *dest = *src;
            dest->source_label_name = machine_container_strdup(src->source_label_name);
            dest->target_name = machine_container_strdup(src->target_name);
            if ((src->source_label_name && !dest->source_label_name) ||
                (src->target_name && !dest->target_name)) {
                machine_reloc_file_free(out_reloc_file);
                machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-113: out of memory cloning relocation text");
                return 0;
            }
        }
    }

    return 1;
}

void machine_container_file_init(MachineContainerFile *container_file) {
    if (!container_file) {
        return;
    }
    machine_reloc_file_init(&container_file->reloc_file);
    container_file->sections = NULL;
    container_file->section_count = 0;
    container_file->section_capacity = 0;
    container_file->bytes = NULL;
    container_file->byte_count = 0;
    container_file->header_offset = 0;
    container_file->header_size = 0;
    container_file->section_table_offset = 0;
    container_file->symbol_table_offset = 0;
    container_file->relocation_table_offset = 0;
    container_file->string_table_offset = 0;
    container_file->string_table_size = 0;
    container_file->payload_offset = 0;
    container_file->payload_size = 0;
}

void machine_container_file_free(MachineContainerFile *container_file) {
    size_t section_index;

    if (!container_file) {
        return;
    }
    for (section_index = 0; section_index < container_file->section_count; ++section_index) {
        free(container_file->sections[section_index].name);
    }
    free(container_file->sections);
    free(container_file->bytes);
    machine_reloc_file_free(&container_file->reloc_file);
    machine_container_file_init(container_file);
}

int machine_container_file_get_summary(const MachineContainerFile *container_file,
    size_t *out_object_byte_count,
    size_t *out_section_count,
    size_t *out_symbol_count,
    size_t *out_relocation_count,
    size_t *out_container_byte_count) {
    if (!container_file) {
        return 0;
    }
    if (out_object_byte_count) {
        *out_object_byte_count = container_file->reloc_file.object_file.total_byte_count;
    }
    if (out_section_count) {
        *out_section_count = container_file->section_count;
    }
    if (out_symbol_count) {
        *out_symbol_count = container_file->reloc_file.object_file.symbol_count;
    }
    if (out_relocation_count) {
        *out_relocation_count = container_file->reloc_file.relocation_count;
    }
    if (out_container_byte_count) {
        *out_container_byte_count = container_file->byte_count;
    }
    return 1;
}

int machine_container_file_get_reloc_file(const MachineContainerFile *container_file,
    const MachineRelocFile **out_reloc_file) {
    if (!container_file || !out_reloc_file) {
        return 0;
    }
    *out_reloc_file = &container_file->reloc_file;
    return 1;
}

int machine_container_file_get_layout_summary(const MachineContainerFile *container_file,
    MachineContainerLayoutSummary *out_summary) {
    if (!container_file || !out_summary) {
        return 0;
    }
    out_summary->header_offset = container_file->header_offset;
    out_summary->header_size = container_file->header_size;
    out_summary->section_table_offset = container_file->section_table_offset;
    out_summary->symbol_table_offset = container_file->symbol_table_offset;
    out_summary->relocation_table_offset = container_file->relocation_table_offset;
    out_summary->string_table_offset = container_file->string_table_offset;
    out_summary->string_table_size = container_file->string_table_size;
    out_summary->payload_offset = container_file->payload_offset;
    out_summary->payload_size = container_file->payload_size;
    out_summary->total_byte_count = container_file->byte_count;
    return 1;
}

int machine_container_file_copy_bytes(const MachineContainerFile *container_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineContainerError *error) {
    unsigned char *copy;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0;
    }
    if (!container_file || !out_bytes || !container_file->bytes) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-115: invalid container byte-copy contract");
        return 0;
    }
    if (container_file->byte_count == 0) {
        return 1;
    }
    copy = (unsigned char *)malloc(container_file->byte_count);
    if (!copy) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-116: out of memory copying container bytes");
        return 0;
    }
    memcpy(copy, container_file->bytes, container_file->byte_count);
    *out_bytes = copy;
    if (out_byte_count) {
        *out_byte_count = container_file->byte_count;
    }
    return 1;
}

int machine_container_file_get_section(const MachineContainerFile *container_file,
    size_t section_index,
    const MachineContainerSection **out_section) {
    if (!container_file || !out_section || section_index >= container_file->section_count || !container_file->sections) {
        return 0;
    }
    *out_section = &container_file->sections[section_index];
    return 1;
}

int machine_container_file_find_section_by_name(const MachineContainerFile *container_file,
    const char *section_name,
    size_t *out_section_index,
    const MachineContainerSection **out_section) {
    size_t section_index;

    if (!container_file || !section_name || !container_file->sections) {
        return 0;
    }
    for (section_index = 0; section_index < container_file->section_count; ++section_index) {
        if (container_file->sections[section_index].name &&
            strcmp(container_file->sections[section_index].name, section_name) == 0) {
            if (out_section_index) {
                *out_section_index = section_index;
            }
            if (out_section) {
                *out_section = &container_file->sections[section_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_container_section_get_summary(const MachineContainerSection *section,
    MachineContainerSectionSummary *out_summary) {
    if (!section || !out_summary) {
        return 0;
    }
    out_summary->reloc_section_index = section->reloc_section_index;
    out_summary->object_section_index = section->object_section_index;
    out_summary->name = section->name;
    out_summary->logical_start_byte_offset = section->logical_start_byte_offset;
    out_summary->byte_count = section->byte_count;
    out_summary->file_offset = section->file_offset;
    out_summary->relocation_count = section->relocation_count;
    return 1;
}

int machine_container_build_from_machine_reloc_file(const MachineRelocFile *reloc_file,
    MachineContainerFile *out_container_file,
    MachineContainerError *error) {
    size_t section_index;
    size_t symbol_index;
    size_t relocation_index;
    size_t string_table_size = 0;
    size_t payload_size = 0;
    size_t payload_cursor;
    size_t string_cursor = 0;
    unsigned char *bytes;

    if (!reloc_file || !out_container_file) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-117: invalid container build contract");
        return 0;
    }

    machine_container_file_free(out_container_file);

    if (!machine_container_clone_reloc_file(reloc_file, &out_container_file->reloc_file, error)) {
        return 0;
    }

    if (reloc_file->section_count > 0) {
        out_container_file->sections =
            (MachineContainerSection *)calloc(reloc_file->section_count, sizeof(MachineContainerSection));
        if (!out_container_file->sections) {
            machine_container_file_free(out_container_file);
            machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-118: out of memory building container sections");
            return 0;
        }
        out_container_file->section_count = reloc_file->section_count;
        out_container_file->section_capacity = reloc_file->section_count;
    }

    for (section_index = 0; section_index < reloc_file->section_count; ++section_index) {
        const MachineRelocSection *src = &reloc_file->sections[section_index];
        MachineContainerSection *dest = &out_container_file->sections[section_index];

        dest->reloc_section_index = section_index;
        dest->object_section_index = src->object_section_index;
        dest->name = machine_container_strdup(src->name);
        dest->logical_start_byte_offset = src->start_byte_offset;
        dest->byte_count = src->byte_count;
        dest->relocation_start_index = src->relocation_start_index;
        dest->relocation_count = src->relocation_count;
        if (src->name && !dest->name) {
            machine_container_file_free(out_container_file);
            machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-119: out of memory copying container section name");
            return 0;
        }
        string_table_size += machine_container_measure_string(src->name);
        payload_size += src->byte_count;
    }

    for (symbol_index = 0; symbol_index < reloc_file->object_file.symbol_count; ++symbol_index) {
        string_table_size += machine_container_measure_string(reloc_file->object_file.symbols[symbol_index].name);
    }
    for (relocation_index = 0; relocation_index < reloc_file->relocation_count; ++relocation_index) {
        string_table_size += machine_container_measure_string(reloc_file->relocations[relocation_index].source_label_name);
        string_table_size += machine_container_measure_string(reloc_file->relocations[relocation_index].target_name);
    }

    out_container_file->header_offset = 0;
    out_container_file->header_size = MACHINE_CONTAINER_HEADER_SIZE;
    out_container_file->section_table_offset = out_container_file->header_size;
    out_container_file->symbol_table_offset =
        out_container_file->section_table_offset + reloc_file->section_count * MACHINE_CONTAINER_SECTION_ENTRY_SIZE;
    out_container_file->relocation_table_offset =
        out_container_file->symbol_table_offset +
        reloc_file->object_file.symbol_count * MACHINE_CONTAINER_SYMBOL_ENTRY_SIZE;
    out_container_file->string_table_offset =
        out_container_file->relocation_table_offset +
        reloc_file->relocation_count * MACHINE_CONTAINER_RELOCATION_ENTRY_SIZE;
    out_container_file->string_table_size = string_table_size;
    out_container_file->payload_offset = out_container_file->string_table_offset + string_table_size;
    out_container_file->payload_size = payload_size;
    out_container_file->byte_count = out_container_file->payload_offset + payload_size;

    bytes = (unsigned char *)calloc(out_container_file->byte_count == 0 ? 1u : out_container_file->byte_count, 1u);
    if (!bytes) {
        machine_container_file_free(out_container_file);
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-120: out of memory allocating container bytes");
        return 0;
    }
    out_container_file->bytes = bytes;

    bytes[0] = MACHINE_CONTAINER_MAGIC_0;
    bytes[1] = MACHINE_CONTAINER_MAGIC_1;
    bytes[2] = MACHINE_CONTAINER_MAGIC_2;
    bytes[3] = MACHINE_CONTAINER_MAGIC_3;
    machine_container_write_u32_le(bytes, 4u, MACHINE_CONTAINER_VERSION);
    machine_container_write_u32_le(bytes, 8u, (uint32_t)reloc_file->section_count);
    machine_container_write_u32_le(bytes, 12u, (uint32_t)reloc_file->object_file.symbol_count);
    machine_container_write_u32_le(bytes, 16u, (uint32_t)reloc_file->relocation_count);
    machine_container_write_u32_le(bytes, 20u, (uint32_t)out_container_file->section_table_offset);
    machine_container_write_u32_le(bytes, 24u, (uint32_t)out_container_file->symbol_table_offset);
    machine_container_write_u32_le(bytes, 28u, (uint32_t)out_container_file->relocation_table_offset);
    machine_container_write_u32_le(bytes, 32u, (uint32_t)out_container_file->string_table_offset);
    machine_container_write_u32_le(bytes, 36u, (uint32_t)out_container_file->payload_offset);
    machine_container_write_u32_le(bytes, 40u, (uint32_t)out_container_file->byte_count);

    payload_cursor = out_container_file->payload_offset;
    for (section_index = 0; section_index < reloc_file->section_count; ++section_index) {
        const MachineRelocSection *src = &reloc_file->sections[section_index];
        const MachineObjectSection *object_section = &reloc_file->object_file.sections[src->object_section_index];
        MachineContainerSection *dest = &out_container_file->sections[section_index];
        size_t entry_offset = out_container_file->section_table_offset + section_index * MACHINE_CONTAINER_SECTION_ENTRY_SIZE;
        uint32_t name_offset =
            machine_container_append_string(bytes, out_container_file->string_table_offset, string_table_size, &string_cursor, src->name);

        dest->file_offset = payload_cursor;
        machine_container_write_u32_le(bytes, entry_offset + 0u, name_offset);
        machine_container_write_u32_le(bytes, entry_offset + 4u, (uint32_t)src->object_section_index);
        machine_container_write_u32_le(bytes, entry_offset + 8u, (uint32_t)src->start_byte_offset);
        machine_container_write_u32_le(bytes, entry_offset + 12u, (uint32_t)src->byte_count);
        machine_container_write_u32_le(bytes, entry_offset + 16u, (uint32_t)payload_cursor);
        machine_container_write_u32_le(bytes, entry_offset + 20u, (uint32_t)src->relocation_start_index);
        machine_container_write_u32_le(bytes, entry_offset + 24u, (uint32_t)src->relocation_count);
        if (object_section->byte_count > 0) {
            memcpy(bytes + payload_cursor, object_section->bytes, object_section->byte_count);
        }
        payload_cursor += object_section->byte_count;
    }

    for (symbol_index = 0; symbol_index < reloc_file->object_file.symbol_count; ++symbol_index) {
        const MachineObjectSymbol *symbol = &reloc_file->object_file.symbols[symbol_index];
        size_t entry_offset = out_container_file->symbol_table_offset + symbol_index * MACHINE_CONTAINER_SYMBOL_ENTRY_SIZE;
        uint32_t flags = 0u;
        uint32_t name_offset =
            machine_container_append_string(bytes, out_container_file->string_table_offset, string_table_size, &string_cursor, symbol->name);

        if (symbol->is_defined) {
            flags |= 1u;
        }
        if (symbol->has_section_index) {
            flags |= 2u;
        }
        machine_container_write_u32_le(bytes, entry_offset + 0u, name_offset);
        machine_container_write_u32_le(bytes, entry_offset + 4u, (uint32_t)symbol->kind);
        machine_container_write_u32_le(bytes, entry_offset + 8u, flags);
        machine_container_write_u32_le(bytes, entry_offset + 12u,
            symbol->has_section_index ? (uint32_t)symbol->section_index : MACHINE_CONTAINER_NONE_U32);
        machine_container_write_u32_le(bytes, entry_offset + 16u, (uint32_t)symbol->byte_offset_in_section);
        machine_container_write_u32_le(bytes, entry_offset + 20u, (uint32_t)symbol->byte_count);
        machine_container_write_u32_le(bytes, entry_offset + 24u, (uint32_t)symbol->incoming_fixup_count);
        machine_container_write_u32_le(bytes, entry_offset + 28u,
            symbol->has_function_index ? (uint32_t)symbol->function_index : MACHINE_CONTAINER_NONE_U32);
        machine_container_write_u32_le(bytes, entry_offset + 32u,
            symbol->has_emit_index ? (uint32_t)symbol->emit_index : MACHINE_CONTAINER_NONE_U32);
    }

    for (relocation_index = 0; relocation_index < reloc_file->relocation_count; ++relocation_index) {
        const MachineRelocation *relocation = &reloc_file->relocations[relocation_index];
        size_t entry_offset =
            out_container_file->relocation_table_offset + relocation_index * MACHINE_CONTAINER_RELOCATION_ENTRY_SIZE;
        uint32_t source_name_offset =
            machine_container_append_string(
                bytes,
                out_container_file->string_table_offset,
                string_table_size,
                &string_cursor,
                relocation->source_label_name);
        uint32_t target_name_offset =
            machine_container_append_string(
                bytes,
                out_container_file->string_table_offset,
                string_table_size,
                &string_cursor,
                relocation->target_name);

        machine_container_write_u32_le(bytes, entry_offset + 0u, (uint32_t)relocation->kind);
        machine_container_write_u32_le(bytes, entry_offset + 4u, (uint32_t)relocation->target_kind);
        machine_container_write_u32_le(bytes, entry_offset + 8u, (uint32_t)relocation->object_section_index);
        machine_container_write_u32_le(bytes, entry_offset + 12u, source_name_offset);
        machine_container_write_u32_le(bytes, entry_offset + 16u, (uint32_t)relocation->owner_byte_offset);
        machine_container_write_u32_le(bytes, entry_offset + 20u, (uint32_t)relocation->owner_byte_count);
        machine_container_write_u32_le(bytes, entry_offset + 24u, (uint32_t)relocation->patch_byte_offset);
        machine_container_write_u32_le(bytes, entry_offset + 28u, (uint32_t)relocation->patch_byte_count);
        machine_container_write_u32_le(bytes, entry_offset + 32u, target_name_offset);
        machine_container_write_u32_le(bytes, entry_offset + 36u,
            relocation->has_target_symbol_index ? (uint32_t)relocation->target_symbol_index : MACHINE_CONTAINER_NONE_U32);
        machine_container_write_u32_le(bytes, entry_offset + 40u,
            relocation->has_target_section_index ? (uint32_t)relocation->target_section_index : MACHINE_CONTAINER_NONE_U32);
        machine_container_write_u32_le(bytes, entry_offset + 44u,
            relocation->has_target_byte_offset ? (uint32_t)relocation->target_byte_offset : MACHINE_CONTAINER_NONE_U32);
        machine_container_write_u32_le(bytes, entry_offset + 48u, (uint32_t)(int32_t)relocation->addend);
    }

    if (string_cursor != string_table_size || payload_cursor != out_container_file->byte_count) {
        machine_container_file_free(out_container_file);
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-121: container layout assembly mismatch");
        return 0;
    }

    return machine_container_verify_file(out_container_file, error);
}

int machine_container_build_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineBytesTargetProfile profile,
    MachineContainerFile *out_container_file,
    MachineContainerError *error) {
    MachineRelocFile reloc_file;
    MachineRelocError reloc_error;
    int ok;

    if (!report || !out_container_file) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-122: invalid machine-ir container build contract");
        return 0;
    }
    machine_reloc_file_init(&reloc_file);
    memset(&reloc_error, 0, sizeof(reloc_error));
    ok = machine_reloc_build_from_machine_ir_report_with_profile(report, profile, &reloc_file, &reloc_error);
    if (!ok) {
        machine_container_set_error(error, reloc_error.line, reloc_error.column, "%s", reloc_error.message);
        machine_reloc_file_free(&reloc_file);
        return 0;
    }
    ok = machine_container_build_from_machine_reloc_file(&reloc_file, out_container_file, error);
    machine_reloc_file_free(&reloc_file);
    return ok;
}

int machine_container_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineContainerFile *out_container_file,
    MachineContainerError *error) {
    return machine_container_build_from_machine_ir_report_with_profile(
        report,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_container_file,
        error);
}

int machine_container_verify_file(const MachineContainerFile *container_file, MachineContainerError *error) {
    size_t expected_section_table_offset;
    size_t expected_symbol_table_offset;
    size_t expected_relocation_table_offset;
    size_t expected_string_table_offset;
    size_t expected_payload_offset;
    size_t section_index;

    if (!container_file) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-123: invalid container verifier contract");
        return 0;
    }
    if (!machine_reloc_verify_file(&container_file->reloc_file, NULL)) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-124: embedded relocation file failed verification");
        return 0;
    }
    if ((container_file->section_count > 0 && !container_file->sections) ||
        container_file->section_count > container_file->section_capacity ||
        (container_file->byte_count > 0 && !container_file->bytes)) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-125: invalid container metadata");
        return 0;
    }
    if (container_file->byte_count < MACHINE_CONTAINER_HEADER_SIZE) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-126: container byte image too small");
        return 0;
    }
    if (container_file->bytes[0] != MACHINE_CONTAINER_MAGIC_0 ||
        container_file->bytes[1] != MACHINE_CONTAINER_MAGIC_1 ||
        container_file->bytes[2] != MACHINE_CONTAINER_MAGIC_2 ||
        container_file->bytes[3] != MACHINE_CONTAINER_MAGIC_3) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-127: invalid container magic");
        return 0;
    }
    expected_section_table_offset = MACHINE_CONTAINER_HEADER_SIZE;
    expected_symbol_table_offset =
        expected_section_table_offset + container_file->section_count * MACHINE_CONTAINER_SECTION_ENTRY_SIZE;
    expected_relocation_table_offset =
        expected_symbol_table_offset + container_file->reloc_file.object_file.symbol_count * MACHINE_CONTAINER_SYMBOL_ENTRY_SIZE;
    expected_string_table_offset =
        expected_relocation_table_offset + container_file->reloc_file.relocation_count * MACHINE_CONTAINER_RELOCATION_ENTRY_SIZE;
    expected_payload_offset = expected_string_table_offset + container_file->string_table_size;
    if (container_file->header_offset != 0 ||
        container_file->header_size != MACHINE_CONTAINER_HEADER_SIZE ||
        container_file->section_table_offset != expected_section_table_offset ||
        container_file->symbol_table_offset != expected_symbol_table_offset ||
        container_file->relocation_table_offset != expected_relocation_table_offset ||
        container_file->string_table_offset != expected_string_table_offset ||
        container_file->payload_offset != expected_payload_offset ||
        container_file->payload_offset + container_file->payload_size != container_file->byte_count) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-128: invalid container layout metadata");
        return 0;
    }
    for (section_index = 0; section_index < container_file->section_count; ++section_index) {
        const MachineContainerSection *section = &container_file->sections[section_index];
        const MachineRelocSection *reloc_section = &container_file->reloc_file.sections[section_index];

        if (!section->name || section->name[0] == '\0' ||
            section->reloc_section_index != section_index ||
            section->object_section_index != reloc_section->object_section_index ||
            section->logical_start_byte_offset != reloc_section->start_byte_offset ||
            section->byte_count != reloc_section->byte_count ||
            section->relocation_start_index != reloc_section->relocation_start_index ||
            section->relocation_count != reloc_section->relocation_count ||
            section->file_offset < container_file->payload_offset ||
            section->file_offset + section->byte_count > container_file->byte_count) {
            machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-129: invalid container section metadata");
            return 0;
        }
    }
    return 1;
}

int machine_container_dump_file(const MachineContainerFile *container_file,
    char **out_text,
    MachineContainerError *error) {
    MachineContainerStringBuilder builder;
    size_t section_index;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!container_file || !out_text) {
        machine_container_set_error(error, 0, 0, "MACHINE-CONTAINER-130: invalid container dump contract");
        return 0;
    }
    if (!machine_container_verify_file(container_file, error)) {
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_container_append_format(
            &builder,
            "machine_container container_bytes=%zu object_bytes=%zu sections=%zu symbols=%zu relocations=%zu\nlayout header=%zu..%zu section_table=%zu symbol_table=%zu relocation_table=%zu string_table=%zu..%zu payload=%zu..%zu\nsections:\n",
            container_file->byte_count,
            container_file->reloc_file.object_file.total_byte_count,
            container_file->section_count,
            container_file->reloc_file.object_file.symbol_count,
            container_file->reloc_file.relocation_count,
            container_file->header_offset,
            container_file->header_offset + container_file->header_size,
            container_file->section_table_offset,
            container_file->symbol_table_offset,
            container_file->relocation_table_offset,
            container_file->string_table_offset,
            container_file->string_table_offset + container_file->string_table_size,
            container_file->payload_offset,
            container_file->payload_offset + container_file->payload_size)) {
        goto cleanup;
    }
    for (section_index = 0; section_index < container_file->section_count; ++section_index) {
        const MachineContainerSection *section = &container_file->sections[section_index];

        if (!machine_container_append_format(
                &builder,
                "  csec.%zu %s file@%zu bytes=%zu logical=%zu relocations=%zu\n",
                section_index,
                section->name ? section->name : "<null>",
                section->file_offset,
                section->byte_count,
                section->logical_start_byte_offset,
                section->relocation_count)) {
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

int machine_container_build_dump_from_machine_reloc_file(const MachineRelocFile *reloc_file,
    char **out_text,
    MachineContainerError *error) {
    MachineContainerFile container_file;
    int ok;

    machine_container_file_init(&container_file);
    ok = machine_container_build_from_machine_reloc_file(reloc_file, &container_file, error) &&
        machine_container_dump_file(&container_file, out_text, error);
    machine_container_file_free(&container_file);
    return ok;
}

int machine_container_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineContainerError *error) {
    MachineContainerFile container_file;
    int ok;

    machine_container_file_init(&container_file);
    ok = machine_container_build_from_machine_ir_report(report, &container_file, error) &&
        machine_container_dump_file(&container_file, out_text, error);
    machine_container_file_free(&container_file);
    return ok;
}
