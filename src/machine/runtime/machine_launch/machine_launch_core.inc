#if defined(__INTELLISENSE__) && !defined(MACHINE_LAUNCH_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAUNCH_SPLIT_AGGREGATOR)
#define MACHINE_LAUNCH_SPLIT_INTELLISENSE_CONTEXT
#include "machine_launch.c"

#else

static void machine_launch_set_error(MachineLaunchError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (!error || !fmt) {
        return;
    }
    error->line = line;
    error->column = column;
    va_start(args, fmt);
    (void)vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

static char *machine_launch_strdup(const char *text) {
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

static int machine_launch_append_format(MachineLaunchStringBuilder *builder, const char *fmt, ...) {
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

static const char *machine_launch_program_counter_register_name(MachineElfTargetProfile profile) {
    switch (profile) {
        case MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW:
            return "eip";
        case MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW:
            return "pc";
        case MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32:
            break;
    }
    return "pc";
}

static const char *machine_launch_stack_pointer_register_name(MachineElfTargetProfile profile) {
    switch (profile) {
        case MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW:
            return "esp";
        case MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW:
            return "sp";
        case MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32:
            break;
    }
    return "sp";
}

void machine_launch_file_init(MachineLaunchFile *launch_file) {
    if (!launch_file) {
        return;
    }
    machine_runtime_file_init(&launch_file->runtime_file);
    launch_file->program_counter = 0u;
    launch_file->stack_pointer = 0u;
    launch_file->registers = NULL;
    launch_file->register_count = 0u;
    launch_file->register_capacity = 0u;
    launch_file->program_counter_register_index = 0u;
    launch_file->stack_pointer_register_index = 0u;
}

void machine_launch_file_free(MachineLaunchFile *launch_file) {
    size_t register_index;

    if (!launch_file) {
        return;
    }
    if (launch_file->registers) {
        for (register_index = 0u; register_index < launch_file->register_count; ++register_index) {
            free(launch_file->registers[register_index].name);
        }
    }
    free(launch_file->registers);
    machine_runtime_file_free(&launch_file->runtime_file);
    machine_launch_file_init(launch_file);
}

void machine_launch_report_init(MachineLaunchReport *report) {
    if (!report) {
        return;
    }
    machine_launch_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    memset(&report->runtime_launch_summary, 0, sizeof(report->runtime_launch_summary));
    memset(&report->initial_stack_summary, 0, sizeof(report->initial_stack_summary));
    memset(&report->runtime_memory_summary, 0, sizeof(report->runtime_memory_summary));
    report->register_summaries = NULL;
    report->register_summary_count = 0u;
    report->program_counter_register_index = 0u;
    report->stack_pointer_register_index = 0u;
}

void machine_launch_report_free(MachineLaunchReport *report) {
    if (!report) {
        return;
    }
    machine_launch_file_free(&report->file);
    free(report->register_summaries);
    machine_launch_report_init(report);
}

int machine_launch_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineLaunchTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->program_counter_register_name =
        machine_launch_program_counter_register_name(profile);
    out_summary->stack_pointer_register_name =
        machine_launch_stack_pointer_register_name(profile);
    return 1;
}

int machine_launch_file_get_target_policy_summary(const MachineLaunchFile *launch_file,
    MachineLaunchTargetPolicySummary *out_summary) {
    if (!launch_file || !out_summary) {
        return 0;
    }
    return machine_launch_get_target_policy_summary(
        launch_file->runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_launch_verify_file(const MachineLaunchFile *launch_file,
    MachineLaunchError *error) {
    const MachineLaunchRegister *pc_reg = NULL;
    const MachineLaunchRegister *sp_reg = NULL;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    size_t register_index;

    memset(&runtime_launch_summary, 0, sizeof(runtime_launch_summary));
    memset(&initial_stack_summary, 0, sizeof(initial_stack_summary));
    if (!launch_file) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-100: invalid launch file");
        return 0;
    }
    if (!machine_runtime_verify_file(&launch_file->runtime_file, NULL)) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-101: invalid runtime input");
        return 0;
    }
    if (launch_file->register_count == 0u || !launch_file->registers) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-102: launch file must materialize registers");
        return 0;
    }
    if (launch_file->register_capacity < launch_file->register_count) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-103: launch register capacity mismatch");
        return 0;
    }
    if (!machine_runtime_file_get_launch_summary(&launch_file->runtime_file, &runtime_launch_summary) ||
        !machine_runtime_file_get_initial_stack_summary(&launch_file->runtime_file, &initial_stack_summary)) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-104: failed deriving runtime launch facts");
        return 0;
    }
    if (launch_file->program_counter != runtime_launch_summary.entry_virtual_address ||
        launch_file->stack_pointer != runtime_launch_summary.initial_stack_pointer) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-105: launch pc/sp must match runtime launch summary");
        return 0;
    }
    if (!(initial_stack_summary.image_base_virtual_address < launch_file->stack_pointer) ||
        initial_stack_summary.image_end_virtual_address != launch_file->stack_pointer) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-106: launch stack pointer must terminate initial stack image");
        return 0;
    }
    if (launch_file->program_counter_register_index >= launch_file->register_count ||
        launch_file->stack_pointer_register_index >= launch_file->register_count) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-107: launch register indices out of range");
        return 0;
    }
    pc_reg = &launch_file->registers[launch_file->program_counter_register_index];
    sp_reg = &launch_file->registers[launch_file->stack_pointer_register_index];
    if (!pc_reg->name || !sp_reg->name) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-108: launch registers must have names");
        return 0;
    }
    if (pc_reg->value != launch_file->program_counter ||
        sp_reg->value != launch_file->stack_pointer) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-109: launch register values must match pc/sp");
        return 0;
    }
    for (register_index = 0u; register_index < launch_file->register_count; ++register_index) {
        const MachineLaunchRegister *reg = &launch_file->registers[register_index];
        size_t other_index;

        if (!reg->name) {
            machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-110: launch register missing name");
            return 0;
        }
        for (other_index = register_index + 1u; other_index < launch_file->register_count; ++other_index) {
            if (strcmp(reg->name, launch_file->registers[other_index].name) == 0) {
                machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-111: duplicate launch register name");
                return 0;
            }
        }
    }
    return 1;
}

