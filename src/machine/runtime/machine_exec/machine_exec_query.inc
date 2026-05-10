#if defined(__INTELLISENSE__) && !defined(MACHINE_EXEC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_EXEC_SPLIT_AGGREGATOR)
#define MACHINE_EXEC_SPLIT_INTELLISENSE_CONTEXT
#include "machine_exec.c"

#else

static int machine_exec_file_has_segment_table(const MachineExecFile *exec_file) {
    return exec_file && (exec_file->segment_count == 0u || exec_file->segments);
}

int machine_exec_file_get_summary(const MachineExecFile *exec_file,
    size_t *out_segment_count,
    size_t *out_byte_count,
    size_t *out_executable_segment_count) {
    size_t segment_index;
    size_t executable_segment_count = 0u;

    if (!machine_exec_file_has_segment_table(exec_file)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < exec_file->segment_count; ++segment_index) {
        if (exec_file->segments[segment_index].executable) {
            ++executable_segment_count;
        }
    }
    if (out_segment_count) {
        *out_segment_count = exec_file->segment_count;
    }
    if (out_byte_count) {
        *out_byte_count = exec_file->image_file.byte_count;
    }
    if (out_executable_segment_count) {
        *out_executable_segment_count = executable_segment_count;
    }
    return 1;
}

int machine_exec_file_get_header_summary(const MachineExecFile *exec_file,
    MachineExecHeaderSummary *out_summary) {
    if (!exec_file || !out_summary) {
        return 0;
    }
    out_summary->target_profile = exec_file->image_file.target_profile;
    out_summary->base_virtual_address = exec_file->image_file.base_virtual_address;
    out_summary->entry_virtual_address = exec_file->entry_virtual_address;
    out_summary->segment_count = exec_file->segment_count;
    out_summary->byte_count = exec_file->image_file.byte_count;
    return 1;
}

int machine_exec_file_get_segment(const MachineExecFile *exec_file,
    size_t segment_index,
    const MachineExecSegment **out_segment) {
    if (!machine_exec_file_has_segment_table(exec_file) ||
        !out_segment ||
        segment_index >= exec_file->segment_count) {
        return 0;
    }
    *out_segment = &exec_file->segments[segment_index];
    return 1;
}

int machine_exec_file_find_segment_by_name(const MachineExecFile *exec_file,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineExecSegment **out_segment) {
    size_t segment_index;

    if (!machine_exec_file_has_segment_table(exec_file) || !segment_name) {
        return 0;
    }
    for (segment_index = 0u; segment_index < exec_file->segment_count; ++segment_index) {
        if (exec_file->segments[segment_index].name &&
            strcmp(exec_file->segments[segment_index].name, segment_name) == 0) {
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            if (out_segment) {
                *out_segment = &exec_file->segments[segment_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_exec_file_find_segment_covering_virtual_address(const MachineExecFile *exec_file,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineExecSegment **out_segment) {
    size_t segment_index;

    if (!machine_exec_file_has_segment_table(exec_file)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < exec_file->segment_count; ++segment_index) {
        const MachineExecSegment *segment = &exec_file->segments[segment_index];

        if (!machine_exec_segment_covers_virtual_address(segment, virtual_address)) {
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

int machine_exec_file_get_entry_segment(const MachineExecFile *exec_file,
    size_t *out_segment_index,
    const MachineExecSegment **out_segment) {
    size_t segment_index = 0u;
    const MachineExecSegment *segment = NULL;

    if (!machine_exec_file_has_segment_table(exec_file)) {
        return 0;
    }
    if (!machine_exec_file_find_segment_covering_virtual_address(
            exec_file,
            exec_file->entry_virtual_address,
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

int machine_exec_file_get_executable_segment_count(const MachineExecFile *exec_file,
    size_t *out_count) {
    size_t segment_index;
    size_t count = 0u;

    if (!machine_exec_file_has_segment_table(exec_file) || !out_count) {
        return 0;
    }
    for (segment_index = 0u; segment_index < exec_file->segment_count; ++segment_index) {
        if (exec_file->segments[segment_index].executable) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_exec_file_get_executable_segment_by_index(const MachineExecFile *exec_file,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineExecSegment **out_segment) {
    size_t segment_index;
    size_t current_index = 0u;

    if (!machine_exec_file_has_segment_table(exec_file)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < exec_file->segment_count; ++segment_index) {
        if (!exec_file->segments[segment_index].executable) {
            continue;
        }
        if (current_index == executable_index) {
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            if (out_segment) {
                *out_segment = &exec_file->segments[segment_index];
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_exec_segment_get_summary(const MachineExecSegment *segment,
    MachineExecSegmentSummary *out_summary) {
    if (!segment || !out_summary) {
        return 0;
    }
    out_summary->name = segment->name;
    out_summary->image_segment_index = segment->image_segment_index;
    out_summary->virtual_address = segment->virtual_address;
    out_summary->byte_count = segment->byte_count;
    out_summary->readable = segment->readable;
    out_summary->writable = segment->writable;
    out_summary->executable = segment->executable;
    return 1;
}

#endif
