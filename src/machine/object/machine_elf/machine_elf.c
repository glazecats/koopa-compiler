#include "machine/elf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <elf.h>

#define MACHINE_ELF_EI_NIDENT 16u
#define MACHINE_ELF_HEADER_SIZE 52u
#define MACHINE_ELF_SECTION_HEADER_SIZE 40u
#define MACHINE_ELF_SYMBOL_ENTRY_SIZE 16u
#define MACHINE_ELF_RELOCATION_ENTRY_SIZE 8u
#define MACHINE_ELF_ET_REL 1u
#define MACHINE_ELF_EM_NONE 0u
#define MACHINE_ELF_EV_CURRENT 1u
#define MACHINE_ELF_SHN_UNDEF 0u
#define MACHINE_ELF_SHF_WRITE 0x1u
#define MACHINE_ELF_SHF_ALLOC 0x2u
#define MACHINE_ELF_SHF_EXECINSTR 0x4u

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineElfStringBuilder;

typedef struct {
    MachineElfTargetProfile profile;
    uint16_t machine;
    uint32_t flags;
} MachineElfTargetPolicy;

static void machine_elf_set_error(MachineElfError *error, int line, int column, const char *fmt, ...);
static char *machine_elf_strdup(const char *text);
static int machine_elf_append_format(MachineElfStringBuilder *builder, const char *fmt, ...);
static void machine_elf_write_u16_le(unsigned char *bytes, size_t offset, uint16_t value);
static void machine_elf_write_u32_le(unsigned char *bytes, size_t offset, uint32_t value);
static uint16_t machine_elf_read_u16_le(const unsigned char *bytes, size_t offset);
static uint32_t machine_elf_read_u32_le(const unsigned char *bytes, size_t offset);
static size_t machine_elf_align_up(size_t value, size_t align);
static uint32_t machine_elf_append_string(unsigned char *bytes,
    size_t string_table_offset,
    size_t string_table_size,
    size_t *io_cursor,
    const char *text);
static int machine_elf_get_target_policy(MachineElfTargetProfile profile,
    MachineElfTargetPolicy *out_policy);
static MachineBytesTargetProfile machine_elf_bytes_target_profile(MachineElfTargetProfile profile);
static uint16_t machine_elf_expected_machine_for_profile(MachineElfTargetProfile profile);
static uint32_t machine_elf_expected_flags_for_profile(MachineElfTargetProfile profile);
static uint32_t machine_elf_map_relocation_type(MachineElfTargetProfile profile, MachineRelocKind kind);
static const char *machine_elf_relocation_kind_name(MachineRelocKind kind);
static MachineElfTargetProfile machine_elf_profile_from_header(uint16_t machine, uint32_t flags);
static MachineRelocKind machine_elf_relocation_kind_from_type(MachineElfTargetProfile profile, uint32_t type);
static int machine_elf_string_length_within_limit(const unsigned char *bytes,
    size_t start_offset,
    size_t limit,
    size_t *out_length);
static int machine_elf_section_name_equals(const MachineElfSection *section, const char *name);
static int machine_elf_replace_owned_string(char **slot, const char *text);
static int machine_elf_symbol_is_droppable_import_artifact(const MachineElfSymbol *symbol);
static int machine_elf_symbol_is_droppable_on_refresh(const MachineElfSymbol *symbol, int referenced_by_relocation);
static int machine_elf_symbol_has_unsupported_defined_section(const MachineElfSymbol *symbol);
static int machine_elf_symbol_is_canonical_null(const MachineElfSymbol *symbol);

