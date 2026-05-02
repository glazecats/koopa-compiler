#if defined(__INTELLISENSE__) && !defined(MACHINE_LAUNCH_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAUNCH_SPLIT_AGGREGATOR)
#define MACHINE_LAUNCH_SPLIT_INTELLISENSE_CONTEXT
#include "machine_launch.c"

#else

int machine_launch_build_from_machine_runtime_file(const MachineRuntimeFile *runtime_file,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error) {
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineLaunchTargetPolicySummary target_policy_summary;

    memset(&runtime_launch_summary, 0, sizeof(runtime_launch_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    if (!runtime_file || !out_launch_file) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-116: invalid launch build contract");
        return 0;
    }
    if (!machine_runtime_verify_file(runtime_file, NULL)) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-117: invalid runtime input");
        return 0;
    }
    if (!machine_runtime_file_get_launch_summary(runtime_file, &runtime_launch_summary) ||
        !machine_launch_get_target_policy_summary(
            runtime_file->load_file.exec_file.image_file.target_profile,
            &target_policy_summary)) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-118: failed deriving launch policy");
        return 0;
    }

    machine_launch_file_free(out_launch_file);
    if (!machine_runtime_clone_file(runtime_file, &out_launch_file->runtime_file, NULL)) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-119: failed cloning runtime input");
        machine_launch_file_free(out_launch_file);
        return 0;
    }

    out_launch_file->program_counter = runtime_launch_summary.entry_virtual_address;
    out_launch_file->stack_pointer = runtime_launch_summary.initial_stack_pointer;
    out_launch_file->register_count = 2u;
    out_launch_file->register_capacity = 2u;
    out_launch_file->program_counter_register_index = 0u;
    out_launch_file->stack_pointer_register_index = 1u;
    out_launch_file->registers = (MachineLaunchRegister *)calloc(2u, sizeof(MachineLaunchRegister));
    if (!out_launch_file->registers) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-120: out of memory building launch registers");
        machine_launch_file_free(out_launch_file);
        return 0;
    }

    out_launch_file->registers[0].name =
        machine_launch_strdup(target_policy_summary.program_counter_register_name);
    out_launch_file->registers[0].value = out_launch_file->program_counter;
    out_launch_file->registers[1].name =
        machine_launch_strdup(target_policy_summary.stack_pointer_register_name);
    out_launch_file->registers[1].value = out_launch_file->stack_pointer;
    if (!out_launch_file->registers[0].name || !out_launch_file->registers[1].name) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-121: out of memory naming launch registers");
        machine_launch_file_free(out_launch_file);
        return 0;
    }

    if (!machine_launch_verify_file(out_launch_file, error)) {
        machine_launch_file_free(out_launch_file);
        return 0;
    }
    return 1;
}

int machine_launch_build_from_machine_runtime_report(const MachineRuntimeReport *report,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error) {
    const MachineRuntimeFile *runtime_file = NULL;

    if (!report) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-122: invalid launch build-from-runtime-report contract");
        return 0;
    }
    if (!machine_runtime_report_get_file(report, &runtime_file) || !runtime_file) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-123: failed recovering runtime file from report");
        return 0;
    }
    return machine_launch_build_from_machine_runtime_file(runtime_file, out_launch_file, error);
}

