#if defined(__INTELLISENSE__) && !defined(MACHINE_RUNTIME_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_RUNTIME_SPLIT_AGGREGATOR)
#define MACHINE_RUNTIME_SPLIT_INTELLISENSE_CONTEXT
#include "machine_runtime.c"

#else

static int machine_runtime_file_has_segment_table(const MachineRuntimeFile *runtime_file) {
    return runtime_file && (runtime_file->segment_count == 0u || runtime_file->segments);
}

int machine_runtime_file_get_summary(const MachineRuntimeFile *runtime_file,
    size_t *out_segment_count,
    size_t *out_mapped_byte_count,
    size_t *out_executable_segment_count) {
    size_t executable_segment_count = 0u;
    size_t segment_index;

    if (!machine_runtime_file_has_segment_table(runtime_file)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < runtime_file->segment_count; ++segment_index) {
        if (runtime_file->segments[segment_index].executable) {
            ++executable_segment_count;
        }
    }
    if (out_segment_count) {
        *out_segment_count = runtime_file->segment_count;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = runtime_file->total_mapped_byte_count;
    }
    if (out_executable_segment_count) {
        *out_executable_segment_count = executable_segment_count;
    }
    return 1;
}

int machine_runtime_file_get_source_elf_artifact_summary(const MachineRuntimeFile *runtime_file,
    MachineElfArtifactSummary *out_summary) {
    if (!runtime_file || !out_summary) {
        return 0;
    }
    *out_summary = runtime_file->load_file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

int machine_runtime_file_get_header_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeHeaderSummary *out_summary) {
    size_t base_virtual_address = 0u;

    if (!runtime_file || !out_summary || !machine_runtime_file_compute_bounds(runtime_file, &base_virtual_address, NULL)) {
        return 0;
    }
    out_summary->target_profile = runtime_file->load_file.exec_file.image_file.target_profile;
    out_summary->base_virtual_address = base_virtual_address;
    out_summary->entry_virtual_address = runtime_file->entry_virtual_address;
    out_summary->initial_stack_pointer = runtime_file->initial_stack_pointer;
    out_summary->segment_count = runtime_file->segment_count;
    out_summary->mapped_byte_count = runtime_file->total_mapped_byte_count;
    return 1;
}

int machine_runtime_file_get_memory_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeMemorySummary *out_summary) {
    size_t base_virtual_address = 0u;
    size_t end_virtual_address = 0u;

    if (!runtime_file || !out_summary ||
        !machine_runtime_file_compute_bounds(runtime_file, &base_virtual_address, &end_virtual_address)) {
        return 0;
    }
    out_summary->base_virtual_address = base_virtual_address;
    out_summary->end_virtual_address = end_virtual_address;
    out_summary->span_byte_count = end_virtual_address - base_virtual_address;
    out_summary->mapped_byte_count = runtime_file->total_mapped_byte_count;
    out_summary->entry_offset = runtime_file->entry_virtual_address - base_virtual_address;
    out_summary->initial_stack_pointer_offset = runtime_file->initial_stack_pointer - base_virtual_address;
    return 1;
}

int machine_runtime_file_get_stack_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeStackSummary *out_summary) {
    const MachineRuntimeSegment *stack_segment = NULL;
    size_t stack_segment_index = 0u;

    if (!runtime_file || !out_summary ||
        !machine_runtime_file_get_stack_segment(runtime_file, &stack_segment_index, &stack_segment) ||
        !stack_segment) {
        return 0;
    }
    if (!machine_runtime_try_add_size(
            stack_segment->virtual_address,
            stack_segment->byte_count,
            &out_summary->end_virtual_address)) {
        return 0;
    }
    out_summary->stack_segment_index = stack_segment_index;
    out_summary->base_virtual_address = stack_segment->virtual_address;
    out_summary->byte_count = stack_segment->byte_count;
    out_summary->initial_stack_pointer = runtime_file->initial_stack_pointer;
    out_summary->initial_stack_pointer_offset =
        runtime_file->initial_stack_pointer - stack_segment->virtual_address;
    return 1;
}