static void machine_elf_set_error(MachineElfError *error, int line, int column, const char *fmt, ...) {
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

static char *machine_elf_strdup(const char *text) {
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

static int machine_elf_append_format(MachineElfStringBuilder *builder, const char *fmt, ...) {
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

static void machine_elf_write_u16_le(unsigned char *bytes, size_t offset, uint16_t value) {
    bytes[offset] = (unsigned char)(value & 0xFFu);
    bytes[offset + 1u] = (unsigned char)((value >> 8u) & 0xFFu);
}

static void machine_elf_write_u32_le(unsigned char *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (unsigned char)(value & 0xFFu);
    bytes[offset + 1u] = (unsigned char)((value >> 8u) & 0xFFu);
    bytes[offset + 2u] = (unsigned char)((value >> 16u) & 0xFFu);
    bytes[offset + 3u] = (unsigned char)((value >> 24u) & 0xFFu);
}

static uint16_t machine_elf_read_u16_le(const unsigned char *bytes, size_t offset) {
    return (uint16_t)((uint16_t)bytes[offset] | ((uint16_t)bytes[offset + 1u] << 8u));
}

static uint32_t machine_elf_read_u32_le(const unsigned char *bytes, size_t offset) {
    return (uint32_t)bytes[offset] |
        ((uint32_t)bytes[offset + 1u] << 8u) |
        ((uint32_t)bytes[offset + 2u] << 16u) |
        ((uint32_t)bytes[offset + 3u] << 24u);
}

static size_t machine_elf_align_up(size_t value, size_t align) {
    size_t remainder;

    if (align == 0u) {
        return value;
    }
    remainder = value % align;
    if (remainder == 0u) {
        return value;
    }
    return value + (align - remainder);
}

static uint32_t machine_elf_append_string(unsigned char *bytes,
    size_t string_table_offset,
    size_t string_table_size,
    size_t *io_cursor,
    const char *text) {
    size_t length;
    size_t start;

    if (!text) {
        return 0u;
    }
    length = strlen(text) + 1u;
    start = *io_cursor;
    if (start + length > string_table_size) {
        return 0u;
    }
    memcpy(bytes + string_table_offset + start, text, length);
    *io_cursor = start + length;
    return (uint32_t)start;
}

const char *machine_elf_target_profile_name(MachineElfTargetProfile profile) {
    switch (profile) {
        case MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32:
            return "generic-elf32";
        case MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW:
            return "riscv32-preview";
        case MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW:
            return "i386-preview";
    }
    return "unknown";
}

const char *machine_elf_relocation_semantics_name(MachineElfRelocationSemantics semantics) {
    switch (semantics) {
        case MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS:
            return "direct-patch-spans";
        case MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE:
            return "imported-relocation-table";
        case MACHINE_ELF_RELOCATION_SEMANTICS_UNKNOWN:
        default:
            return "unknown";
    }
}

int machine_elf_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineElfTargetPolicySummary *out_summary) {
    MachineElfTargetPolicy policy;

    if (!out_summary || !machine_elf_get_target_policy(profile, &policy)) {
        return 0;
    }
    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->target_profile = profile;
    if (!machine_bytes_get_target_policy_summary(
            machine_elf_bytes_target_profile(profile), &out_summary->bytes_policy)) {
        return 0;
    }
    out_summary->machine = policy.machine;
    out_summary->flags = policy.flags;
    out_summary->call_relocation_type =
        machine_elf_map_relocation_type(profile, MACHINE_BYTES_FIXUP_CALL_TARGET);
    out_summary->primary_control_relocation_type =
        machine_elf_map_relocation_type(profile, MACHINE_BYTES_FIXUP_CONTROL_PRIMARY);
    out_summary->secondary_control_relocation_type =
        machine_elf_map_relocation_type(profile, MACHINE_BYTES_FIXUP_CONTROL_SECONDARY);
    out_summary->data_address_relocation_type =
        machine_elf_map_relocation_type(profile, MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET);
    out_summary->data_load_relocation_type =
        machine_elf_map_relocation_type(profile, MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET);
    out_summary->data_store_relocation_type =
        machine_elf_map_relocation_type(profile, MACHINE_BYTES_FIXUP_DATA_STORE_TARGET);
    return 1;
}

static int machine_elf_get_target_policy(MachineElfTargetProfile profile,
    MachineElfTargetPolicy *out_policy) {
    switch (profile) {
        case MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32:
            if (out_policy) {
                out_policy->profile = profile;
                out_policy->machine = MACHINE_ELF_EM_NONE;
                out_policy->flags = 0u;
            }
            return 1;
        case MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW:
            if (out_policy) {
                out_policy->profile = profile;
                out_policy->machine = (uint16_t)EM_RISCV;
                out_policy->flags = 0u;
            }
            return 1;
        case MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW:
            if (out_policy) {
                out_policy->profile = profile;
                out_policy->machine = (uint16_t)EM_386;
                out_policy->flags = 0u;
            }
            return 1;
    }
    return 0;
}

static uint16_t machine_elf_expected_machine_for_profile(MachineElfTargetProfile profile) {
    MachineElfTargetPolicy policy;

    if (!machine_elf_get_target_policy(profile, &policy)) {
        return MACHINE_ELF_EM_NONE;
    }
    return policy.machine;
}

static uint32_t machine_elf_expected_flags_for_profile(MachineElfTargetProfile profile) {
    MachineElfTargetPolicy policy;

    if (!machine_elf_get_target_policy(profile, &policy)) {
        return 0u;
    }
    return policy.flags;
}

static MachineBytesTargetProfile machine_elf_bytes_target_profile(MachineElfTargetProfile profile) {
    switch (profile) {
        case MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW:
            return MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW;
        case MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW:
            return MACHINE_BYTES_TARGET_PROFILE_I386_PREVIEW;
        case MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32:
        default:
            return MACHINE_BYTES_TARGET_PROFILE_GENERIC;
    }
}

static MachineElfTargetProfile machine_elf_profile_from_bytes_target_profile(MachineBytesTargetProfile profile) {
    switch (profile) {
        case MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW:
            return MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW;
        case MACHINE_BYTES_TARGET_PROFILE_I386_PREVIEW:
            return MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW;
        case MACHINE_BYTES_TARGET_PROFILE_GENERIC:
        default:
            return MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    }
}

static uint32_t machine_elf_map_relocation_type(MachineElfTargetProfile profile, MachineRelocKind kind) {
    if (profile == MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW) {
        switch (kind) {
            case MACHINE_BYTES_FIXUP_CALL_TARGET:
                return (uint32_t)R_RISCV_CALL;
            case MACHINE_BYTES_FIXUP_CONTROL_PRIMARY:
                return (uint32_t)R_RISCV_BRANCH;
            case MACHINE_BYTES_FIXUP_CONTROL_SECONDARY:
                return (uint32_t)R_RISCV_JAL;
            case MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET:
                return (uint32_t)R_RISCV_HI20;
            case MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET:
                return (uint32_t)R_RISCV_LO12_I;
            case MACHINE_BYTES_FIXUP_DATA_STORE_TARGET:
                return (uint32_t)R_RISCV_LO12_S;
        }
        return 0u;
    }
    if (profile == MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW) {
        switch (kind) {
            case MACHINE_BYTES_FIXUP_CALL_TARGET:
                return (uint32_t)R_386_PLT32;
            case MACHINE_BYTES_FIXUP_CONTROL_PRIMARY:
                return (uint32_t)R_386_PC32;
            case MACHINE_BYTES_FIXUP_CONTROL_SECONDARY:
                return (uint32_t)R_386_32PLT;
            case MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET:
                return (uint32_t)R_386_32;
            case MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET:
                return (uint32_t)R_386_GOT32;
            case MACHINE_BYTES_FIXUP_DATA_STORE_TARGET:
                return (uint32_t)R_386_GOTOFF;
        }
        return 0u;
    }

    switch (kind) {
        case MACHINE_BYTES_FIXUP_CALL_TARGET:
            return 1u;
        case MACHINE_BYTES_FIXUP_CONTROL_PRIMARY:
            return 2u;
        case MACHINE_BYTES_FIXUP_CONTROL_SECONDARY:
            return 3u;
        case MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET:
            return 4u;
        case MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET:
            return 5u;
        case MACHINE_BYTES_FIXUP_DATA_STORE_TARGET:
            return 6u;
    }
    return 0u;
}

static const char *machine_elf_relocation_kind_name(MachineRelocKind kind) {
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
    }
    return "unknown";
}

static MachineElfTargetProfile machine_elf_profile_from_header(uint16_t machine, uint32_t flags) {
    if (machine == (uint16_t)EM_RISCV && flags == 0u) {
        return MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW;
    }
    if (machine == (uint16_t)EM_386 && flags == 0u) {
        return MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW;
    }
    if (machine == MACHINE_ELF_EM_NONE && flags == 0u) {
        return MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    }
    return (MachineElfTargetProfile)-1;
}

static MachineRelocKind machine_elf_relocation_kind_from_type(MachineElfTargetProfile profile, uint32_t type) {
    if (profile == MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW) {
        switch (type) {
            case R_RISCV_CALL:
                return MACHINE_BYTES_FIXUP_CALL_TARGET;
            case R_RISCV_BRANCH:
                return MACHINE_BYTES_FIXUP_CONTROL_PRIMARY;
            case R_RISCV_JAL:
                return MACHINE_BYTES_FIXUP_CONTROL_SECONDARY;
            case R_RISCV_HI20:
                return MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET;
            case R_RISCV_LO12_I:
                return MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET;
            case R_RISCV_LO12_S:
                return MACHINE_BYTES_FIXUP_DATA_STORE_TARGET;
        }
        return (MachineRelocKind)-1;
    }
    if (profile == MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW) {
        switch (type) {
            case R_386_PLT32:
                return MACHINE_BYTES_FIXUP_CALL_TARGET;
            case R_386_PC32:
                return MACHINE_BYTES_FIXUP_CONTROL_PRIMARY;
            case R_386_32PLT:
                return MACHINE_BYTES_FIXUP_CONTROL_SECONDARY;
            case R_386_32:
                return MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET;
            case R_386_GOT32:
                return MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET;
            case R_386_GOTOFF:
                return MACHINE_BYTES_FIXUP_DATA_STORE_TARGET;
        }
        return (MachineRelocKind)-1;
    }

    switch (type) {
        case 1u:
            return MACHINE_BYTES_FIXUP_CALL_TARGET;
        case 2u:
            return MACHINE_BYTES_FIXUP_CONTROL_PRIMARY;
        case 3u:
            return MACHINE_BYTES_FIXUP_CONTROL_SECONDARY;
        case 4u:
            return MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET;
        case 5u:
            return MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET;
        case 6u:
            return MACHINE_BYTES_FIXUP_DATA_STORE_TARGET;
    }
    return (MachineRelocKind)-1;
}

static int machine_elf_string_length_within_limit(const unsigned char *bytes,
    size_t start_offset,
    size_t limit,
    size_t *out_length) {
    size_t cursor;

    if (!bytes || !out_length || start_offset > limit) {
        return 0;
    }
    for (cursor = start_offset; cursor < limit; ++cursor) {
        if (bytes[cursor] == '\0') {
            *out_length = cursor - start_offset;
            return 1;
        }
    }
    return 0;
}

static int machine_elf_section_name_equals(const MachineElfSection *section, const char *name) {
    if (!section || !section->name || !name) {
        return 0;
    }
    return strcmp(section->name, name) == 0;
}

static int machine_elf_replace_owned_string(char **slot, const char *text) {
    char *replacement;

    if (!slot || !text) {
        return 0;
    }
    replacement = machine_elf_strdup(text);
    if (!replacement) {
        return 0;
    }
    free(*slot);
    *slot = replacement;
    return 1;
}

static int machine_elf_symbol_is_droppable_import_artifact(const MachineElfSymbol *symbol) {
    if (!symbol || symbol->binding != MACHINE_ELF_SYMBOL_LOCAL || !symbol->is_defined) {
        return 0;
    }
    if (machine_elf_symbol_has_unsupported_defined_section(symbol)) {
        return 1;
    }
    if (symbol->type == MACHINE_ELF_SYMBOL_SECTION) {
        return 1;
    }
    if (symbol->type == MACHINE_ELF_SYMBOL_FILE) {
        return 1;
    }
    return symbol->name && symbol->name[0] == '\0';
}

static int machine_elf_symbol_is_droppable_on_refresh(const MachineElfSymbol *symbol, int referenced_by_relocation) {
    if (machine_elf_symbol_has_unsupported_defined_section(symbol) && !referenced_by_relocation) {
        return 1;
    }
    if (machine_elf_symbol_is_droppable_import_artifact(symbol)) {
        return 1;
    }
    return symbol && !symbol->is_defined && !referenced_by_relocation;
}

static int machine_elf_symbol_has_unsupported_defined_section(const MachineElfSymbol *symbol) {
    return symbol && symbol->is_defined && symbol->section_index == (size_t)-1;
}

static int machine_elf_symbol_is_canonical_null(const MachineElfSymbol *symbol) {
    return symbol &&
        symbol->name &&
        symbol->name[0] == '\0' &&
        symbol->binding == MACHINE_ELF_SYMBOL_LOCAL &&
        symbol->type == MACHINE_ELF_SYMBOL_NOTYPE &&
        !symbol->is_defined &&
        symbol->section_index == 0u &&
        symbol->value == 0u &&
        symbol->size == 0u;
}

#define MACHINE_ELF_SPLIT_AGGREGATOR
#include "machine_elf_core.inc"
#include "machine_elf_build.inc"
#include "machine_elf_parse.inc"
#include "machine_elf_verify.inc"
#include "machine_elf_query.inc"
#include "machine_elf_dump.inc"
