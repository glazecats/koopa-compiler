#if defined(__INTELLISENSE__) && !defined(MACHINE_LAUNCH_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAUNCH_SPLIT_AGGREGATOR)
#define MACHINE_LAUNCH_SPLIT_INTELLISENSE_CONTEXT
#include "machine_launch.c"

#else

int machine_launch_file_get_summary(const MachineLaunchFile *launch_file,
    size_t *out_register_count,
    size_t *out_runtime_segment_count,
    size_t *out_mapped_byte_count) {
    if (!launch_file) {
        return 0;
    }
    if (out_register_count) {
        *out_register_count = launch_file->register_count;
    }
    if (out_runtime_segment_count) {
        *out_runtime_segment_count = launch_file->runtime_file.segment_count;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = launch_file->runtime_file.total_mapped_byte_count;
    }
    return 1;
}

int machine_launch_file_get_source_elf_artifact_summary(const MachineLaunchFile *launch_file,
    MachineElfArtifactSummary *out_summary) {
    if (!launch_file || !out_summary) {
        return 0;
    }
    *out_summary = launch_file->runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

int machine_launch_file_get_header_summary(const MachineLaunchFile *launch_file,
    MachineLaunchHeaderSummary *out_summary) {
    if (!launch_file || !out_summary) {
        return 0;
    }
    out_summary->target_profile = launch_file->runtime_file.load_file.exec_file.image_file.target_profile;
    out_summary->program_counter = launch_file->program_counter;
    out_summary->stack_pointer = launch_file->stack_pointer;
    out_summary->register_count = launch_file->register_count;
    out_summary->runtime_segment_count = launch_file->runtime_file.segment_count;
    out_summary->mapped_byte_count = launch_file->runtime_file.total_mapped_byte_count;
    return 1;
}

int machine_launch_file_get_runtime_launch_summary(const MachineLaunchFile *launch_file,
    MachineRuntimeLaunchSummary *out_summary) {
    if (!launch_file || !out_summary) {
        return 0;
    }
    return machine_runtime_file_get_launch_summary(&launch_file->runtime_file, out_summary);
}

int machine_launch_file_get_initial_stack_summary(const MachineLaunchFile *launch_file,
    MachineRuntimeInitialStackSummary *out_summary) {
    if (!launch_file || !out_summary) {
        return 0;
    }
    return machine_runtime_file_get_initial_stack_summary(&launch_file->runtime_file, out_summary);
}

int machine_launch_file_get_runtime_memory_summary(const MachineLaunchFile *launch_file,
    MachineRuntimeMemorySummary *out_summary) {
    if (!launch_file || !out_summary) {
        return 0;
    }
    return machine_runtime_file_get_memory_summary(&launch_file->runtime_file, out_summary);
}

int machine_launch_file_get_register(const MachineLaunchFile *launch_file,
    size_t register_index,
    const MachineLaunchRegister **out_register) {
    if (!launch_file || register_index >= launch_file->register_count) {
        return 0;
    }
    if (out_register) {
        *out_register = &launch_file->registers[register_index];
    }
    return 1;
}

int machine_launch_file_find_register_by_name(const MachineLaunchFile *launch_file,
    const char *register_name,
    size_t *out_register_index,
    const MachineLaunchRegister **out_register) {
    size_t register_index;

    if (!launch_file || !register_name) {
        return 0;
    }
    for (register_index = 0u; register_index < launch_file->register_count; ++register_index) {
        if (launch_file->registers[register_index].name &&
            strcmp(launch_file->registers[register_index].name, register_name) == 0) {
            if (out_register_index) {
                *out_register_index = register_index;
            }
            if (out_register) {
                *out_register = &launch_file->registers[register_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_launch_file_get_program_counter_register(const MachineLaunchFile *launch_file,
    size_t *out_register_index,
    const MachineLaunchRegister **out_register) {
    if (!launch_file ||
        !machine_launch_file_get_register(
            launch_file,
            launch_file->program_counter_register_index,
            out_register)) {
        return 0;
    }
    if (out_register_index) {
        *out_register_index = launch_file->program_counter_register_index;
    }
    return 1;
}

int machine_launch_file_get_stack_pointer_register(const MachineLaunchFile *launch_file,
    size_t *out_register_index,
    const MachineLaunchRegister **out_register) {
    if (!launch_file ||
        !machine_launch_file_get_register(
            launch_file,
            launch_file->stack_pointer_register_index,
            out_register)) {
        return 0;
    }
    if (out_register_index) {
        *out_register_index = launch_file->stack_pointer_register_index;
    }
    return 1;
}

int machine_launch_register_get_summary(const MachineLaunchRegister *reg,
    MachineLaunchRegisterSummary *out_summary) {
    if (!reg || !out_summary) {
        return 0;
    }
    out_summary->name = reg->name;
    out_summary->value = reg->value;
    return 1;
}


#endif
