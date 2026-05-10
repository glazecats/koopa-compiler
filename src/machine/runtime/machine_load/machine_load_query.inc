#if defined(__INTELLISENSE__) && !defined(MACHINE_LOAD_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LOAD_SPLIT_AGGREGATOR)
#define MACHINE_LOAD_SPLIT_INTELLISENSE_CONTEXT
#include "machine_load.c"

#else

static int machine_load_file_has_segment_table(const MachineLoadFile *load_file) {
    return load_file && (load_file->segment_count == 0u || load_file->segments);
}

int machine_load_file_get_summary(const MachineLoadFile *load_file,
    size_t *out_segment_count,
    size_t *out_file_byte_count,
    size_t *out_memory_byte_count,
    size_t *out_executable_segment_count) {
    size_t executable_segment_count = 0u;
    size_t segment_index;

    if (!machine_load_file_has_segment_table(load_file)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < load_file->segment_count; ++segment_index) {
        if (load_file->segments[segment_index].executable) {
            ++executable_segment_count;
        }
    }
    if (out_segment_count) {
        *out_segment_count = load_file->segment_count;
    }
    if (out_file_byte_count) {
        *out_file_byte_count = load_file->exec_file.image_file.byte_count;
    }
    if (out_memory_byte_count) {
        *out_memory_byte_count = load_file->total_memory_byte_count;
    }
    if (out_executable_segment_count) {
        *out_executable_segment_count = executable_segment_count;
    }
    return 1;
}

int machine_load_file_get_header_summary(const MachineLoadFile *load_file,
    MachineLoadHeaderSummary *out_summary) {
    if (!load_file || !out_summary) {
        return 0;
    }
    out_summary->target_profile = load_file->exec_file.image_file.target_profile;
    out_summary->base_virtual_address = load_file->exec_file.image_file.base_virtual_address;
    out_summary->entry_virtual_address = load_file->entry_virtual_address;
    out_summary->segment_count = load_file->segment_count;
    out_summary->file_byte_count = load_file->exec_file.image_file.byte_count;
    out_summary->memory_byte_count = load_file->total_memory_byte_count;
    return 1;
}

