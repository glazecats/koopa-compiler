#include "machine/step.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineStepStringBuilder;

static void machine_step_set_error(MachineStepError *error, int line, int column, const char *fmt, ...);
static int machine_step_append_format(MachineStepStringBuilder *builder, const char *fmt, ...);

static void machine_step_set_error(MachineStepError *error, int line, int column, const char *fmt, ...) {
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

static int machine_step_append_format(MachineStepStringBuilder *builder, const char *fmt, ...) {
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

void machine_step_file_init(MachineStepFile *step_file) {
    if (!step_file) {
        return;
    }
    machine_launch_file_init(&step_file->launch_file);
    step_file->status = MACHINE_STEP_STATUS_READY;
    step_file->program_counter = 0u;
    step_file->stack_pointer = 0u;
    step_file->current_segment_index = 0u;
    step_file->current_byte = 0u;
    step_file->current_byte_memory_offset = 0u;
}

void machine_step_file_free(MachineStepFile *step_file) {
    if (!step_file) {
        return;
    }
    machine_launch_file_free(&step_file->launch_file);
    machine_step_file_init(step_file);
}

void machine_step_report_init(MachineStepReport *report) {
    if (!report) {
        return;
    }
    machine_step_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    memset(&report->runtime_launch_summary, 0, sizeof(report->runtime_launch_summary));
    memset(&report->initial_stack_summary, 0, sizeof(report->initial_stack_summary));
    memset(&report->runtime_memory_summary, 0, sizeof(report->runtime_memory_summary));
    memset(&report->current_segment_summary, 0, sizeof(report->current_segment_summary));
    memset(&report->fetch_summary, 0, sizeof(report->fetch_summary));
}

void machine_step_report_free(MachineStepReport *report) {
    if (!report) {
        return;
    }
    machine_step_file_free(&report->file);
    machine_step_report_init(report);
}

const char *machine_step_status_name(MachineStepStatus status) {
    switch (status) {
        case MACHINE_STEP_STATUS_READY:
            return "ready";
        case MACHINE_STEP_STATUS_HALTED:
            return "halted";
    }
    return "unknown";
}

int machine_step_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineStepTargetPolicySummary *out_summary) {
    MachineLaunchTargetPolicySummary launch_policy_summary;

    memset(&launch_policy_summary, 0, sizeof(launch_policy_summary));
    if (!out_summary ||
        !machine_launch_get_target_policy_summary(profile, &launch_policy_summary)) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->program_counter_register_name =
        launch_policy_summary.program_counter_register_name;
    out_summary->stack_pointer_register_name =
        launch_policy_summary.stack_pointer_register_name;
    out_summary->fetch_byte_count = 1u;
    return 1;
}

int machine_step_file_get_target_policy_summary(const MachineStepFile *step_file,
    MachineStepTargetPolicySummary *out_summary) {
    if (!step_file || !out_summary) {
        return 0;
    }
    return machine_step_get_target_policy_summary(
        step_file->launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_step_verify_file(const MachineStepFile *step_file,
    MachineStepError *error) {
    const MachineRuntimeSegment *segment = NULL;
    size_t segment_index = 0u;
    unsigned char current_byte = 0u;
    MachineRuntimeMemorySummary runtime_memory_summary;

    memset(&runtime_memory_summary, 0, sizeof(runtime_memory_summary));
    if (!step_file) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-100: invalid step file");
        return 0;
    }
    if (!machine_launch_verify_file(&step_file->launch_file, NULL)) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-101: invalid launch input");
        return 0;
    }
    if (step_file->program_counter != step_file->launch_file.program_counter ||
        step_file->stack_pointer != step_file->launch_file.stack_pointer) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-102: step pc/sp must match launch file");
        return 0;
    }
    if (step_file->status != MACHINE_STEP_STATUS_READY &&
        step_file->status != MACHINE_STEP_STATUS_HALTED) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-103: invalid step status");
        return 0;
    }
    if (step_file->status == MACHINE_STEP_STATUS_HALTED) {
        return 1;
    }
    if (!machine_runtime_file_find_segment_covering_virtual_address(
            &step_file->launch_file.runtime_file,
            step_file->program_counter,
            &segment_index,
            &segment) ||
        !segment) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-104: current pc must be mapped");
        return 0;
    }
    if (!segment->executable) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-105: current pc must remain inside executable segment");
        return 0;
    }
    if (step_file->current_segment_index != segment_index) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-106: current segment index mismatch");
        return 0;
    }
    if (!machine_runtime_file_get_memory_byte_at_virtual_address(
            &step_file->launch_file.runtime_file,
            step_file->program_counter,
            &current_byte) ||
        current_byte != step_file->current_byte) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-107: current fetch byte mismatch");
        return 0;
    }
    if (!machine_runtime_file_get_memory_summary(&step_file->launch_file.runtime_file, &runtime_memory_summary) ||
        step_file->program_counter < runtime_memory_summary.base_virtual_address ||
        step_file->current_byte_memory_offset !=
            (step_file->program_counter - runtime_memory_summary.base_virtual_address)) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-108: current fetch offset mismatch");
        return 0;
    }
    return 1;
}