int machine_runtime_file_get_gap_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeGapSummary *out_summary) {
    MachineRuntimeStackSummary stack_summary;
    size_t highest_non_stack_end = 0u;
    size_t segment_index;
    int found_non_stack = 0;

    memset(&stack_summary, 0, sizeof(stack_summary));
    if (!machine_runtime_file_has_segment_table(runtime_file) ||
        !out_summary ||
        !machine_runtime_file_get_stack_summary(runtime_file, &stack_summary)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < runtime_file->segment_count; ++segment_index) {
        const MachineRuntimeSegment *segment = &runtime_file->segments[segment_index];
        size_t segment_end;

        if (segment->kind == MACHINE_RUNTIME_SEGMENT_KIND_STACK) {
            continue;
        }
        if (!machine_runtime_try_add_size(segment->virtual_address, segment->byte_count, &segment_end)) {
            return 0;
        }
        if (!found_non_stack || segment_end > highest_non_stack_end) {
            highest_non_stack_end = segment_end;
            found_non_stack = 1;
        }
    }
    if (!found_non_stack || highest_non_stack_end > stack_summary.base_virtual_address) {
        return 0;
    }
    out_summary->base_virtual_address = highest_non_stack_end;
    out_summary->end_virtual_address = stack_summary.base_virtual_address;
    out_summary->byte_count = out_summary->end_virtual_address - out_summary->base_virtual_address;
    return 1;
}

int machine_runtime_file_get_launch_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeLaunchSummary *out_summary) {
    MachineRuntimeStackSummary stack_summary;

    memset(&stack_summary, 0, sizeof(stack_summary));
    if (!runtime_file || !out_summary ||
        !machine_runtime_file_get_stack_summary(runtime_file, &stack_summary)) {
        return 0;
    }
    out_summary->entry_virtual_address = runtime_file->entry_virtual_address;
    out_summary->initial_stack_pointer = runtime_file->initial_stack_pointer;
    out_summary->stack_segment_index = stack_summary.stack_segment_index;
    out_summary->stack_base_virtual_address = stack_summary.base_virtual_address;
    out_summary->stack_end_virtual_address = stack_summary.end_virtual_address;
    out_summary->stack_byte_count = stack_summary.byte_count;
    return 1;
}

int machine_runtime_file_get_initial_stack_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeInitialStackSummary *out_summary) {
    MachineRuntimeLaunchSummary launch_summary;
    size_t word_byte_count;
    size_t image_word_count;
    size_t image_byte_count;
    size_t image_base_virtual_address;
    MachineElfTargetProfile profile;

    memset(&launch_summary, 0, sizeof(launch_summary));
    if (!runtime_file || !out_summary ||
        !machine_runtime_file_get_launch_summary(runtime_file, &launch_summary)) {
        return 0;
    }
    profile = runtime_file->load_file.exec_file.image_file.target_profile;
    word_byte_count = machine_runtime_target_word_byte_count(profile);
    image_word_count = 5u;
    image_byte_count = image_word_count * word_byte_count;
    if (image_byte_count > launch_summary.stack_byte_count ||
        launch_summary.initial_stack_pointer < image_byte_count) {
        return 0;
    }
    image_base_virtual_address = launch_summary.initial_stack_pointer - image_byte_count;
    out_summary->word_byte_count = word_byte_count;
    out_summary->image_base_virtual_address = image_base_virtual_address;
    out_summary->image_end_virtual_address = launch_summary.initial_stack_pointer;
    out_summary->image_byte_count = image_byte_count;
    out_summary->argc = 0u;
    out_summary->argc_virtual_address = image_base_virtual_address;
    out_summary->argv_terminator_virtual_address = image_base_virtual_address + word_byte_count;
    out_summary->envp_terminator_virtual_address = image_base_virtual_address + (2u * word_byte_count);
    out_summary->auxv_terminator_virtual_address = image_base_virtual_address + (3u * word_byte_count);
    return 1;
}

