#if defined(__INTELLISENSE__) && !defined(MACHINE_LAUNCH_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAUNCH_SPLIT_AGGREGATOR)
#define MACHINE_LAUNCH_SPLIT_INTELLISENSE_CONTEXT
#include "machine_launch.c"

#else

int machine_launch_report_get_summary(const MachineLaunchReport *report,
    size_t *out_register_count,
    size_t *out_runtime_segment_count,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_register_count) {
        *out_register_count = report->register_summary_count;
    }
    if (out_runtime_segment_count) {
        *out_runtime_segment_count = report->header_summary.runtime_segment_count;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_launch_report_get_overview_artifact(const MachineLaunchReport *report,
    MachineLaunchReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact ||
        (report->register_summary_count > 0u && !report->register_summaries) ||
        report->program_counter_register_index >= report->register_summary_count ||
        report->stack_pointer_register_index >= report->register_summary_count) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->runtime_launch_summary = &report->runtime_launch_summary;
    out_artifact->initial_stack_summary = &report->initial_stack_summary;
    out_artifact->runtime_memory_summary = &report->runtime_memory_summary;
    out_artifact->program_counter_register_index = report->program_counter_register_index;
    out_artifact->program_counter_register_summary =
        &report->register_summaries[report->program_counter_register_index];
    out_artifact->stack_pointer_register_index = report->stack_pointer_register_index;
    out_artifact->stack_pointer_register_summary =
        &report->register_summaries[report->stack_pointer_register_index];
    return 1;
}