int machine_step_clone_file(const MachineStepFile *source,
    MachineStepFile *out_clone,
    MachineStepError *error) {
    if (!source || !out_clone) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-109: invalid step clone contract");
        return 0;
    }
    if (!machine_step_verify_file(source, error)) {
        return 0;
    }

    machine_step_file_free(out_clone);
    if (!machine_launch_clone_file(&source->launch_file, &out_clone->launch_file, NULL)) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-110: failed cloning launch input");
        machine_step_file_free(out_clone);
        return 0;
    }
    out_clone->status = source->status;
    out_clone->program_counter = source->program_counter;
    out_clone->stack_pointer = source->stack_pointer;
    out_clone->current_segment_index = source->current_segment_index;
    out_clone->current_byte = source->current_byte;
    out_clone->current_byte_memory_offset = source->current_byte_memory_offset;
    return 1;
}

int machine_step_file_get_summary(const MachineStepFile *step_file,
    size_t *out_launch_register_count,
    size_t *out_runtime_segment_count,
    size_t *out_mapped_byte_count) {
    if (!step_file) {
        return 0;
    }
    if (out_launch_register_count) {
        *out_launch_register_count = step_file->launch_file.register_count;
    }
    if (out_runtime_segment_count) {
        *out_runtime_segment_count = step_file->launch_file.runtime_file.segment_count;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = step_file->launch_file.runtime_file.total_mapped_byte_count;
    }
    return 1;
}

int machine_step_file_get_header_summary(const MachineStepFile *step_file,
    MachineStepHeaderSummary *out_summary) {
    if (!step_file || !out_summary) {
        return 0;
    }
    out_summary->target_profile =
        step_file->launch_file.runtime_file.load_file.exec_file.image_file.target_profile;
    out_summary->status = step_file->status;
    out_summary->program_counter = step_file->program_counter;
    out_summary->stack_pointer = step_file->stack_pointer;
    out_summary->launch_register_count = step_file->launch_file.register_count;
    out_summary->runtime_segment_count = step_file->launch_file.runtime_file.segment_count;
    out_summary->mapped_byte_count = step_file->launch_file.runtime_file.total_mapped_byte_count;
    out_summary->current_segment_index = step_file->current_segment_index;
    return 1;
}

int machine_step_file_get_runtime_launch_summary(const MachineStepFile *step_file,
    MachineRuntimeLaunchSummary *out_summary) {
    if (!step_file || !out_summary) {
        return 0;
    }
    return machine_launch_file_get_runtime_launch_summary(&step_file->launch_file, out_summary);
}

int machine_step_file_get_initial_stack_summary(const MachineStepFile *step_file,
    MachineRuntimeInitialStackSummary *out_summary) {
    if (!step_file || !out_summary) {
        return 0;
    }
    return machine_launch_file_get_initial_stack_summary(&step_file->launch_file, out_summary);
}