int machine_launch_clone_file(const MachineLaunchFile *source,
    MachineLaunchFile *out_clone,
    MachineLaunchError *error) {
    size_t register_index;

    if (!source || !out_clone) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-112: invalid launch clone contract");
        return 0;
    }
    if (!machine_launch_verify_file(source, error)) {
        return 0;
    }

    machine_launch_file_free(out_clone);
    if (!machine_runtime_clone_file(&source->runtime_file, &out_clone->runtime_file, NULL)) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-113: failed cloning runtime input");
        machine_launch_file_free(out_clone);
        return 0;
    }

    out_clone->program_counter = source->program_counter;
    out_clone->stack_pointer = source->stack_pointer;
    out_clone->register_count = source->register_count;
    out_clone->register_capacity = source->register_count;
    out_clone->program_counter_register_index = source->program_counter_register_index;
    out_clone->stack_pointer_register_index = source->stack_pointer_register_index;
    out_clone->registers = source->register_count > 0u
        ? (MachineLaunchRegister *)calloc(source->register_count, sizeof(MachineLaunchRegister))
        : NULL;
    if (source->register_count > 0u && !out_clone->registers) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-114: out of memory cloning launch registers");
        machine_launch_file_free(out_clone);
        return 0;
    }

    for (register_index = 0u; register_index < source->register_count; ++register_index) {
        out_clone->registers[register_index].name =
            machine_launch_strdup(source->registers[register_index].name);
        out_clone->registers[register_index].value = source->registers[register_index].value;
        if (!out_clone->registers[register_index].name) {
            machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-115: out of memory cloning launch register");
            machine_launch_file_free(out_clone);
            return 0;
        }
    }

    if (!machine_launch_verify_file(out_clone, error)) {
        machine_launch_file_free(out_clone);
        return 0;
    }
    return 1;
}


#endif
