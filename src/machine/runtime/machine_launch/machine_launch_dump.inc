#if defined(__INTELLISENSE__) && !defined(MACHINE_LAUNCH_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAUNCH_SPLIT_AGGREGATOR)
#define MACHINE_LAUNCH_SPLIT_INTELLISENSE_CONTEXT
#include "machine_launch.c"

#else

int machine_launch_dump_file(const MachineLaunchFile *launch_file,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchStringBuilder builder;
    MachineLaunchHeaderSummary header_summary;
    size_t register_index;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    if (!launch_file || !out_text || !machine_launch_verify_file(launch_file, error) ||
        !machine_launch_file_get_header_summary(launch_file, &header_summary)) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-133: invalid launch dump contract");
        return 0;
    }

    if (!machine_launch_append_format(
            &builder,
            "machine_launch profile=%s elf_origin=%s elf_semantics=%s pc=0x%zx sp=0x%zx registers=%zu runtime_segments=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_elf_target_profile_name(
                launch_file->runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                launch_file->runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            header_summary.program_counter,
            header_summary.stack_pointer,
            header_summary.register_count,
            header_summary.runtime_segment_count,
            header_summary.mapped_byte_count) ||
        !machine_launch_append_format(
            &builder,
            "program_counter_register: lreg.%zu %s\n",
            launch_file->program_counter_register_index,
            launch_file->registers[launch_file->program_counter_register_index].name) ||
        !machine_launch_append_format(
            &builder,
            "stack_pointer_register: lreg.%zu %s\n",
            launch_file->stack_pointer_register_index,
            launch_file->registers[launch_file->stack_pointer_register_index].name) ||
        !machine_launch_append_format(&builder, "registers:\n")) {
        free(builder.data);
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-134: out of memory building launch dump");
        return 0;
    }

    for (register_index = 0u; register_index < launch_file->register_count; ++register_index) {
        if (!machine_launch_append_format(
                &builder,
                "  lreg.%zu %s value=0x%zx\n",
                register_index,
                launch_file->registers[register_index].name,
                launch_file->registers[register_index].value)) {
            free(builder.data);
            machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-135: out of memory appending launch register");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int machine_launch_build_dump_from_file(const MachineLaunchFile *source,
    char **out_text,
    MachineLaunchError *error) {
    return machine_launch_dump_file(source, out_text, error);
}

int machine_launch_build_dump_from_machine_runtime_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_runtime_file(source, &launch_file, error) &&
        machine_launch_dump_file(&launch_file, out_text, error);
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_launch_build_dump_from_machine_runtime_report(const MachineRuntimeReport *report,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_runtime_report(report, &launch_file, error) &&
        machine_launch_dump_file(&launch_file, out_text, error);
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_launch_build_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_load_file(source, &launch_file, error) &&
        machine_launch_dump_file(&launch_file, out_text, error);
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_launch_build_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_load_report(report, &launch_file, error) &&
        machine_launch_dump_file(&launch_file, out_text, error);
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_launch_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_ir_report(report, &launch_file, error) &&
        machine_launch_dump_file(&launch_file, out_text, error);
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_launch_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_ir_report_with_profile(report, profile, &launch_file, error) &&
        machine_launch_dump_file(&launch_file, out_text, error);
    machine_launch_file_free(&launch_file);
    return ok;
}


#endif