int machine_step_file_get_runtime_memory_summary(const MachineStepFile *step_file,
    MachineRuntimeMemorySummary *out_summary) {
    if (!step_file || !out_summary) {
        return 0;
    }
    return machine_launch_file_get_runtime_memory_summary(&step_file->launch_file, out_summary);
}

int machine_step_file_get_current_segment_summary(const MachineStepFile *step_file,
    size_t *out_segment_index,
    MachineRuntimeSegmentSummary *out_summary) {
    const MachineRuntimeSegment *segment = NULL;

    if (!step_file ||
        !machine_runtime_file_get_segment(
            &step_file->launch_file.runtime_file,
            step_file->current_segment_index,
            &segment) ||
        !segment) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = step_file->current_segment_index;
    }
    return !out_summary || machine_runtime_segment_get_summary(segment, out_summary);
}

int machine_step_file_get_fetch_summary(const MachineStepFile *step_file,
    MachineStepFetchSummary *out_summary) {
    MachineRuntimeSegmentSummary segment_summary;

    memset(&segment_summary, 0, sizeof(segment_summary));
    if (!step_file || !out_summary ||
        !machine_step_file_get_current_segment_summary(step_file, NULL, &segment_summary)) {
        return 0;
    }
    out_summary->byte_virtual_address = step_file->program_counter;
    out_summary->byte_memory_offset = step_file->current_byte_memory_offset;
    out_summary->segment_index = step_file->current_segment_index;
    out_summary->segment_name = segment_summary.name;
    out_summary->byte_value = step_file->current_byte;
    return 1;
}

int machine_step_build_from_machine_launch_file(const MachineLaunchFile *launch_file,
    MachineStepFile *out_step_file,
    MachineStepError *error) {
    MachineRuntimeMemorySummary runtime_memory_summary;
    unsigned char current_byte = 0u;
    size_t current_segment_index = 0u;
    const MachineRuntimeSegment *segment = NULL;

    memset(&runtime_memory_summary, 0, sizeof(runtime_memory_summary));
    if (!launch_file || !out_step_file) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-111: invalid step build contract");
        return 0;
    }
    if (!machine_launch_verify_file(launch_file, NULL)) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-112: invalid launch input");
        return 0;
    }
    if (!machine_runtime_file_find_segment_covering_virtual_address(
            &launch_file->runtime_file,
            launch_file->program_counter,
            &current_segment_index,
            &segment) ||
        !segment ||
        !machine_runtime_file_get_memory_byte_at_virtual_address(
            &launch_file->runtime_file,
            launch_file->program_counter,
            &current_byte) ||
        !machine_runtime_file_get_memory_summary(
            &launch_file->runtime_file,
            &runtime_memory_summary)) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-113: failed deriving current step state");
        return 0;
    }

    machine_step_file_free(out_step_file);
    if (!machine_launch_clone_file(launch_file, &out_step_file->launch_file, NULL)) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-114: failed cloning launch input");
        machine_step_file_free(out_step_file);
        return 0;
    }
    out_step_file->status = MACHINE_STEP_STATUS_READY;
    out_step_file->program_counter = launch_file->program_counter;
    out_step_file->stack_pointer = launch_file->stack_pointer;
    out_step_file->current_segment_index = current_segment_index;
    out_step_file->current_byte = current_byte;
    out_step_file->current_byte_memory_offset =
        out_step_file->program_counter - runtime_memory_summary.base_virtual_address;

    if (!machine_step_verify_file(out_step_file, error)) {
        machine_step_file_free(out_step_file);
        return 0;
    }
    return 1;
}

int machine_step_build_from_machine_launch_report(const MachineLaunchReport *report,
    MachineStepFile *out_step_file,
    MachineStepError *error) {
    const MachineLaunchFile *launch_file = NULL;

    if (!report) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-115: invalid step build-from-launch-report contract");
        return 0;
    }
    if (!machine_launch_report_get_file(report, &launch_file) || !launch_file) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-116: failed recovering launch file from report");
        return 0;
    }
    return machine_step_build_from_machine_launch_file(launch_file, out_step_file, error);
}