int machine_runtime_file_get_segment(const MachineRuntimeFile *runtime_file,
    size_t segment_index,
    const MachineRuntimeSegment **out_segment) {
    if (!machine_runtime_file_has_segment_table(runtime_file) ||
        segment_index >= runtime_file->segment_count) {
        return 0;
    }
    if (out_segment) {
        *out_segment = &runtime_file->segments[segment_index];
    }
    return 1;
}

int machine_runtime_file_find_segment_by_name(const MachineRuntimeFile *runtime_file,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment) {
    size_t segment_index;

    if (!machine_runtime_file_has_segment_table(runtime_file) || !segment_name) {
        return 0;
    }
    for (segment_index = 0u; segment_index < runtime_file->segment_count; ++segment_index) {
        if (runtime_file->segments[segment_index].name &&
            strcmp(runtime_file->segments[segment_index].name, segment_name) == 0) {
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            if (out_segment) {
                *out_segment = &runtime_file->segments[segment_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_runtime_file_find_segment_covering_virtual_address(const MachineRuntimeFile *runtime_file,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment) {
    size_t segment_index;

    if (!machine_runtime_file_has_segment_table(runtime_file)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < runtime_file->segment_count; ++segment_index) {
        if (machine_runtime_segment_covers_virtual_address(&runtime_file->segments[segment_index], virtual_address)) {
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            if (out_segment) {
                *out_segment = &runtime_file->segments[segment_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_runtime_file_find_segment_covering_memory_offset(const MachineRuntimeFile *runtime_file,
    size_t memory_offset,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment) {
    MachineRuntimeMemorySummary memory_summary;
    size_t virtual_address;

    memset(&memory_summary, 0, sizeof(memory_summary));
    if (!runtime_file || !machine_runtime_file_get_memory_summary(runtime_file, &memory_summary) ||
        memory_offset >= memory_summary.span_byte_count) {
        return 0;
    }
    virtual_address = memory_summary.base_virtual_address + memory_offset;
    return machine_runtime_file_find_segment_covering_virtual_address(
        runtime_file, virtual_address, out_segment_index, out_segment);
}

int machine_runtime_file_get_memory_byte_at_virtual_address(const MachineRuntimeFile *runtime_file,
    size_t virtual_address,
    unsigned char *out_byte) {
    const MachineRuntimeSegment *segment = NULL;
    size_t segment_index = 0u;
    size_t offset = 0u;

    if (!runtime_file || !out_byte ||
        !machine_runtime_file_find_segment_covering_virtual_address(runtime_file, virtual_address, &segment_index, &segment) ||
        !segment) {
        return 0;
    }
    (void)segment_index;
    offset = virtual_address - segment->virtual_address;
    *out_byte = segment->bytes[offset];
    return 1;
}

int machine_runtime_file_get_memory_byte_at_offset(const MachineRuntimeFile *runtime_file,
    size_t memory_offset,
    unsigned char *out_byte) {
    MachineRuntimeMemorySummary memory_summary;

    memset(&memory_summary, 0, sizeof(memory_summary));
    if (!runtime_file || !out_byte ||
        !machine_runtime_file_get_memory_summary(runtime_file, &memory_summary) ||
        memory_offset >= memory_summary.span_byte_count) {
        return 0;
    }
    return machine_runtime_file_get_memory_byte_at_virtual_address(
        runtime_file, memory_summary.base_virtual_address + memory_offset, out_byte);
}

int machine_runtime_file_copy_flat_memory_image(const MachineRuntimeFile *runtime_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error) {
    size_t base_virtual_address = 0u;
    size_t end_virtual_address = 0u;
    size_t span_byte_count;
    size_t segment_index;
    unsigned char *bytes = NULL;

    if (!runtime_file || !out_bytes || !out_byte_count ||
        !machine_runtime_file_compute_bounds(runtime_file, &base_virtual_address, &end_virtual_address)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-120: invalid flat-image copy contract");
        return 0;
    }
    span_byte_count = end_virtual_address - base_virtual_address;
    bytes = span_byte_count > 0u ? (unsigned char *)calloc(span_byte_count, 1u) : NULL;
    if (span_byte_count > 0u && !bytes) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-121: out of memory copying flat runtime image");
        return 0;
    }
    for (segment_index = 0u; segment_index < runtime_file->segment_count; ++segment_index) {
        const MachineRuntimeSegment *segment = &runtime_file->segments[segment_index];
        size_t offset = segment->virtual_address - base_virtual_address;

        if (segment->byte_count > 0u) {
            memcpy(bytes + offset, segment->bytes, segment->byte_count);
        }
    }

    *out_bytes = bytes;
    *out_byte_count = span_byte_count;
    if (out_base_virtual_address) {
        *out_base_virtual_address = base_virtual_address;
    }
    return 1;
}

int machine_runtime_file_copy_segment_bytes(const MachineRuntimeFile *runtime_file,
    size_t segment_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineRuntimeError *error) {
    const MachineRuntimeSegment *segment = NULL;
    unsigned char *bytes = NULL;

    if (!runtime_file || !out_bytes || !out_byte_count ||
        !machine_runtime_file_get_segment(runtime_file, segment_index, &segment) || !segment) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-121: invalid segment-byte copy contract");
        return 0;
    }
    bytes = segment->byte_count > 0u ? (unsigned char *)malloc(segment->byte_count) : NULL;
    if (segment->byte_count > 0u && !bytes) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-122: out of memory copying runtime segment bytes");
        return 0;
    }
    if (segment->byte_count > 0u) {
        memcpy(bytes, segment->bytes, segment->byte_count);
    }
    *out_bytes = bytes;
    *out_byte_count = segment->byte_count;
    return 1;
}

int machine_runtime_file_copy_memory_window(const MachineRuntimeFile *runtime_file,
    size_t memory_offset,
    size_t requested_byte_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error) {
    MachineRuntimeMemorySummary memory_summary;
    unsigned char *bytes = NULL;
    size_t base_virtual_address;
    size_t actual_byte_count;
    size_t index;

    memset(&memory_summary, 0, sizeof(memory_summary));
    if (!runtime_file || !out_bytes || !out_byte_count ||
        !machine_runtime_file_get_memory_summary(runtime_file, &memory_summary) ||
        memory_offset > memory_summary.span_byte_count) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-124: invalid memory-window copy contract");
        return 0;
    }
    actual_byte_count = memory_summary.span_byte_count - memory_offset;
    if (requested_byte_count < actual_byte_count) {
        actual_byte_count = requested_byte_count;
    }
    bytes = actual_byte_count > 0u ? (unsigned char *)calloc(actual_byte_count, 1u) : NULL;
    if (actual_byte_count > 0u && !bytes) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-125: out of memory copying runtime memory window");
        return 0;
    }

    base_virtual_address = memory_summary.base_virtual_address + memory_offset;
    for (index = 0u; index < actual_byte_count; ++index) {
        if (!machine_runtime_file_get_memory_byte_at_virtual_address(runtime_file, base_virtual_address + index, &bytes[index])) {
            free(bytes);
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-126: failed reading runtime memory window byte");
            return 0;
        }
    }

    *out_bytes = bytes;
    *out_byte_count = actual_byte_count;
    if (out_base_virtual_address) {
        *out_base_virtual_address = base_virtual_address;
    }
    return 1;
}

int machine_runtime_file_get_stack_byte_at_offset(const MachineRuntimeFile *runtime_file,
    size_t stack_offset,
    unsigned char *out_byte) {
    MachineRuntimeStackSummary stack_summary;

    memset(&stack_summary, 0, sizeof(stack_summary));
    if (!runtime_file || !out_byte ||
        !machine_runtime_file_get_stack_summary(runtime_file, &stack_summary) ||
        stack_offset >= stack_summary.byte_count) {
        return 0;
    }
    return machine_runtime_file_get_memory_byte_at_virtual_address(
        runtime_file,
        stack_summary.base_virtual_address + stack_offset,
        out_byte);
}

int machine_runtime_file_copy_stack_bytes(const MachineRuntimeFile *runtime_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineRuntimeError *error) {
    MachineRuntimeStackSummary stack_summary;

    memset(&stack_summary, 0, sizeof(stack_summary));
    if (!runtime_file || !machine_runtime_file_get_stack_summary(runtime_file, &stack_summary)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-123: invalid stack-byte copy contract");
        return 0;
    }
    return machine_runtime_file_copy_segment_bytes(
        runtime_file, stack_summary.stack_segment_index, out_bytes, out_byte_count, error);
}

int machine_runtime_file_copy_stack_window(const MachineRuntimeFile *runtime_file,
    size_t stack_offset,
    size_t requested_byte_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error) {
    MachineRuntimeStackSummary stack_summary;
    unsigned char *bytes = NULL;
    size_t actual_byte_count;
    size_t base_virtual_address;
    size_t index;

    memset(&stack_summary, 0, sizeof(stack_summary));
    if (!runtime_file || !out_bytes || !out_byte_count ||
        !machine_runtime_file_get_stack_summary(runtime_file, &stack_summary) ||
        stack_offset > stack_summary.byte_count) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-127: invalid stack-window copy contract");
        return 0;
    }
    actual_byte_count = stack_summary.byte_count - stack_offset;
    if (requested_byte_count < actual_byte_count) {
        actual_byte_count = requested_byte_count;
    }
    bytes = actual_byte_count > 0u ? (unsigned char *)calloc(actual_byte_count, 1u) : NULL;
    if (actual_byte_count > 0u && !bytes) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-128: out of memory copying runtime stack window");
        return 0;
    }

    base_virtual_address = stack_summary.base_virtual_address + stack_offset;
    for (index = 0u; index < actual_byte_count; ++index) {
        if (!machine_runtime_file_get_stack_byte_at_offset(runtime_file, stack_offset + index, &bytes[index])) {
            free(bytes);
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-129: failed reading runtime stack window byte");
            return 0;
        }
    }

    *out_bytes = bytes;
    *out_byte_count = actual_byte_count;
    if (out_base_virtual_address) {
        *out_base_virtual_address = base_virtual_address;
    }
    return 1;
}

int machine_runtime_file_copy_initial_stack_window(const MachineRuntimeFile *runtime_file,
    size_t requested_byte_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error) {
    MachineRuntimeStackSummary stack_summary;
    size_t actual_byte_count;
    size_t stack_offset;

    memset(&stack_summary, 0, sizeof(stack_summary));
    if (!runtime_file || !machine_runtime_file_get_stack_summary(runtime_file, &stack_summary)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-130: invalid initial-stack-window copy contract");
        return 0;
    }
    actual_byte_count = requested_byte_count;
    if (actual_byte_count > stack_summary.byte_count) {
        actual_byte_count = stack_summary.byte_count;
    }
    stack_offset = stack_summary.byte_count - actual_byte_count;
    return machine_runtime_file_copy_stack_window(
        runtime_file, stack_offset, actual_byte_count, out_bytes, out_byte_count, out_base_virtual_address, error);
}

int machine_runtime_file_copy_initial_stack_image(const MachineRuntimeFile *runtime_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error) {
    MachineRuntimeInitialStackSummary initial_stack_summary;

    memset(&initial_stack_summary, 0, sizeof(initial_stack_summary));
    if (!runtime_file || !machine_runtime_file_get_initial_stack_summary(runtime_file, &initial_stack_summary)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-131: invalid initial-stack-image copy contract");
        return 0;
    }
    return machine_runtime_file_copy_initial_stack_window(
        runtime_file,
        initial_stack_summary.image_byte_count,
        out_bytes,
        out_byte_count,
        out_base_virtual_address,
        error);
}

int machine_runtime_file_copy_gap_window(const MachineRuntimeFile *runtime_file,
    size_t gap_offset,
    size_t requested_byte_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error) {
    MachineRuntimeGapSummary gap_summary;
    size_t actual_byte_count;
    unsigned char *bytes = NULL;

    memset(&gap_summary, 0, sizeof(gap_summary));
    if (!runtime_file || !out_bytes || !out_byte_count ||
        !machine_runtime_file_get_gap_summary(runtime_file, &gap_summary) ||
        gap_offset > gap_summary.byte_count) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-131: invalid gap-window copy contract");
        return 0;
    }
    actual_byte_count = gap_summary.byte_count - gap_offset;
    if (requested_byte_count < actual_byte_count) {
        actual_byte_count = requested_byte_count;
    }
    bytes = actual_byte_count > 0u ? (unsigned char *)calloc(actual_byte_count, 1u) : NULL;
    if (actual_byte_count > 0u && !bytes) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-132: out of memory copying runtime gap window");
        return 0;
    }
    *out_bytes = bytes;
    *out_byte_count = actual_byte_count;
    if (out_base_virtual_address) {
        *out_base_virtual_address = gap_summary.base_virtual_address + gap_offset;
    }
    return 1;
}

int machine_runtime_file_get_entry_segment(const MachineRuntimeFile *runtime_file,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment) {
    const MachineRuntimeSegment *segment = NULL;
    size_t segment_index = 0u;

    if (!runtime_file ||
        !machine_runtime_file_find_segment_covering_virtual_address(
            runtime_file, runtime_file->entry_virtual_address, &segment_index, &segment) ||
        !segment || !segment->executable) {
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

int machine_runtime_file_get_stack_segment(const MachineRuntimeFile *runtime_file,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment) {
    if (!machine_runtime_file_has_segment_table(runtime_file) ||
        !runtime_file->has_stack_segment ||
        runtime_file->stack_segment_index >= runtime_file->segment_count ||
        runtime_file->segments[runtime_file->stack_segment_index].kind != MACHINE_RUNTIME_SEGMENT_KIND_STACK) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = runtime_file->stack_segment_index;
    }
    if (out_segment) {
        *out_segment = &runtime_file->segments[runtime_file->stack_segment_index];
    }
    return 1;
}

int machine_runtime_file_get_executable_segment_count(const MachineRuntimeFile *runtime_file,
    size_t *out_count) {
    size_t count = 0u;
    size_t segment_index;

    if (!machine_runtime_file_has_segment_table(runtime_file) || !out_count) {
        return 0;
    }
    for (segment_index = 0u; segment_index < runtime_file->segment_count; ++segment_index) {
        if (runtime_file->segments[segment_index].executable) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_runtime_file_get_executable_segment_by_index(const MachineRuntimeFile *runtime_file,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment) {
    size_t segment_index;
    size_t current_index = 0u;

    if (!machine_runtime_file_has_segment_table(runtime_file)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < runtime_file->segment_count; ++segment_index) {
        if (!runtime_file->segments[segment_index].executable) {
            continue;
        }
        if (current_index == executable_index) {
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            if (out_segment) {
                *out_segment = &runtime_file->segments[segment_index];
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_runtime_file_get_region_count(const MachineRuntimeFile *runtime_file,
    size_t *out_count) {
    MachineRuntimeGapSummary gap_summary;

    memset(&gap_summary, 0, sizeof(gap_summary));
    if (!runtime_file || !out_count) {
        return 0;
    }
    *out_count = runtime_file->segment_count;
    if (machine_runtime_file_get_gap_summary(runtime_file, &gap_summary)) {
        *out_count += 1u;
    }
    return 1;
}

int machine_runtime_file_get_region_summary(const MachineRuntimeFile *runtime_file,
    size_t region_index,
    MachineRuntimeRegionSummary *out_summary) {
    MachineRuntimeGapSummary gap_summary;

    memset(&gap_summary, 0, sizeof(gap_summary));
    if (!machine_runtime_file_has_segment_table(runtime_file) || !out_summary) {
        return 0;
    }
    if (region_index < runtime_file->segment_count) {
        const MachineRuntimeSegment *segment = &runtime_file->segments[region_index];
        size_t segment_end = 0u;

        if (!machine_runtime_try_add_size(segment->virtual_address, segment->byte_count, &segment_end)) {
            return 0;
        }

        out_summary->kind =
            segment->kind == MACHINE_RUNTIME_SEGMENT_KIND_STACK ? MACHINE_RUNTIME_REGION_STACK : MACHINE_RUNTIME_REGION_LOAD;
        out_summary->region_index = region_index;
        out_summary->segment_index = region_index;
        out_summary->name = segment->name;
        out_summary->base_virtual_address = segment->virtual_address;
        out_summary->end_virtual_address = segment_end;
        out_summary->byte_count = segment->byte_count;
        out_summary->readable = segment->readable;
        out_summary->writable = segment->writable;
        out_summary->executable = segment->executable;
        return 1;
    }
    if (region_index != runtime_file->segment_count ||
        !machine_runtime_file_get_gap_summary(runtime_file, &gap_summary)) {
        return 0;
    }
    out_summary->kind = MACHINE_RUNTIME_REGION_GAP;
    out_summary->region_index = region_index;
    out_summary->segment_index = (size_t)-1;
    out_summary->name = ".gap";
    out_summary->base_virtual_address = gap_summary.base_virtual_address;
    out_summary->end_virtual_address = gap_summary.end_virtual_address;
    out_summary->byte_count = gap_summary.byte_count;
    out_summary->readable = 0u;
    out_summary->writable = 0u;
    out_summary->executable = 0u;
    return 1;
}

int machine_runtime_file_find_region_covering_virtual_address(const MachineRuntimeFile *runtime_file,
    size_t virtual_address,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary) {
    size_t region_count = 0u;
    size_t region_index;
    MachineRuntimeRegionSummary summary;

    memset(&summary, 0, sizeof(summary));
    if (!runtime_file || !machine_runtime_file_get_region_count(runtime_file, &region_count)) {
        return 0;
    }
    for (region_index = 0u; region_index < region_count; ++region_index) {
        if (!machine_runtime_file_get_region_summary(runtime_file, region_index, &summary)) {
            return 0;
        }
        if (machine_runtime_region_summary_covers_virtual_address(&summary, virtual_address)) {
            if (out_region_index) {
                *out_region_index = region_index;
            }
            if (out_summary) {
                *out_summary = summary;
            }
            return 1;
        }
    }
    return 0;
}

int machine_runtime_file_find_region_covering_memory_offset(const MachineRuntimeFile *runtime_file,
    size_t memory_offset,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary) {
    MachineRuntimeMemorySummary memory_summary;

    memset(&memory_summary, 0, sizeof(memory_summary));
    if (!runtime_file || !machine_runtime_file_get_memory_summary(runtime_file, &memory_summary) ||
        memory_offset >= memory_summary.span_byte_count) {
        return 0;
    }
    return machine_runtime_file_find_region_covering_virtual_address(
        runtime_file,
        memory_summary.base_virtual_address + memory_offset,
        out_region_index,
        out_summary);
}

int machine_runtime_segment_get_summary(const MachineRuntimeSegment *segment,
    MachineRuntimeSegmentSummary *out_summary) {
    if (!segment || !out_summary) {
        return 0;
    }
    out_summary->name = segment->name;
    out_summary->kind = segment->kind;
    out_summary->load_segment_index = segment->load_segment_index;
    out_summary->virtual_address = segment->virtual_address;
    out_summary->byte_count = segment->byte_count;
    out_summary->readable = segment->readable;
    out_summary->writable = segment->writable;
    out_summary->executable = segment->executable;
    return 1;
}


#endif