int machine_launch_report_get_file(const MachineLaunchReport *report,
    const MachineLaunchFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_launch_report_get_runtime_file(const MachineLaunchReport *report,
    const MachineRuntimeFile **out_runtime_file) {
    if (!report || !out_runtime_file) {
        return 0;
    }
    *out_runtime_file = &report->file.runtime_file;
    return 1;
}

int machine_launch_report_get_source_elf_artifact_summary_artifact(const MachineLaunchReport *report,
    const MachineElfArtifactSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

int machine_launch_report_get_header_summary_artifact(const MachineLaunchReport *report,
    const MachineLaunchHeaderSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->header_summary;
    return 1;
}

int machine_launch_report_get_target_policy_summary_artifact(const MachineLaunchReport *report,
    const MachineLaunchTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_launch_report_get_runtime_launch_summary_artifact(const MachineLaunchReport *report,
    const MachineRuntimeLaunchSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->runtime_launch_summary;
    return 1;
}

int machine_launch_report_get_initial_stack_summary_artifact(const MachineLaunchReport *report,
    const MachineRuntimeInitialStackSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->initial_stack_summary;
    return 1;
}

int machine_launch_report_get_runtime_memory_summary_artifact(const MachineLaunchReport *report,
    const MachineRuntimeMemorySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->runtime_memory_summary;
    return 1;
}

int machine_launch_report_get_register_summary(const MachineLaunchReport *report,
    size_t register_index,
    const MachineLaunchRegisterSummary **out_summary) {
    if (!report || register_index >= report->register_summary_count ||
        (report->register_summary_count > 0u && !report->register_summaries)) {
        return 0;
    }
    if (out_summary) {
        *out_summary = &report->register_summaries[register_index];
    }
    return 1;
}

int machine_launch_report_get_register_artifact(const MachineLaunchReport *report,
    size_t register_index,
    MachineLaunchReportRegisterArtifact *out_artifact) {
    if (!report || !out_artifact ||
        !machine_launch_report_get_register_summary(report, register_index, &out_artifact->register_summary)) {
        return 0;
    }
    out_artifact->report = report;
    out_artifact->register_index = register_index;
    out_artifact->is_program_counter_register =
        register_index == report->program_counter_register_index;
    out_artifact->is_stack_pointer_register =
        register_index == report->stack_pointer_register_index;
    return 1;
}

int machine_launch_report_find_register_summary_by_name(const MachineLaunchReport *report,
    const char *register_name,
    size_t *out_register_index,
    const MachineLaunchRegisterSummary **out_summary) {
    size_t register_index;

    if (!report || !register_name ||
        (report->register_summary_count > 0u && !report->register_summaries)) {
        return 0;
    }
    for (register_index = 0u; register_index < report->register_summary_count; ++register_index) {
        if (report->register_summaries[register_index].name &&
            strcmp(report->register_summaries[register_index].name, register_name) == 0) {
            if (out_register_index) {
                *out_register_index = register_index;
            }
            if (out_summary) {
                *out_summary = &report->register_summaries[register_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_launch_report_find_register_artifact_by_name(const MachineLaunchReport *report,
    const char *register_name,
    size_t *out_register_index,
    MachineLaunchReportRegisterArtifact *out_artifact) {
    size_t register_index = 0u;

    if (!machine_launch_report_find_register_summary_by_name(report, register_name, &register_index, NULL)) {
        return 0;
    }
    if (out_register_index) {
        *out_register_index = register_index;
    }
    return !out_artifact || machine_launch_report_get_register_artifact(report, register_index, out_artifact);
}

int machine_launch_report_get_program_counter_register_summary_artifact(const MachineLaunchReport *report,
    size_t *out_register_index,
    const MachineLaunchRegisterSummary **out_summary) {
    if (!report ||
        !machine_launch_report_get_register_summary(report, report->program_counter_register_index, out_summary)) {
        return 0;
    }
    if (out_register_index) {
        *out_register_index = report->program_counter_register_index;
    }
    return 1;
}

int machine_launch_report_get_stack_pointer_register_summary_artifact(const MachineLaunchReport *report,
    size_t *out_register_index,
    const MachineLaunchRegisterSummary **out_summary) {
    if (!report ||
        !machine_launch_report_get_register_summary(report, report->stack_pointer_register_index, out_summary)) {
        return 0;
    }
    if (out_register_index) {
        *out_register_index = report->stack_pointer_register_index;
    }
    return 1;
}

int machine_launch_report_get_program_counter_register_artifact(const MachineLaunchReport *report,
    MachineLaunchReportRegisterArtifact *out_artifact) {
    return machine_launch_report_get_register_artifact(
        report,
        report ? report->program_counter_register_index : 0u,
        out_artifact);
}

int machine_launch_report_get_stack_pointer_register_artifact(const MachineLaunchReport *report,
    MachineLaunchReportRegisterArtifact *out_artifact) {
    return machine_launch_report_get_register_artifact(
        report,
        report ? report->stack_pointer_register_index : 0u,
        out_artifact);
}

int machine_launch_report_overview_artifact_get_program_counter_register_summary_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    const MachineLaunchRegisterSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->program_counter_register_summary;
    return 1;
}

int machine_launch_report_overview_artifact_get_stack_pointer_register_summary_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    const MachineLaunchRegisterSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->stack_pointer_register_summary;
    return 1;
}

int machine_launch_report_overview_artifact_get_runtime_launch_summary_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->runtime_launch_summary;
    return 1;
}

int machine_launch_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->initial_stack_summary;
    return 1;
}

int machine_launch_report_overview_artifact_get_runtime_memory_summary_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    const MachineRuntimeMemorySummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->runtime_memory_summary;
    return 1;
}

int machine_launch_report_overview_artifact_get_program_counter_register_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    MachineLaunchReportRegisterArtifact *out_artifact) {
    if (!artifact || !artifact->report || !out_artifact) {
        return 0;
    }
    return machine_launch_report_get_program_counter_register_artifact(artifact->report, out_artifact);
}

int machine_launch_report_overview_artifact_get_stack_pointer_register_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    MachineLaunchReportRegisterArtifact *out_artifact) {
    if (!artifact || !artifact->report || !out_artifact) {
        return 0;
    }
    return machine_launch_report_get_stack_pointer_register_artifact(artifact->report, out_artifact);
}

int machine_launch_dump_report(const MachineLaunchReport *report,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchStringBuilder builder;
    MachineLaunchReportOverviewArtifact overview_artifact;
    size_t register_index;

    memset(&builder, 0, sizeof(builder));
    memset(&overview_artifact, 0, sizeof(overview_artifact));
    if (!report || !out_text ||
        !machine_launch_report_get_overview_artifact(report, &overview_artifact)) {
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-136: invalid launch report dump contract");
        return 0;
    }

    if (!machine_launch_append_format(
            &builder,
            "machine_launch profile=%s elf_origin=%s elf_semantics=%s pc=0x%zx sp=0x%zx registers=%zu runtime_segments=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            report->header_summary.program_counter,
            report->header_summary.stack_pointer,
            report->header_summary.register_count,
            report->header_summary.runtime_segment_count,
            report->header_summary.mapped_byte_count) ||
        !machine_launch_append_format(
            &builder,
            "program_counter_register: lreg.%zu %s\n",
            report->program_counter_register_index,
            report->register_summaries[report->program_counter_register_index].name) ||
        !machine_launch_append_format(
            &builder,
            "stack_pointer_register: lreg.%zu %s\n",
            report->stack_pointer_register_index,
            report->register_summaries[report->stack_pointer_register_index].name) ||
        !machine_launch_append_format(&builder, "registers:\n")) {
        free(builder.data);
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-137: out of memory building launch report dump");
        return 0;
    }

    for (register_index = 0u; register_index < report->register_summary_count; ++register_index) {
        if (!machine_launch_append_format(
                &builder,
                "  lreg.%zu %s value=0x%zx\n",
                register_index,
                report->register_summaries[register_index].name,
                report->register_summaries[register_index].value)) {
            free(builder.data);
            machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-138: out of memory appending launch report register");
            return 0;
        }
    }

    if (!machine_launch_append_format(&builder, "report_overview:\n") ||
        !machine_launch_append_format(
            &builder,
            "  registers=%zu runtime-segments=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx pc-register=%zu sp-register=%zu\n",
            report->header_summary.register_count,
            report->header_summary.runtime_segment_count,
            report->header_summary.mapped_byte_count,
            report->header_summary.program_counter,
            report->header_summary.stack_pointer,
            report->program_counter_register_index,
            report->stack_pointer_register_index) ||
        !machine_launch_append_format(
            &builder,
            "  elf_source: target=%s origin=%s semantics=%s\n",
            machine_elf_target_profile_name(
                report->file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics)) ||
        !machine_launch_append_format(
            &builder,
            "  policy: profile=%s pc-reg=%s sp-reg=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.program_counter_register_name,
            report->target_policy_summary.stack_pointer_register_name) ||
        !machine_launch_append_format(
            &builder,
            "  runtime-launch: pc=0x%zx sp=0x%zx stack-segment=%zu stack-base=0x%zx stack-end=0x%zx stack-bytes=%zu\n",
            report->runtime_launch_summary.entry_virtual_address,
            report->runtime_launch_summary.initial_stack_pointer,
            report->runtime_launch_summary.stack_segment_index,
            report->runtime_launch_summary.stack_base_virtual_address,
            report->runtime_launch_summary.stack_end_virtual_address,
            report->runtime_launch_summary.stack_byte_count) ||
        !machine_launch_append_format(
            &builder,
            "  initial-stack: base=0x%zx end=0x%zx bytes=%zu word=%zu argc=%zu\n",
            report->initial_stack_summary.image_base_virtual_address,
            report->initial_stack_summary.image_end_virtual_address,
            report->initial_stack_summary.image_byte_count,
            report->initial_stack_summary.word_byte_count,
            report->initial_stack_summary.argc) ||
        !machine_launch_append_format(
            &builder,
            "  runtime-memory: base=0x%zx end=0x%zx span-bytes=%zu mapped-bytes=%zu entry-offset=0x%zx sp-offset=0x%zx\n",
            report->runtime_memory_summary.base_virtual_address,
            report->runtime_memory_summary.end_virtual_address,
            report->runtime_memory_summary.span_byte_count,
            report->runtime_memory_summary.mapped_byte_count,
            report->runtime_memory_summary.entry_offset,
            report->runtime_memory_summary.initial_stack_pointer_offset) ||
        !machine_launch_append_format(&builder, "report_registers:\n")) {
        free(builder.data);
        machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-139: out of memory appending launch report overview");
        return 0;
    }

    for (register_index = 0u; register_index < report->register_summary_count; ++register_index) {
        MachineLaunchReportRegisterArtifact register_artifact;

        memset(&register_artifact, 0, sizeof(register_artifact));
        if (!machine_launch_report_get_register_artifact(report, register_index, &register_artifact) ||
            !machine_launch_append_format(
                &builder,
                "  lreg.%zu %s pc=%s sp=%s value=0x%zx\n",
                register_index,
                register_artifact.register_summary ? register_artifact.register_summary->name : "",
                register_artifact.is_program_counter_register ? "yes" : "no",
                register_artifact.is_stack_pointer_register ? "yes" : "no",
                register_artifact.register_summary ? register_artifact.register_summary->value : 0u)) {
            free(builder.data);
            machine_launch_set_error(error, 0, 0, "MACHINE-LAUNCH-140: failed appending launch report register");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int machine_launch_build_report_dump_from_file(const MachineLaunchFile *source,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchReport report;
    int ok;

    machine_launch_report_init(&report);
    ok = machine_launch_build_report_from_file(source, &report, error) &&
        machine_launch_dump_report(&report, out_text, error);
    machine_launch_report_free(&report);
    return ok;
}

int machine_launch_build_report_dump_from_machine_runtime_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchReport report;
    int ok;

    machine_launch_report_init(&report);
    ok = machine_launch_build_report_from_machine_runtime_file(source, &report, error) &&
        machine_launch_dump_report(&report, out_text, error);
    machine_launch_report_free(&report);
    return ok;
}

int machine_launch_build_report_dump_from_machine_runtime_report(const MachineRuntimeReport *report,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchReport launch_report;
    int ok;

    machine_launch_report_init(&launch_report);
    ok = machine_launch_build_report_from_machine_runtime_report(report, &launch_report, error) &&
        machine_launch_dump_report(&launch_report, out_text, error);
    machine_launch_report_free(&launch_report);
    return ok;
}

int machine_launch_build_report_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchReport report;
    int ok;

    machine_launch_report_init(&report);
    ok = machine_launch_build_report_from_machine_load_file(source, &report, error) &&
        machine_launch_dump_report(&report, out_text, error);
    machine_launch_report_free(&report);
    return ok;
}

int machine_launch_build_report_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchReport launch_report;
    int ok;

    machine_launch_report_init(&launch_report);
    ok = machine_launch_build_report_from_machine_load_report(report, &launch_report, error) &&
        machine_launch_dump_report(&launch_report, out_text, error);
    machine_launch_report_free(&launch_report);
    return ok;
}

int machine_launch_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchReport launch_report;
    int ok;

    machine_launch_report_init(&launch_report);
    ok = machine_launch_build_report_from_machine_ir_report(report, &launch_report, error) &&
        machine_launch_dump_report(&launch_report, out_text, error);
    machine_launch_report_free(&launch_report);
    return ok;
}

int machine_launch_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLaunchError *error) {
    MachineLaunchReport launch_report;
    int ok;

    machine_launch_report_init(&launch_report);
    ok = machine_launch_build_report_from_machine_ir_report_with_profile(report, profile, &launch_report, error) &&
        machine_launch_dump_report(&launch_report, out_text, error);
    machine_launch_report_free(&launch_report);
    return ok;
}

#endif