int machine_launch_build_from_machine_load_file(const MachineLoadFile *load_file,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_load_file(load_file, &runtime_file, NULL) &&
        machine_launch_build_from_machine_runtime_file(&runtime_file, out_launch_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-124: failed building launch from load file");
    }
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_launch_build_from_machine_load_report(const MachineLoadReport *report,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_load_report(report, &runtime_file, NULL) &&
        machine_launch_build_from_machine_runtime_file(&runtime_file, out_launch_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-125: failed building launch from load report");
    }
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_launch_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_ir_report(report, &runtime_file, NULL) &&
        machine_launch_build_from_machine_runtime_file(&runtime_file, out_launch_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-126: failed building launch from machine-ir report");
    }
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_launch_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_ir_report_with_profile(report, profile, &runtime_file, NULL) &&
        machine_launch_build_from_machine_runtime_file(&runtime_file, out_launch_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-127: failed building profiled launch from machine-ir report");
    }
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_launch_build_report_from_file(const MachineLaunchFile *source,
    MachineLaunchReport *out_report,
    MachineLaunchError *error) {
    size_t register_index;

    if (!source || !out_report) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-128: invalid launch report build contract");
        return 0;
    }
    if (!machine_launch_verify_file(source, error)) {
        return 0;
    }

    machine_launch_report_free(out_report);
    if (!machine_launch_clone_file(source, &out_report->file, error)) {
        machine_launch_report_free(out_report);
        return 0;
    }

    out_report->register_summary_count = source->register_count;
    out_report->register_summaries = source->register_count > 0u
        ? (MachineLaunchRegisterSummary *)calloc(source->register_count, sizeof(MachineLaunchRegisterSummary))
        : NULL;
    if (source->register_count > 0u && !out_report->register_summaries) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-129: out of memory building launch report");
        machine_launch_report_free(out_report);
        return 0;
    }

    out_report->program_counter_register_index = source->program_counter_register_index;
    out_report->stack_pointer_register_index = source->stack_pointer_register_index;
    for (register_index = 0u; register_index < source->register_count; ++register_index) {
        if (!machine_launch_register_get_summary(
                &out_report->file.registers[register_index],
                &out_report->register_summaries[register_index])) {
            machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-130: failed summarizing launch register");
            machine_launch_report_free(out_report);
            return 0;
        }
    }
    if (!machine_launch_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_launch_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_launch_file_get_runtime_launch_summary(&out_report->file, &out_report->runtime_launch_summary) ||
        !machine_launch_file_get_initial_stack_summary(&out_report->file, &out_report->initial_stack_summary) ||
        !machine_launch_file_get_runtime_memory_summary(&out_report->file, &out_report->runtime_memory_summary)) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-131: failed summarizing launch report");
        machine_launch_report_free(out_report);
        return 0;
    }
    return 1;
}

int machine_launch_build_report_from_machine_runtime_file(const MachineRuntimeFile *source,
    MachineLaunchReport *out_report,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_runtime_file(source, &launch_file, error) &&
        machine_launch_build_report_from_file(&launch_file, out_report, error);
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_launch_build_report_from_machine_runtime_report(const MachineRuntimeReport *report,
    MachineLaunchReport *out_report,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_runtime_report(report, &launch_file, error) &&
        machine_launch_build_report_from_file(&launch_file, out_report, error);
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_launch_build_report_from_machine_load_file(const MachineLoadFile *source,
    MachineLaunchReport *out_report,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_load_file(source, &launch_file, error) &&
        machine_launch_build_report_from_file(&launch_file, out_report, error);
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_launch_build_report_from_machine_load_report(const MachineLoadReport *report,
    MachineLaunchReport *out_report,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_load_report(report, &launch_file, error) &&
        machine_launch_build_report_from_file(&launch_file, out_report, error);
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_launch_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineLaunchReport *out_report,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_ir_report(report, &launch_file, error) &&
        machine_launch_build_report_from_file(&launch_file, out_report, error);
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_launch_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLaunchReport *out_report,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_ir_report_with_profile(report, profile, &launch_file, error) &&
        machine_launch_build_report_from_file(&launch_file, out_report, error);
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_launch_report_refresh(MachineLaunchReport *report,
    MachineLaunchError *error) {
    MachineLaunchReport refreshed_report;
    int ok;

    if (!report) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-132: invalid launch report refresh contract");
        return 0;
    }
    machine_launch_report_init(&refreshed_report);
    ok = machine_launch_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_launch_report_free(report);
        *report = refreshed_report;
    } else {
        machine_launch_report_free(&refreshed_report);
    }
    return ok;
}


#endif