int machine_load_file_get_source_elf_artifact_summary(const MachineLoadFile *load_file,
    MachineElfArtifactSummary *out_summary) {
    if (!load_file || !out_summary) {
        return 0;
    }
    *out_summary = load_file->exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

int machine_load_file_get_memory_summary(const MachineLoadFile *load_file,
    MachineLoadMemorySummary *out_summary) {
    size_t segment_index;
    size_t base_virtual_address;
    size_t end_virtual_address;

    if (!machine_load_file_has_segment_table(load_file) ||
        !out_summary ||
        load_file->segment_count == 0u) {
        return 0;
    }
    base_virtual_address = load_file->segments[0].virtual_address;
    if (!machine_load_try_add_size(
            load_file->segments[0].virtual_address,
            load_file->segments[0].memory_byte_count,
            &end_virtual_address)) {
        return 0;
    }
    for (segment_index = 1u; segment_index < load_file->segment_count; ++segment_index) {
        const MachineLoadSegment *segment = &load_file->segments[segment_index];
        size_t segment_end = 0u;

        if (!machine_load_try_add_size(segment->virtual_address, segment->memory_byte_count, &segment_end)) {
            return 0;
        }

        if (segment->virtual_address < base_virtual_address) {
            base_virtual_address = segment->virtual_address;
        }
        if (segment_end > end_virtual_address) {
            end_virtual_address = segment_end;
        }
    }
    out_summary->base_virtual_address = base_virtual_address;
    out_summary->end_virtual_address = end_virtual_address;
    out_summary->memory_byte_count = end_virtual_address - base_virtual_address;
    out_summary->entry_offset = load_file->entry_virtual_address - base_virtual_address;
    return 1;
}

int machine_load_file_get_segment(const MachineLoadFile *load_file,
    size_t segment_index,
    const MachineLoadSegment **out_segment) {
    if (!machine_load_file_has_segment_table(load_file) ||
        !out_segment ||
        segment_index >= load_file->segment_count) {
        return 0;
    }
    *out_segment = &load_file->segments[segment_index];
    return 1;
}

int machine_load_file_find_segment_by_name(const MachineLoadFile *load_file,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineLoadSegment **out_segment) {
    size_t segment_index;

    if (!machine_load_file_has_segment_table(load_file) || !segment_name) {
        return 0;
    }
    for (segment_index = 0u; segment_index < load_file->segment_count; ++segment_index) {
        if (load_file->segments[segment_index].name &&
            strcmp(load_file->segments[segment_index].name, segment_name) == 0) {
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            if (out_segment) {
                *out_segment = &load_file->segments[segment_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_load_file_find_segment_covering_virtual_address(const MachineLoadFile *load_file,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineLoadSegment **out_segment) {
    size_t segment_index;

    if (!machine_load_file_has_segment_table(load_file)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < load_file->segment_count; ++segment_index) {
        const MachineLoadSegment *segment = &load_file->segments[segment_index];

        if (!machine_load_segment_covers_virtual_address(segment, virtual_address)) {
            continue;
        }
        if (out_segment_index) {
            *out_segment_index = segment_index;
        }
        if (out_segment) {
            *out_segment = segment;
        }
        return 1;
    }
    return 0;
}

int machine_load_file_get_memory_byte_at_virtual_address(const MachineLoadFile *load_file,
    size_t virtual_address,
    unsigned char *out_byte) {
    size_t segment_index = 0u;
    const MachineLoadSegment *segment = NULL;

    if (!load_file || !out_byte ||
        !machine_load_file_find_segment_covering_virtual_address(
            load_file, virtual_address, &segment_index, &segment) ||
        !segment) {
        return 0;
    }
    *out_byte = segment->bytes[virtual_address - segment->virtual_address];
    return 1;
}

int machine_load_file_copy_flat_memory_image(const MachineLoadFile *load_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineLoadError *error) {
    MachineLoadMemorySummary memory_summary;
    unsigned char *bytes = NULL;
    size_t segment_index;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0u;
    }
    if (out_base_virtual_address) {
        *out_base_virtual_address = 0u;
    }
    if (!load_file || !out_bytes || !out_byte_count || !out_base_virtual_address) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-137: invalid flat-memory copy contract");
        return 0;
    }
    if (!machine_load_verify_file(load_file, error) ||
        !machine_load_file_get_memory_summary(load_file, &memory_summary)) {
        return 0;
    }
    bytes = memory_summary.memory_byte_count > 0u
        ? (unsigned char *)calloc(memory_summary.memory_byte_count, 1u)
        : NULL;
    if (memory_summary.memory_byte_count > 0u && !bytes) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-138: out of memory copying flat memory image");
        return 0;
    }
    for (segment_index = 0u; segment_index < load_file->segment_count; ++segment_index) {
        const MachineLoadSegment *segment = &load_file->segments[segment_index];
        size_t offset = segment->virtual_address - memory_summary.base_virtual_address;

        if (segment->memory_byte_count > 0u) {
            memcpy(bytes + offset, segment->bytes, segment->memory_byte_count);
        }
    }
    *out_bytes = bytes;
    *out_byte_count = memory_summary.memory_byte_count;
    *out_base_virtual_address = memory_summary.base_virtual_address;
    return 1;
}

int machine_load_file_get_entry_segment(const MachineLoadFile *load_file,
    size_t *out_segment_index,
    const MachineLoadSegment **out_segment) {
    size_t segment_index = 0u;
    const MachineLoadSegment *segment = NULL;

    if (!machine_load_file_has_segment_table(load_file)) {
        return 0;
    }
    if (!machine_load_file_find_segment_covering_virtual_address(
            load_file,
            load_file->entry_virtual_address,
            &segment_index,
            &segment) ||
        !segment ||
        !segment->executable) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    if (out_segment) {
        *out_segment = segment;
    }
    return 1;
}

int machine_load_file_get_executable_segment_count(const MachineLoadFile *load_file,
    size_t *out_count) {
    size_t count = 0u;
    size_t segment_index;

    if (!machine_load_file_has_segment_table(load_file) || !out_count) {
        return 0;
    }
    for (segment_index = 0u; segment_index < load_file->segment_count; ++segment_index) {
        if (load_file->segments[segment_index].executable) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_load_file_get_executable_segment_by_index(const MachineLoadFile *load_file,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineLoadSegment **out_segment) {
    size_t segment_index;
    size_t current_index = 0u;

    if (!machine_load_file_has_segment_table(load_file)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < load_file->segment_count; ++segment_index) {
        if (!load_file->segments[segment_index].executable) {
            continue;
        }
        if (current_index == executable_index) {
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            if (out_segment) {
                *out_segment = &load_file->segments[segment_index];
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_load_segment_get_summary(const MachineLoadSegment *segment,
    MachineLoadSegmentSummary *out_summary) {
    if (!segment || !out_summary) {
        return 0;
    }
    out_summary->name = segment->name;
    out_summary->exec_segment_index = segment->exec_segment_index;
    out_summary->virtual_address = segment->virtual_address;
    out_summary->file_byte_count = segment->file_byte_count;
    out_summary->memory_byte_count = segment->memory_byte_count;
    out_summary->readable = segment->readable;
    out_summary->writable = segment->writable;
    out_summary->executable = segment->executable;
    return 1;
}


#endif