int machine_step_build_from_machine_runtime_file(const MachineRuntimeFile *runtime_file,
    MachineStepFile *out_step_file,
    MachineStepError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_runtime_file(runtime_file, &launch_file, NULL) &&
        machine_step_build_from_machine_launch_file(&launch_file, out_step_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-117: failed building step from runtime file");
    }
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_step_build_from_machine_runtime_report(const MachineRuntimeReport *report,
    MachineStepFile *out_step_file,
    MachineStepError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_runtime_report(report, &launch_file, NULL) &&
        machine_step_build_from_machine_launch_file(&launch_file, out_step_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-118: failed building step from runtime report");
    }
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_step_build_from_machine_load_file(const MachineLoadFile *load_file,
    MachineStepFile *out_step_file,
    MachineStepError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_load_file(load_file, &launch_file, NULL) &&
        machine_step_build_from_machine_launch_file(&launch_file, out_step_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-119: failed building step from load file");
    }
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_step_build_from_machine_load_report(const MachineLoadReport *report,
    MachineStepFile *out_step_file,
    MachineStepError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_load_report(report, &launch_file, NULL) &&
        machine_step_build_from_machine_launch_file(&launch_file, out_step_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-120: failed building step from load report");
    }
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_step_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineStepFile *out_step_file,
    MachineStepError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_ir_report(report, &launch_file, NULL) &&
        machine_step_build_from_machine_launch_file(&launch_file, out_step_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-121: failed building step from machine-ir report");
    }
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_step_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineStepFile *out_step_file,
    MachineStepError *error) {
    MachineLaunchFile launch_file;
    int ok;

    machine_launch_file_init(&launch_file);
    ok = machine_launch_build_from_machine_ir_report_with_profile(report, profile, &launch_file, NULL) &&
        machine_step_build_from_machine_launch_file(&launch_file, out_step_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-122: failed building profiled step from machine-ir report");
    }
    machine_launch_file_free(&launch_file);
    return ok;
}

int machine_step_build_report_from_file(const MachineStepFile *source,
    MachineStepReport *out_report,
    MachineStepError *error) {
    if (!source || !out_report) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-123: invalid step report build contract");
        return 0;
    }
    if (!machine_step_verify_file(source, error)) {
        return 0;
    }

    machine_step_report_free(out_report);
    if (!machine_step_clone_file(source, &out_report->file, error)) {
        machine_step_report_free(out_report);
        return 0;
    }
    if (!machine_step_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_step_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_step_file_get_runtime_launch_summary(&out_report->file, &out_report->runtime_launch_summary) ||
        !machine_step_file_get_initial_stack_summary(&out_report->file, &out_report->initial_stack_summary) ||
        !machine_step_file_get_runtime_memory_summary(&out_report->file, &out_report->runtime_memory_summary) ||
        !machine_step_file_get_current_segment_summary(
            &out_report->file, NULL, &out_report->current_segment_summary) ||
        !machine_step_file_get_fetch_summary(&out_report->file, &out_report->fetch_summary)) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-124: failed summarizing step report");
        machine_step_report_free(out_report);
        return 0;
    }
    return 1;
}

int machine_step_build_report_from_machine_launch_file(const MachineLaunchFile *source,
    MachineStepReport *out_report,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_launch_file(source, &step_file, error) &&
        machine_step_build_report_from_file(&step_file, out_report, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_report_from_machine_launch_report(const MachineLaunchReport *report,
    MachineStepReport *out_report,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_launch_report(report, &step_file, error) &&
        machine_step_build_report_from_file(&step_file, out_report, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_report_from_machine_runtime_file(const MachineRuntimeFile *source,
    MachineStepReport *out_report,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_runtime_file(source, &step_file, error) &&
        machine_step_build_report_from_file(&step_file, out_report, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_report_from_machine_runtime_report(const MachineRuntimeReport *report,
    MachineStepReport *out_report,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_runtime_report(report, &step_file, error) &&
        machine_step_build_report_from_file(&step_file, out_report, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_report_from_machine_load_file(const MachineLoadFile *source,
    MachineStepReport *out_report,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_load_file(source, &step_file, error) &&
        machine_step_build_report_from_file(&step_file, out_report, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_report_from_machine_load_report(const MachineLoadReport *report,
    MachineStepReport *out_report,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_load_report(report, &step_file, error) &&
        machine_step_build_report_from_file(&step_file, out_report, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineStepReport *out_report,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_ir_report(report, &step_file, error) &&
        machine_step_build_report_from_file(&step_file, out_report, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineStepReport *out_report,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_ir_report_with_profile(report, profile, &step_file, error) &&
        machine_step_build_report_from_file(&step_file, out_report, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_report_refresh(MachineStepReport *report,
    MachineStepError *error) {
    MachineStepReport refreshed_report;
    int ok;

    if (!report) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-125: invalid step report refresh contract");
        return 0;
    }
    machine_step_report_init(&refreshed_report);
    ok = machine_step_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_step_report_free(report);
        *report = refreshed_report;
    } else {
        machine_step_report_free(&refreshed_report);
    }
    return ok;
}

int machine_step_dump_file(const MachineStepFile *step_file,
    char **out_text,
    MachineStepError *error) {
    MachineStepStringBuilder builder;
    MachineStepHeaderSummary header_summary;
    MachineRuntimeSegmentSummary current_segment_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&current_segment_summary, 0, sizeof(current_segment_summary));
    if (!step_file || !out_text ||
        !machine_step_verify_file(step_file, error) ||
        !machine_step_file_get_header_summary(step_file, &header_summary) ||
        !machine_step_file_get_current_segment_summary(step_file, NULL, &current_segment_summary)) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-126: invalid step dump contract");
        return 0;
    }

    if (!machine_step_append_format(
            &builder,
            "machine_step profile=%s status=%s pc=0x%zx sp=0x%zx launch_registers=%zu runtime_segments=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_step_status_name(header_summary.status),
            header_summary.program_counter,
            header_summary.stack_pointer,
            header_summary.launch_register_count,
            header_summary.runtime_segment_count,
            header_summary.mapped_byte_count) ||
        !machine_step_append_format(
            &builder,
            "current_segment: rseg.%zu %s\n",
            header_summary.current_segment_index,
            current_segment_summary.name ? current_segment_summary.name : "") ||
        !machine_step_append_format(
            &builder,
            "fetch: vaddr=0x%zx mem-offset=0x%zx byte=0x%02x\n",
            step_file->program_counter,
            step_file->current_byte_memory_offset,
            (unsigned int)step_file->current_byte)) {
        free(builder.data);
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-127: out of memory building step dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_step_build_dump_from_file(const MachineStepFile *source,
    char **out_text,
    MachineStepError *error) {
    return machine_step_dump_file(source, out_text, error);
}

int machine_step_build_dump_from_machine_launch_file(const MachineLaunchFile *source,
    char **out_text,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_launch_file(source, &step_file, error) &&
        machine_step_dump_file(&step_file, out_text, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_dump_from_machine_launch_report(const MachineLaunchReport *report,
    char **out_text,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_launch_report(report, &step_file, error) &&
        machine_step_dump_file(&step_file, out_text, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_dump_from_machine_runtime_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_runtime_file(source, &step_file, error) &&
        machine_step_dump_file(&step_file, out_text, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_dump_from_machine_runtime_report(const MachineRuntimeReport *report,
    char **out_text,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_runtime_report(report, &step_file, error) &&
        machine_step_dump_file(&step_file, out_text, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_load_file(source, &step_file, error) &&
        machine_step_dump_file(&step_file, out_text, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_load_report(report, &step_file, error) &&
        machine_step_dump_file(&step_file, out_text, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_ir_report(report, &step_file, error) &&
        machine_step_dump_file(&step_file, out_text, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineStepError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_ir_report_with_profile(report, profile, &step_file, error) &&
        machine_step_dump_file(&step_file, out_text, error);
    machine_step_file_free(&step_file);
    return ok;
}

int machine_step_report_get_summary(const MachineStepReport *report,
    size_t *out_launch_register_count,
    size_t *out_runtime_segment_count,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_launch_register_count) {
        *out_launch_register_count = report->header_summary.launch_register_count;
    }
    if (out_runtime_segment_count) {
        *out_runtime_segment_count = report->header_summary.runtime_segment_count;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_step_report_get_overview_artifact(const MachineStepReport *report,
    MachineStepReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->runtime_launch_summary = &report->runtime_launch_summary;
    out_artifact->initial_stack_summary = &report->initial_stack_summary;
    out_artifact->runtime_memory_summary = &report->runtime_memory_summary;
    out_artifact->current_segment_summary = &report->current_segment_summary;
    out_artifact->fetch_summary = &report->fetch_summary;
    return 1;
}

int machine_step_report_get_file(const MachineStepReport *report,
    const MachineStepFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_step_report_get_launch_file(const MachineStepReport *report,
    const MachineLaunchFile **out_launch_file) {
    if (!report || !out_launch_file) {
        return 0;
    }
    *out_launch_file = &report->file.launch_file;
    return 1;
}

int machine_step_report_get_header_summary_artifact(const MachineStepReport *report,
    const MachineStepHeaderSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->header_summary;
    return 1;
}

int machine_step_report_get_target_policy_summary_artifact(const MachineStepReport *report,
    const MachineStepTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_step_report_get_runtime_launch_summary_artifact(const MachineStepReport *report,
    const MachineRuntimeLaunchSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->runtime_launch_summary;
    return 1;
}

int machine_step_report_get_initial_stack_summary_artifact(const MachineStepReport *report,
    const MachineRuntimeInitialStackSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->initial_stack_summary;
    return 1;
}

int machine_step_report_get_runtime_memory_summary_artifact(const MachineStepReport *report,
    const MachineRuntimeMemorySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->runtime_memory_summary;
    return 1;
}

int machine_step_report_get_current_segment_summary_artifact(const MachineStepReport *report,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = report->header_summary.current_segment_index;
    }
    *out_summary = &report->current_segment_summary;
    return 1;
}

int machine_step_report_get_fetch_summary_artifact(const MachineStepReport *report,
    const MachineStepFetchSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->fetch_summary;
    return 1;
}

int machine_step_report_overview_artifact_get_runtime_launch_summary_artifact(
    const MachineStepReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->runtime_launch_summary;
    return 1;
}

int machine_step_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachineStepReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->initial_stack_summary;
    return 1;
}

int machine_step_report_overview_artifact_get_runtime_memory_summary_artifact(
    const MachineStepReportOverviewArtifact *artifact,
    const MachineRuntimeMemorySummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->runtime_memory_summary;
    return 1;
}

int machine_step_report_overview_artifact_get_current_segment_summary_artifact(
    const MachineStepReportOverviewArtifact *artifact,
    const MachineRuntimeSegmentSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->current_segment_summary;
    return 1;
}

int machine_step_report_overview_artifact_get_fetch_summary_artifact(
    const MachineStepReportOverviewArtifact *artifact,
    const MachineStepFetchSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->fetch_summary;
    return 1;
}

int machine_step_dump_report(const MachineStepReport *report,
    char **out_text,
    MachineStepError *error) {
    MachineStepStringBuilder builder;
    MachineStepReportOverviewArtifact overview_artifact;

    memset(&builder, 0, sizeof(builder));
    memset(&overview_artifact, 0, sizeof(overview_artifact));
    if (!report || !out_text ||
        !machine_step_report_get_overview_artifact(report, &overview_artifact)) {
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-128: invalid step report dump contract");
        return 0;
    }

    if (!machine_step_append_format(
            &builder,
            "machine_step profile=%s status=%s pc=0x%zx sp=0x%zx launch_registers=%zu runtime_segments=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_step_status_name(report->header_summary.status),
            report->header_summary.program_counter,
            report->header_summary.stack_pointer,
            report->header_summary.launch_register_count,
            report->header_summary.runtime_segment_count,
            report->header_summary.mapped_byte_count) ||
        !machine_step_append_format(
            &builder,
            "current_segment: rseg.%zu %s\n",
            report->header_summary.current_segment_index,
            report->current_segment_summary.name ? report->current_segment_summary.name : "") ||
        !machine_step_append_format(
            &builder,
            "fetch: vaddr=0x%zx mem-offset=0x%zx byte=0x%02x\n",
            report->fetch_summary.byte_virtual_address,
            report->fetch_summary.byte_memory_offset,
            (unsigned int)report->fetch_summary.byte_value) ||
        !machine_step_append_format(&builder, "report_overview:\n") ||
        !machine_step_append_format(
            &builder,
            "  status=%s launch-registers=%zu runtime-segments=%zu mapped-bytes=%zu current-segment=%zu pc=0x%zx sp=0x%zx\n",
            machine_step_status_name(report->header_summary.status),
            report->header_summary.launch_register_count,
            report->header_summary.runtime_segment_count,
            report->header_summary.mapped_byte_count,
            report->header_summary.current_segment_index,
            report->header_summary.program_counter,
            report->header_summary.stack_pointer) ||
        !machine_step_append_format(
            &builder,
            "  policy: profile=%s pc-reg=%s sp-reg=%s fetch-bytes=%zu\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.program_counter_register_name,
            report->target_policy_summary.stack_pointer_register_name,
            report->target_policy_summary.fetch_byte_count) ||
        !machine_step_append_format(
            &builder,
            "  runtime-launch: pc=0x%zx sp=0x%zx stack-segment=%zu stack-base=0x%zx stack-end=0x%zx stack-bytes=%zu\n",
            report->runtime_launch_summary.entry_virtual_address,
            report->runtime_launch_summary.initial_stack_pointer,
            report->runtime_launch_summary.stack_segment_index,
            report->runtime_launch_summary.stack_base_virtual_address,
            report->runtime_launch_summary.stack_end_virtual_address,
            report->runtime_launch_summary.stack_byte_count) ||
        !machine_step_append_format(
            &builder,
            "  initial-stack: base=0x%zx end=0x%zx bytes=%zu word=%zu argc=%zu\n",
            report->initial_stack_summary.image_base_virtual_address,
            report->initial_stack_summary.image_end_virtual_address,
            report->initial_stack_summary.image_byte_count,
            report->initial_stack_summary.word_byte_count,
            report->initial_stack_summary.argc) ||
        !machine_step_append_format(
            &builder,
            "  runtime-memory: base=0x%zx end=0x%zx span-bytes=%zu mapped-bytes=%zu entry-offset=0x%zx sp-offset=0x%zx\n",
            report->runtime_memory_summary.base_virtual_address,
            report->runtime_memory_summary.end_virtual_address,
            report->runtime_memory_summary.span_byte_count,
            report->runtime_memory_summary.mapped_byte_count,
            report->runtime_memory_summary.entry_offset,
            report->runtime_memory_summary.initial_stack_pointer_offset) ||
        !machine_step_append_format(
            &builder,
            "  current-segment: rseg.%zu %s perms=%c%c%c\n",
            report->header_summary.current_segment_index,
            report->current_segment_summary.name ? report->current_segment_summary.name : "",
            report->current_segment_summary.readable ? 'r' : '-',
            report->current_segment_summary.writable ? 'w' : '-',
            report->current_segment_summary.executable ? 'x' : '-') ||
        !machine_step_append_format(
            &builder,
            "  fetch: vaddr=0x%zx mem-offset=0x%zx byte=0x%02x segment=%zu %s\n",
            report->fetch_summary.byte_virtual_address,
            report->fetch_summary.byte_memory_offset,
            (unsigned int)report->fetch_summary.byte_value,
            report->fetch_summary.segment_index,
            report->fetch_summary.segment_name ? report->fetch_summary.segment_name : "")) {
        free(builder.data);
        machine_step_set_error(error, 0, 0, "MACHINE-STEP-129: out of memory building step report dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_step_build_report_dump_from_file(const MachineStepFile *source,
    char **out_text,
    MachineStepError *error) {
    MachineStepReport report;
    int ok;

    machine_step_report_init(&report);
    ok = machine_step_build_report_from_file(source, &report, error) &&
        machine_step_dump_report(&report, out_text, error);
    machine_step_report_free(&report);
    return ok;
}

int machine_step_build_report_dump_from_machine_launch_file(const MachineLaunchFile *source,
    char **out_text,
    MachineStepError *error) {
    MachineStepReport report;
    int ok;

    machine_step_report_init(&report);
    ok = machine_step_build_report_from_machine_launch_file(source, &report, error) &&
        machine_step_dump_report(&report, out_text, error);
    machine_step_report_free(&report);
    return ok;
}

int machine_step_build_report_dump_from_machine_launch_report(const MachineLaunchReport *report,
    char **out_text,
    MachineStepError *error) {
    MachineStepReport step_report;
    int ok;

    machine_step_report_init(&step_report);
    ok = machine_step_build_report_from_machine_launch_report(report, &step_report, error) &&
        machine_step_dump_report(&step_report, out_text, error);
    machine_step_report_free(&step_report);
    return ok;
}

int machine_step_build_report_dump_from_machine_runtime_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineStepError *error) {
    MachineStepReport report;
    int ok;

    machine_step_report_init(&report);
    ok = machine_step_build_report_from_machine_runtime_file(source, &report, error) &&
        machine_step_dump_report(&report, out_text, error);
    machine_step_report_free(&report);
    return ok;
}

int machine_step_build_report_dump_from_machine_runtime_report(const MachineRuntimeReport *report,
    char **out_text,
    MachineStepError *error) {
    MachineStepReport step_report;
    int ok;

    machine_step_report_init(&step_report);
    ok = machine_step_build_report_from_machine_runtime_report(report, &step_report, error) &&
        machine_step_dump_report(&step_report, out_text, error);
    machine_step_report_free(&step_report);
    return ok;
}

int machine_step_build_report_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineStepError *error) {
    MachineStepReport report;
    int ok;

    machine_step_report_init(&report);
    ok = machine_step_build_report_from_machine_load_file(source, &report, error) &&
        machine_step_dump_report(&report, out_text, error);
    machine_step_report_free(&report);
    return ok;
}

int machine_step_build_report_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineStepError *error) {
    MachineStepReport step_report;
    int ok;

    machine_step_report_init(&step_report);
    ok = machine_step_build_report_from_machine_load_report(report, &step_report, error) &&
        machine_step_dump_report(&step_report, out_text, error);
    machine_step_report_free(&step_report);
    return ok;
}

int machine_step_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineStepError *error) {
    MachineStepReport step_report;
    int ok;

    machine_step_report_init(&step_report);
    ok = machine_step_build_report_from_machine_ir_report(report, &step_report, error) &&
        machine_step_dump_report(&step_report, out_text, error);
    machine_step_report_free(&step_report);
    return ok;
}

int machine_step_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineStepError *error) {
    MachineStepReport step_report;
    int ok;

    machine_step_report_init(&step_report);
    ok = machine_step_build_report_from_machine_ir_report_with_profile(report, profile, &step_report, error) &&
        machine_step_dump_report(&step_report, out_text, error);
    machine_step_report_free(&step_report);
    return ok;
}
