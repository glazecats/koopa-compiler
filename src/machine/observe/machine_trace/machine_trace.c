#include "machine/trace.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineTraceStringBuilder;

static void machine_trace_set_error(MachineTraceError *error, int line, int column, const char *fmt, ...);
static int machine_trace_append_format(MachineTraceStringBuilder *builder, const char *fmt, ...);
static int machine_trace_append_targets(MachineTraceStringBuilder *builder,
    const MachineTraceSummary *trace_summary);
static int machine_trace_append_return_immediate(MachineTraceStringBuilder *builder,
    const MachineTraceSummary *trace_summary);
static int machine_trace_append_payload_bytes(MachineTraceStringBuilder *builder,
    const MachineTraceSummary *trace_summary);
static int machine_trace_append_immediate_hint(MachineTraceStringBuilder *builder,
    const MachineTraceSummary *trace_summary);
static MachineTraceChangeClass machine_trace_classify_change(const MachineDeltaSummary *delta_summary);

static void machine_trace_set_error(MachineTraceError *error, int line, int column, const char *fmt, ...) {
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

static int machine_trace_append_format(MachineTraceStringBuilder *builder, const char *fmt, ...) {
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

static int machine_trace_append_targets(MachineTraceStringBuilder *builder,
    const MachineTraceSummary *trace_summary) {
    if (!builder || !trace_summary) {
        return 0;
    }
    if (!trace_summary->has_primary_target_block) {
        return machine_trace_append_format(builder, " targets=[]");
    }
    if (!trace_summary->has_secondary_target_block) {
        return machine_trace_append_format(builder, " targets=[%zu]", trace_summary->primary_target_block_index);
    }
    return machine_trace_append_format(
        builder,
        " targets=[%zu,%zu]",
        trace_summary->primary_target_block_index,
        trace_summary->secondary_target_block_index);
}

static int machine_trace_append_return_immediate(MachineTraceStringBuilder *builder,
    const MachineTraceSummary *trace_summary) {
    if (!builder || !trace_summary) {
        return 0;
    }
    if (!trace_summary->has_return_immediate) {
        return machine_trace_append_format(builder, " return-imm=-");
    }
    return machine_trace_append_format(builder, " return-imm=%zu", trace_summary->return_immediate);
}

static int machine_trace_append_payload_bytes(MachineTraceStringBuilder *builder,
    const MachineTraceSummary *trace_summary) {
    size_t index;

    if (!builder || !trace_summary) {
        return 0;
    }
    if (!machine_trace_append_format(builder, " payload=[")) {
        return 0;
    }
    for (index = 0u; index < trace_summary->payload_byte_count; ++index) {
        if (!machine_trace_append_format(
                builder,
                "%s0x%02x",
                index == 0u ? "" : ",",
                (unsigned int)trace_summary->payload_bytes[index])) {
            return 0;
        }
    }
    return machine_trace_append_format(builder, "]");
}

static int machine_trace_append_immediate_hint(MachineTraceStringBuilder *builder,
    const MachineTraceSummary *trace_summary) {
    if (!builder || !trace_summary) {
        return 0;
    }
    if (!trace_summary->has_immediate_hint) {
        return machine_trace_append_format(builder, " imm=-");
    }
    return machine_trace_append_format(builder, " imm=%zu", trace_summary->immediate_hint);
}

static MachineTraceChangeClass machine_trace_classify_change(const MachineDeltaSummary *delta_summary) {
    unsigned int bits = 0u;

    if (!delta_summary) {
        return MACHINE_TRACE_CHANGE_CLASS_NONE;
    }
    if (delta_summary->has_status_change && delta_summary->status_changed) {
        bits |= 1u;
    }
    if (delta_summary->has_program_counter_change && delta_summary->program_counter_changed) {
        bits |= 2u;
    }
    if (delta_summary->has_stack_pointer_change && delta_summary->stack_pointer_changed) {
        bits |= 4u;
    }
    if (delta_summary->has_fetch_change && delta_summary->fetch_changed) {
        bits |= 8u;
    }

    switch (bits) {
        case 0u:
            return MACHINE_TRACE_CHANGE_CLASS_NONE;
        case 1u:
            return MACHINE_TRACE_CHANGE_CLASS_STATUS_ONLY;
        case 2u:
            return MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_ONLY;
        case 4u:
            return MACHINE_TRACE_CHANGE_CLASS_STACK_ONLY;
        case 8u:
            return MACHINE_TRACE_CHANGE_CLASS_FETCH_ONLY;
        case 3u:
            return MACHINE_TRACE_CHANGE_CLASS_STATUS_AND_PROGRAM_COUNTER;
        case 5u:
            return MACHINE_TRACE_CHANGE_CLASS_STATUS_AND_STACK;
        case 9u:
            return MACHINE_TRACE_CHANGE_CLASS_STATUS_AND_FETCH;
        case 6u:
            return MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_AND_STACK;
        case 10u:
            return MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_AND_FETCH;
        case 12u:
            return MACHINE_TRACE_CHANGE_CLASS_STACK_AND_FETCH;
        case 7u:
            return MACHINE_TRACE_CHANGE_CLASS_STATUS_PROGRAM_COUNTER_AND_STACK;
        case 11u:
            return MACHINE_TRACE_CHANGE_CLASS_STATUS_PROGRAM_COUNTER_AND_FETCH;
        case 13u:
            return MACHINE_TRACE_CHANGE_CLASS_STATUS_STACK_AND_FETCH;
        case 14u:
            return MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_STACK_AND_FETCH;
        case 15u:
            return MACHINE_TRACE_CHANGE_CLASS_STATUS_PROGRAM_COUNTER_STACK_AND_FETCH;
    }
    return MACHINE_TRACE_CHANGE_CLASS_NONE;
}

void machine_trace_file_init(MachineTraceFile *trace_file) {
    if (!trace_file) {
        return;
    }
    machine_delta_file_init(&trace_file->delta_file);
    trace_file->resolution_kind = MACHINE_TRACE_RESOLUTION_NONE;
    trace_file->trace_kind = MACHINE_TRACE_KIND_NONE;
    trace_file->change_class = MACHINE_TRACE_CHANGE_CLASS_NONE;
}

void machine_trace_file_free(MachineTraceFile *trace_file) {
    if (!trace_file) {
        return;
    }
    machine_delta_file_free(&trace_file->delta_file);
    machine_trace_file_init(trace_file);
}

void machine_trace_report_init(MachineTraceReport *report) {
    if (!report) {
        return;
    }
    machine_trace_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_delta_report_init(&report->delta_report);
    memset(&report->trace_summary, 0, sizeof(report->trace_summary));
}

void machine_trace_report_free(MachineTraceReport *report) {
    if (!report) {
        return;
    }
    machine_delta_report_free(&report->delta_report);
    machine_trace_file_free(&report->file);
    machine_trace_report_init(report);
}

const char *machine_trace_resolution_kind_name(MachineTraceResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_TRACE_RESOLUTION_NONE:
            return "none";
        case MACHINE_TRACE_RESOLUTION_EXACT_TRACE:
            return "exact-trace";
        case MACHINE_TRACE_RESOLUTION_PREVIEW_TRACE:
            return "preview-trace";
        case MACHINE_TRACE_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_TRACE_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_trace_kind_name(MachineTraceKind trace_kind) {
    switch (trace_kind) {
        case MACHINE_TRACE_KIND_NONE:
            return "none";
        case MACHINE_TRACE_KIND_STATE_RECORD:
            return "state-record";
    }
    return "unknown";
}

const char *machine_trace_change_class_name(MachineTraceChangeClass change_class) {
    switch (change_class) {
        case MACHINE_TRACE_CHANGE_CLASS_NONE:
            return "none";
        case MACHINE_TRACE_CHANGE_CLASS_STATUS_ONLY:
            return "status-only";
        case MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_ONLY:
            return "program-counter-only";
        case MACHINE_TRACE_CHANGE_CLASS_FETCH_ONLY:
            return "fetch-only";
        case MACHINE_TRACE_CHANGE_CLASS_STATUS_AND_PROGRAM_COUNTER:
            return "status-and-program-counter";
        case MACHINE_TRACE_CHANGE_CLASS_STATUS_AND_FETCH:
            return "status-and-fetch";
        case MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_AND_FETCH:
            return "program-counter-and-fetch";
        case MACHINE_TRACE_CHANGE_CLASS_STATUS_PROGRAM_COUNTER_AND_FETCH:
            return "status-program-counter-and-fetch";
        case MACHINE_TRACE_CHANGE_CLASS_STACK_ONLY:
            return "stack-only";
        case MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_AND_STACK:
            return "program-counter-and-stack";
        case MACHINE_TRACE_CHANGE_CLASS_STACK_AND_FETCH:
            return "stack-and-fetch";
        case MACHINE_TRACE_CHANGE_CLASS_STATUS_AND_STACK:
            return "status-and-stack";
        case MACHINE_TRACE_CHANGE_CLASS_STATUS_PROGRAM_COUNTER_AND_STACK:
            return "status-program-counter-and-stack";
        case MACHINE_TRACE_CHANGE_CLASS_STATUS_STACK_AND_FETCH:
            return "status-stack-and-fetch";
        case MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_STACK_AND_FETCH:
            return "program-counter-stack-and-fetch";
        case MACHINE_TRACE_CHANGE_CLASS_STATUS_PROGRAM_COUNTER_STACK_AND_FETCH:
            return "status-program-counter-stack-and-fetch";
    }
    return "unknown";
}

int machine_trace_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineTraceTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->surfaces_exact_trace = 1;
    out_summary->surfaces_preview_trace = 1;
    out_summary->surfaces_change_class = 1;
    return 1;
}

int machine_trace_file_get_target_policy_summary(const MachineTraceFile *trace_file,
    MachineTraceTargetPolicySummary *out_summary) {
    if (!trace_file || !out_summary) {
        return 0;
    }
    return machine_trace_get_target_policy_summary(
        trace_file->delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_trace_file_get_summary(const MachineTraceFile *trace_file,
    size_t *out_mapped_byte_count) {
    if (!trace_file) {
        return 0;
    }
    return machine_delta_file_get_summary(&trace_file->delta_file, out_mapped_byte_count);
}

int machine_trace_file_get_source_elf_artifact_summary(const MachineTraceFile *trace_file,
    MachineElfArtifactSummary *out_summary) {
    if (!trace_file || !out_summary) {
        return 0;
    }
    return machine_delta_file_get_source_elf_artifact_summary(&trace_file->delta_file, out_summary);
}

int machine_trace_file_get_header_summary(const MachineTraceFile *trace_file,
    MachineTraceHeaderSummary *out_summary) {
    MachineDeltaHeaderSummary delta_header_summary;

    memset(&delta_header_summary, 0, sizeof(delta_header_summary));
    if (!trace_file || !out_summary ||
        !machine_delta_file_get_header_summary(&trace_file->delta_file, &delta_header_summary)) {
        return 0;
    }
    out_summary->target_profile = delta_header_summary.target_profile;
    out_summary->delta_resolution_kind = trace_file->delta_file.resolution_kind;
    out_summary->origin_step_status = delta_header_summary.origin_step_status;
    out_summary->origin_program_counter = delta_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = delta_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = delta_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = delta_header_summary.mapped_byte_count;
    return 1;
}

int machine_trace_file_get_delta_summary(const MachineTraceFile *trace_file,
    MachineDeltaSummary *out_summary) {
    if (!trace_file || !out_summary) {
        return 0;
    }
    return machine_delta_file_get_delta_summary(&trace_file->delta_file, out_summary);
}

int machine_trace_file_get_trace_summary(const MachineTraceFile *trace_file,
    MachineTraceSummary *out_summary) {
    MachineDeltaSummary delta_summary;

    memset(&delta_summary, 0, sizeof(delta_summary));
    if (!trace_file || !out_summary ||
        !machine_trace_file_get_delta_summary(trace_file, &delta_summary)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->resolution_kind = trace_file->resolution_kind;
    out_summary->trace_kind = trace_file->trace_kind;
    out_summary->change_class = trace_file->change_class;
    out_summary->delta_resolution_kind = delta_summary.resolution_kind;
    out_summary->delta_kind = delta_summary.delta_kind;
    out_summary->observe_resolution_kind = delta_summary.observe_resolution_kind;
    out_summary->observe_kind = delta_summary.observe_kind;
    out_summary->apply_resolution_kind = delta_summary.apply_resolution_kind;
    out_summary->apply_kind = delta_summary.apply_kind;
    out_summary->commit_resolution_kind = delta_summary.commit_resolution_kind;
    out_summary->commit_kind = delta_summary.commit_kind;
    out_summary->writeback_resolution_kind = delta_summary.writeback_resolution_kind;
    out_summary->writeback_commit_kind = delta_summary.writeback_commit_kind;
    out_summary->mutation_resolution_kind = delta_summary.mutation_resolution_kind;
    out_summary->mutation_effect_kind = delta_summary.mutation_effect_kind;
    out_summary->state_resolution_kind = delta_summary.state_resolution_kind;
    out_summary->transition_resolution_kind = delta_summary.transition_resolution_kind;
    out_summary->action_kind = delta_summary.action_kind;
    out_summary->payload_kind = delta_summary.payload_kind;
    out_summary->tag_class = delta_summary.tag_class;
    out_summary->raw_byte = delta_summary.raw_byte;
    out_summary->tag_value = delta_summary.tag_value;
    out_summary->is_known = delta_summary.is_known;
    out_summary->tag_name = delta_summary.tag_name;
    out_summary->instruction_byte_count = delta_summary.instruction_byte_count;
    out_summary->payload_byte_count = delta_summary.payload_byte_count;
    memcpy(out_summary->payload_bytes, delta_summary.payload_bytes, sizeof(out_summary->payload_bytes));
    out_summary->has_immediate_hint = delta_summary.has_immediate_hint;
    out_summary->immediate_hint = delta_summary.immediate_hint;
    out_summary->is_exact_trace = trace_file->resolution_kind == MACHINE_TRACE_RESOLUTION_EXACT_TRACE;
    out_summary->origin_status = delta_summary.origin_status;
    out_summary->origin_program_counter = delta_summary.origin_program_counter;
    out_summary->origin_stack_pointer = delta_summary.origin_stack_pointer;
    out_summary->has_origin_fetch = delta_summary.has_origin_fetch;
    out_summary->origin_segment_index = delta_summary.origin_segment_index;
    out_summary->origin_byte = delta_summary.origin_byte;
    out_summary->has_observed_state = delta_summary.has_observed_state;
    out_summary->observed_status = delta_summary.observed_status;
    out_summary->observed_program_counter = delta_summary.observed_program_counter;
    out_summary->observed_stack_pointer = delta_summary.observed_stack_pointer;
    out_summary->has_observed_fetch = delta_summary.has_observed_fetch;
    out_summary->observed_segment_index = delta_summary.observed_segment_index;
    out_summary->observed_byte = delta_summary.observed_byte;
    out_summary->has_status_change = delta_summary.has_status_change;
    out_summary->status_changed = delta_summary.status_changed;
    out_summary->has_program_counter_change = delta_summary.has_program_counter_change;
    out_summary->program_counter_changed = delta_summary.program_counter_changed;
    out_summary->has_stack_pointer_change = delta_summary.has_stack_pointer_change;
    out_summary->stack_pointer_changed = delta_summary.stack_pointer_changed;
    out_summary->has_fetch_change = delta_summary.has_fetch_change;
    out_summary->fetch_changed = delta_summary.fetch_changed;
    out_summary->has_primary_target_block = delta_summary.has_primary_target_block;
    out_summary->primary_target_block_index = delta_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = delta_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = delta_summary.secondary_target_block_index;
    out_summary->has_return_immediate = delta_summary.has_return_immediate;
    out_summary->return_immediate = delta_summary.return_immediate;
    return 1;
}

int machine_trace_verify_file(const MachineTraceFile *trace_file,
    MachineTraceError *error) {
    MachineDeltaSummary delta_summary;
    MachineTraceSummary trace_summary;
    MachineTraceChangeClass expected_change_class;

    memset(&delta_summary, 0, sizeof(delta_summary));
    memset(&trace_summary, 0, sizeof(trace_summary));
    if (!trace_file) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-100: invalid trace file");
        return 0;
    }
    if (!machine_delta_verify_file(&trace_file->delta_file, NULL) ||
        !machine_trace_file_get_delta_summary(trace_file, &delta_summary) ||
        !machine_trace_file_get_trace_summary(trace_file, &trace_summary)) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-101: invalid delta input");
        return 0;
    }

    expected_change_class = machine_trace_classify_change(&delta_summary);
    if (trace_file->change_class != expected_change_class) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-102: invalid trace change class");
        return 0;
    }

    switch (trace_file->resolution_kind) {
        case MACHINE_TRACE_RESOLUTION_EXACT_TRACE:
            if (trace_file->delta_file.resolution_kind != MACHINE_DELTA_RESOLUTION_EXACT_STATE_DELTA ||
                trace_file->trace_kind != MACHINE_TRACE_KIND_STATE_RECORD ||
                !trace_summary.is_exact_trace ||
                !delta_summary.has_observed_state) {
                machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-103: invalid exact trace");
                return 0;
            }
            break;
        case MACHINE_TRACE_RESOLUTION_PREVIEW_TRACE:
            if (trace_file->delta_file.resolution_kind != MACHINE_DELTA_RESOLUTION_PREVIEW_STATE_DELTA ||
                trace_file->trace_kind != MACHINE_TRACE_KIND_STATE_RECORD ||
                trace_summary.is_exact_trace ||
                !delta_summary.has_observed_state) {
                machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-104: invalid preview trace");
                return 0;
            }
            break;
        case MACHINE_TRACE_RESOLUTION_BLOCKED_ON_CONTROL:
            if (trace_file->delta_file.resolution_kind != MACHINE_DELTA_RESOLUTION_BLOCKED_ON_CONTROL ||
                trace_file->trace_kind != MACHINE_TRACE_KIND_NONE ||
                expected_change_class != MACHINE_TRACE_CHANGE_CLASS_NONE) {
                machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-105: invalid blocked-control trace");
                return 0;
            }
            break;
        case MACHINE_TRACE_RESOLUTION_BLOCKED_UNSUPPORTED:
            if (trace_file->delta_file.resolution_kind != MACHINE_DELTA_RESOLUTION_BLOCKED_UNSUPPORTED ||
                trace_file->trace_kind != MACHINE_TRACE_KIND_NONE ||
                expected_change_class != MACHINE_TRACE_CHANGE_CLASS_NONE) {
                machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-106: invalid blocked-unsupported trace");
                return 0;
            }
            break;
        case MACHINE_TRACE_RESOLUTION_NONE:
        default:
            machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-107: invalid trace resolution kind");
            return 0;
    }
    return 1;
}

int machine_trace_clone_file(const MachineTraceFile *source,
    MachineTraceFile *out_clone,
    MachineTraceError *error) {
    if (!source || !out_clone) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-108: invalid clone contract");
        return 0;
    }
    if (!machine_trace_verify_file(source, error)) {
        return 0;
    }

    machine_trace_file_free(out_clone);
    if (!machine_delta_clone_file(&source->delta_file, &out_clone->delta_file, NULL)) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-109: failed cloning delta input");
        machine_trace_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->trace_kind = source->trace_kind;
    out_clone->change_class = source->change_class;
    return 1;
}

int machine_trace_build_from_machine_delta_file(const MachineDeltaFile *delta_file,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error) {
    MachineDeltaSummary delta_summary;

    memset(&delta_summary, 0, sizeof(delta_summary));
    if (!delta_file || !out_trace_file) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-110: invalid build contract");
        return 0;
    }
    if (!machine_delta_verify_file(delta_file, NULL) ||
        !machine_delta_file_get_delta_summary(delta_file, &delta_summary)) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-111: invalid delta input");
        return 0;
    }

    machine_trace_file_free(out_trace_file);
    if (!machine_delta_clone_file(delta_file, &out_trace_file->delta_file, NULL)) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-112: failed cloning delta input");
        machine_trace_file_free(out_trace_file);
        return 0;
    }

    out_trace_file->change_class = machine_trace_classify_change(&delta_summary);
    switch (delta_file->resolution_kind) {
        case MACHINE_DELTA_RESOLUTION_EXACT_STATE_DELTA:
            out_trace_file->resolution_kind = MACHINE_TRACE_RESOLUTION_EXACT_TRACE;
            out_trace_file->trace_kind = MACHINE_TRACE_KIND_STATE_RECORD;
            break;
        case MACHINE_DELTA_RESOLUTION_PREVIEW_STATE_DELTA:
            out_trace_file->resolution_kind = MACHINE_TRACE_RESOLUTION_PREVIEW_TRACE;
            out_trace_file->trace_kind = MACHINE_TRACE_KIND_STATE_RECORD;
            break;
        case MACHINE_DELTA_RESOLUTION_BLOCKED_ON_CONTROL:
            out_trace_file->resolution_kind = MACHINE_TRACE_RESOLUTION_BLOCKED_ON_CONTROL;
            out_trace_file->trace_kind = MACHINE_TRACE_KIND_NONE;
            break;
        case MACHINE_DELTA_RESOLUTION_BLOCKED_UNSUPPORTED:
            out_trace_file->resolution_kind = MACHINE_TRACE_RESOLUTION_BLOCKED_UNSUPPORTED;
            out_trace_file->trace_kind = MACHINE_TRACE_KIND_NONE;
            break;
        case MACHINE_DELTA_RESOLUTION_NONE:
        default:
            machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-113: unsupported delta resolution");
            machine_trace_file_free(out_trace_file);
            return 0;
    }

    if (!machine_trace_verify_file(out_trace_file, error)) {
        machine_trace_file_free(out_trace_file);
        return 0;
    }
    return 1;
}

int machine_trace_build_from_machine_delta_report(const MachineDeltaReport *report,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error) {
    const MachineDeltaFile *delta_file = NULL;

    if (!report) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-114: invalid build-from-delta-report contract");
        return 0;
    }
    if (!machine_delta_report_get_file(report, &delta_file) || !delta_file) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-115: failed recovering delta file from report");
        return 0;
    }
    return machine_trace_build_from_machine_delta_file(delta_file, out_trace_file, error);
}

#define MACHINE_TRACE_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineTraceFile *out_trace_file, MachineTraceError *error) { \
    MachineDeltaFile delta_file; \
    int ok; \
    machine_delta_file_init(&delta_file); \
    ok = builder(source, &delta_file, NULL) && \
        machine_trace_build_from_machine_delta_file(&delta_file, out_trace_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-116: failed building trace wrapper"); \
    } \
    machine_delta_file_free(&delta_file); \
    return ok; \
}

MACHINE_TRACE_DEFINE_BUILD_FROM_WRAPPER(machine_trace_build_from_machine_observe_file,
    MachineObserveFile, machine_delta_build_from_machine_observe_file)
MACHINE_TRACE_DEFINE_BUILD_FROM_WRAPPER(machine_trace_build_from_machine_observe_report,
    MachineObserveReport, machine_delta_build_from_machine_observe_report)
MACHINE_TRACE_DEFINE_BUILD_FROM_WRAPPER(machine_trace_build_from_machine_apply_file,
    MachineApplyFile, machine_delta_build_from_machine_apply_file)
MACHINE_TRACE_DEFINE_BUILD_FROM_WRAPPER(machine_trace_build_from_machine_apply_report,
    MachineApplyReport, machine_delta_build_from_machine_apply_report)
MACHINE_TRACE_DEFINE_BUILD_FROM_WRAPPER(machine_trace_build_from_machine_commit_file,
    MachineCommitFile, machine_delta_build_from_machine_commit_file)
MACHINE_TRACE_DEFINE_BUILD_FROM_WRAPPER(machine_trace_build_from_machine_commit_report,
    MachineCommitReport, machine_delta_build_from_machine_commit_report)
MACHINE_TRACE_DEFINE_BUILD_FROM_WRAPPER(machine_trace_build_from_machine_step_file,
    MachineStepFile, machine_delta_build_from_machine_step_file)
MACHINE_TRACE_DEFINE_BUILD_FROM_WRAPPER(machine_trace_build_from_machine_step_report,
    MachineStepReport, machine_delta_build_from_machine_step_report)
MACHINE_TRACE_DEFINE_BUILD_FROM_WRAPPER(machine_trace_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_delta_build_from_machine_ir_report)

int machine_trace_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error) {
    MachineDeltaFile delta_file;
    int ok;

    machine_delta_file_init(&delta_file);
    ok = machine_delta_build_from_machine_ir_report_with_profile(report, profile, &delta_file, NULL) &&
        machine_trace_build_from_machine_delta_file(&delta_file, out_trace_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-117: failed building profiled trace wrapper");
    }
    machine_delta_file_free(&delta_file);
    return ok;
}

int machine_trace_build_report_from_file(const MachineTraceFile *source,
    MachineTraceReport *out_report,
    MachineTraceError *error) {
    if (!source || !out_report) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-118: invalid report build contract");
        return 0;
    }
    if (!machine_trace_verify_file(source, error)) {
        return 0;
    }

    machine_trace_report_free(out_report);
    if (!machine_trace_clone_file(source, &out_report->file, error)) {
        machine_trace_report_free(out_report);
        return 0;
    }
    if (!machine_trace_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_trace_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_delta_build_report_from_file(&out_report->file.delta_file, &out_report->delta_report, NULL) ||
        !machine_trace_file_get_trace_summary(&out_report->file, &out_report->trace_summary)) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-119: failed summarizing trace report");
        machine_trace_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineTraceReport *out_report, MachineTraceError *error) { \
    MachineTraceFile trace_file; \
    int ok; \
    machine_trace_file_init(&trace_file); \
    ok = builder(source, &trace_file, error) && \
        machine_trace_build_report_from_file(&trace_file, out_report, error); \
    machine_trace_file_free(&trace_file); \
    return ok; \
}

MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(machine_trace_build_report_from_machine_delta_file,
    MachineDeltaFile, machine_trace_build_from_machine_delta_file)
MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(machine_trace_build_report_from_machine_delta_report,
    MachineDeltaReport, machine_trace_build_from_machine_delta_report)
MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(machine_trace_build_report_from_machine_observe_file,
    MachineObserveFile, machine_trace_build_from_machine_observe_file)
MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(machine_trace_build_report_from_machine_observe_report,
    MachineObserveReport, machine_trace_build_from_machine_observe_report)
MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(machine_trace_build_report_from_machine_apply_file,
    MachineApplyFile, machine_trace_build_from_machine_apply_file)
MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(machine_trace_build_report_from_machine_apply_report,
    MachineApplyReport, machine_trace_build_from_machine_apply_report)
MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(machine_trace_build_report_from_machine_commit_file,
    MachineCommitFile, machine_trace_build_from_machine_commit_file)
MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(machine_trace_build_report_from_machine_commit_report,
    MachineCommitReport, machine_trace_build_from_machine_commit_report)
MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(machine_trace_build_report_from_machine_step_file,
    MachineStepFile, machine_trace_build_from_machine_step_file)
MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(machine_trace_build_report_from_machine_step_report,
    MachineStepReport, machine_trace_build_from_machine_step_report)
MACHINE_TRACE_DEFINE_REPORT_FROM_WRAPPER(machine_trace_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_trace_build_from_machine_ir_report)

int machine_trace_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTraceReport *out_report,
    MachineTraceError *error) {
    MachineTraceFile trace_file;
    int ok;

    machine_trace_file_init(&trace_file);
    ok = machine_trace_build_from_machine_ir_report_with_profile(report, profile, &trace_file, error) &&
        machine_trace_build_report_from_file(&trace_file, out_report, error);
    machine_trace_file_free(&trace_file);
    return ok;
}

int machine_trace_report_refresh(MachineTraceReport *report,
    MachineTraceError *error) {
    MachineTraceReport refreshed_report;
    int ok;

    if (!report) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-120: invalid report refresh contract");
        return 0;
    }
    machine_trace_report_init(&refreshed_report);
    ok = machine_trace_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_trace_report_free(report);
        *report = refreshed_report;
    } else {
        machine_trace_report_free(&refreshed_report);
    }
    return ok;
}

int machine_trace_dump_file(const MachineTraceFile *trace_file,
    char **out_text,
    MachineTraceError *error) {
    MachineTraceStringBuilder builder;
    MachineTraceHeaderSummary header_summary;
    MachineTraceSummary trace_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&trace_summary, 0, sizeof(trace_summary));
    if (!trace_file || !out_text ||
        !machine_trace_verify_file(trace_file, error) ||
        !machine_trace_file_get_header_summary(trace_file, &header_summary) ||
        !machine_trace_file_get_trace_summary(trace_file, &trace_summary)) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-121: invalid dump contract");
        return 0;
    }

    if (!machine_trace_append_format(
            &builder,
            "machine_trace profile=%s elf_origin=%s elf_semantics=%s delta=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_elf_target_profile_name(
                trace_file->delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file
                    .transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file
                    .load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                trace_file->delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file
                    .transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file
                    .load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_delta_resolution_kind_name(header_summary.delta_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_trace_append_format(
            &builder,
            "trace: resolution=%s kind=%s change-class=%s delta=%s observe=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_trace_resolution_kind_name(trace_summary.resolution_kind),
            machine_trace_kind_name(trace_summary.trace_kind),
            machine_trace_change_class_name(trace_summary.change_class),
            machine_delta_resolution_kind_name(trace_summary.delta_resolution_kind),
            machine_observe_resolution_kind_name(trace_summary.observe_resolution_kind),
            machine_apply_resolution_kind_name(trace_summary.apply_resolution_kind),
            machine_commit_resolution_kind_name(trace_summary.commit_resolution_kind),
            machine_writeback_resolution_kind_name(trace_summary.writeback_resolution_kind),
            machine_mutation_resolution_kind_name(trace_summary.mutation_resolution_kind),
            machine_mutation_effect_kind_name(trace_summary.mutation_effect_kind),
            machine_transition_resolution_kind_name(trace_summary.transition_resolution_kind),
            machine_interp_action_kind_name(trace_summary.action_kind),
            (unsigned int)trace_summary.raw_byte,
            (unsigned int)trace_summary.tag_value,
            trace_summary.is_known ? "yes" : "no",
            trace_summary.tag_name ? trace_summary.tag_name : "-",
            trace_summary.instruction_byte_count) ||
        !machine_trace_append_payload_bytes(&builder, &trace_summary) ||
        !machine_trace_append_immediate_hint(&builder, &trace_summary) ||
        !machine_trace_append_format(
            &builder,
            " exact=%s origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            trace_summary.is_exact_trace ? "yes" : "no",
            machine_step_status_name(trace_summary.origin_status),
            trace_summary.has_observed_state ? machine_step_status_name(trace_summary.observed_status) : "-",
            trace_summary.has_status_change ? (trace_summary.status_changed ? "yes" : "no") : "-",
            trace_summary.has_program_counter_change ? (trace_summary.program_counter_changed ? "yes" : "no") : "-",
            trace_summary.has_stack_pointer_change ? (trace_summary.stack_pointer_changed ? "yes" : "no") : "-",
            trace_summary.has_fetch_change ? (trace_summary.fetch_changed ? "yes" : "no") : "-")) {
        free(builder.data);
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-122: out of memory building dump");
        return 0;
    }
    if (!machine_trace_append_targets(&builder, &trace_summary) ||
        !machine_trace_append_return_immediate(&builder, &trace_summary) ||
        !machine_trace_append_format(&builder, "\n")) {
        free(builder.data);
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-123: out of memory terminating dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_trace_build_dump_from_file(const MachineTraceFile *source,
    char **out_text,
    MachineTraceError *error) {
    return machine_trace_dump_file(source, out_text, error);
}

#define MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineTraceError *error) { \
    MachineTraceFile trace_file; \
    int ok; \
    machine_trace_file_init(&trace_file); \
    ok = builder(source, &trace_file, error) && \
        machine_trace_dump_file(&trace_file, out_text, error); \
    machine_trace_file_free(&trace_file); \
    return ok; \
}

MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(machine_trace_build_dump_from_machine_delta_file,
    MachineDeltaFile, machine_trace_build_from_machine_delta_file)
MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(machine_trace_build_dump_from_machine_delta_report,
    MachineDeltaReport, machine_trace_build_from_machine_delta_report)
MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(machine_trace_build_dump_from_machine_observe_file,
    MachineObserveFile, machine_trace_build_from_machine_observe_file)
MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(machine_trace_build_dump_from_machine_observe_report,
    MachineObserveReport, machine_trace_build_from_machine_observe_report)
MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(machine_trace_build_dump_from_machine_apply_file,
    MachineApplyFile, machine_trace_build_from_machine_apply_file)
MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(machine_trace_build_dump_from_machine_apply_report,
    MachineApplyReport, machine_trace_build_from_machine_apply_report)
MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(machine_trace_build_dump_from_machine_commit_file,
    MachineCommitFile, machine_trace_build_from_machine_commit_file)
MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(machine_trace_build_dump_from_machine_commit_report,
    MachineCommitReport, machine_trace_build_from_machine_commit_report)
MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(machine_trace_build_dump_from_machine_step_file,
    MachineStepFile, machine_trace_build_from_machine_step_file)
MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(machine_trace_build_dump_from_machine_step_report,
    MachineStepReport, machine_trace_build_from_machine_step_report)
MACHINE_TRACE_DEFINE_DUMP_FROM_WRAPPER(machine_trace_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_trace_build_from_machine_ir_report)

int machine_trace_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTraceError *error) {
    MachineTraceFile trace_file;
    int ok;

    machine_trace_file_init(&trace_file);
    ok = machine_trace_build_from_machine_ir_report_with_profile(report, profile, &trace_file, error) &&
        machine_trace_dump_file(&trace_file, out_text, error);
    machine_trace_file_free(&trace_file);
    return ok;
}

int machine_trace_report_get_summary(const MachineTraceReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_trace_report_get_overview_artifact(const MachineTraceReport *report,
    MachineTraceReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->delta_report = &report->delta_report;
    out_artifact->trace_summary = &report->trace_summary;
    return 1;
}

int machine_trace_report_get_file(const MachineTraceReport *report,
    const MachineTraceFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_trace_report_get_delta_file(const MachineTraceReport *report,
    const MachineDeltaFile **out_delta_file) {
    if (!report || !out_delta_file) {
        return 0;
    }
    *out_delta_file = &report->file.delta_file;
    return 1;
}

int machine_trace_report_get_delta_report(const MachineTraceReport *report,
    const MachineDeltaReport **out_delta_report) {
    if (!report || !out_delta_report) {
        return 0;
    }
    *out_delta_report = &report->delta_report;
    return 1;
}

int machine_trace_report_get_source_elf_artifact_summary_artifact(
    const MachineTraceReport *report,
    const MachineElfArtifactSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file
                        .state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file
                        .launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

#define MACHINE_TRACE_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineTraceReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_TRACE_DEFINE_REPORT_ARTIFACT_GETTER(machine_trace_report_get_header_summary_artifact,
    header_summary, MachineTraceHeaderSummary)
MACHINE_TRACE_DEFINE_REPORT_ARTIFACT_GETTER(machine_trace_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineTraceTargetPolicySummary)
MACHINE_TRACE_DEFINE_REPORT_ARTIFACT_GETTER(machine_trace_report_get_trace_summary_artifact,
    trace_summary, MachineTraceSummary)

int machine_trace_report_overview_artifact_get_delta_report(
    const MachineTraceReportOverviewArtifact *artifact,
    const MachineDeltaReport **out_delta_report) {
    if (!artifact || !out_delta_report) {
        return 0;
    }
    *out_delta_report = artifact->delta_report;
    return 1;
}

int machine_trace_report_overview_artifact_get_trace_summary_artifact(
    const MachineTraceReportOverviewArtifact *artifact,
    const MachineTraceSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->trace_summary;
    return 1;
}

int machine_trace_dump_report(const MachineTraceReport *report,
    char **out_text,
    MachineTraceError *error) {
    MachineTraceStringBuilder builder;
    const MachineTraceSummary *trace_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_trace_report_get_trace_summary_artifact(report, &trace_summary) ||
        !trace_summary) {
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-124: invalid report dump contract");
        return 0;
    }

    if (!machine_trace_append_format(
            &builder,
            "machine_trace profile=%s elf_origin=%s elf_semantics=%s delta=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file
                    .transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file
                    .load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file
                    .transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file
                    .load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_delta_resolution_kind_name(report->header_summary.delta_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_trace_append_format(
            &builder,
            "trace: resolution=%s kind=%s change-class=%s delta=%s observe=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_trace_resolution_kind_name(trace_summary->resolution_kind),
            machine_trace_kind_name(trace_summary->trace_kind),
            machine_trace_change_class_name(trace_summary->change_class),
            machine_delta_resolution_kind_name(trace_summary->delta_resolution_kind),
            machine_observe_resolution_kind_name(trace_summary->observe_resolution_kind),
            machine_apply_resolution_kind_name(trace_summary->apply_resolution_kind),
            machine_commit_resolution_kind_name(trace_summary->commit_resolution_kind),
            machine_writeback_resolution_kind_name(trace_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(trace_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(trace_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(trace_summary->transition_resolution_kind),
            machine_interp_action_kind_name(trace_summary->action_kind),
            (unsigned int)trace_summary->raw_byte,
            (unsigned int)trace_summary->tag_value,
            trace_summary->is_known ? "yes" : "no",
            trace_summary->tag_name ? trace_summary->tag_name : "-",
            trace_summary->instruction_byte_count) ||
        !machine_trace_append_payload_bytes(&builder, trace_summary) ||
        !machine_trace_append_immediate_hint(&builder, trace_summary) ||
        !machine_trace_append_format(
            &builder,
            " exact=%s origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            trace_summary->is_exact_trace ? "yes" : "no",
            machine_step_status_name(trace_summary->origin_status),
            trace_summary->has_observed_state ? machine_step_status_name(trace_summary->observed_status) : "-",
            trace_summary->has_status_change ? (trace_summary->status_changed ? "yes" : "no") : "-",
            trace_summary->has_program_counter_change ? (trace_summary->program_counter_changed ? "yes" : "no") : "-",
            trace_summary->has_stack_pointer_change ? (trace_summary->stack_pointer_changed ? "yes" : "no") : "-",
            trace_summary->has_fetch_change ? (trace_summary->fetch_changed ? "yes" : "no") : "-") ||
        !machine_trace_append_targets(&builder, trace_summary) ||
        !machine_trace_append_return_immediate(&builder, trace_summary) ||
        !machine_trace_append_format(&builder, "\nreport_overview:\n") ||
        !machine_trace_append_format(
            &builder,
            "  origin: delta=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_delta_resolution_kind_name(report->header_summary.delta_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer) ||
        !machine_trace_append_format(
            &builder,
            "  elf_source: target=%s origin=%s semantics=%s\n",
            machine_elf_target_profile_name(
                report->file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file
                    .transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file
                    .load_file.exec_file.image_file.source_elf_artifact_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file
                    .transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file
                    .load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file
                    .transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file
                    .load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics)) ||
        !machine_trace_append_format(
            &builder,
            "  policy: profile=%s exact=%s preview=%s class=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.surfaces_exact_trace ? "yes" : "no",
            report->target_policy_summary.surfaces_preview_trace ? "yes" : "no",
            report->target_policy_summary.surfaces_change_class ? "yes" : "no") ||
        !machine_trace_append_format(
            &builder,
            "  trace: resolution=%s kind=%s class=%s exact=%s state=%s status=%s pc=%s fetch=%s",
            machine_trace_resolution_kind_name(trace_summary->resolution_kind),
            machine_trace_kind_name(trace_summary->trace_kind),
            machine_trace_change_class_name(trace_summary->change_class),
            trace_summary->is_exact_trace ? "yes" : "no",
            trace_summary->has_observed_state ? "yes" : "no",
            trace_summary->has_status_change ? (trace_summary->status_changed ? "yes" : "no") : "-",
            trace_summary->has_program_counter_change ? (trace_summary->program_counter_changed ? "yes" : "no") : "-",
            trace_summary->has_fetch_change ? (trace_summary->fetch_changed ? "yes" : "no") : "-")) {
        free(builder.data);
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-125: out of memory building report dump");
        return 0;
    }
    if (!machine_trace_append_targets(&builder, trace_summary) ||
        !machine_trace_append_return_immediate(&builder, trace_summary) ||
        !machine_trace_append_format(&builder, "\n")) {
        free(builder.data);
        machine_trace_set_error(error, 0, 0, "MACHINE-TRACE-126: out of memory terminating report dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_trace_build_report_dump_from_file(const MachineTraceFile *source,
    char **out_text,
    MachineTraceError *error) {
    MachineTraceReport report;
    int ok;

    machine_trace_report_init(&report);
    ok = machine_trace_build_report_from_file(source, &report, error) &&
        machine_trace_dump_report(&report, out_text, error);
    machine_trace_report_free(&report);
    return ok;
}

#define MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineTraceError *error) { \
    MachineTraceReport report; \
    int ok; \
    machine_trace_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_trace_dump_report(&report, out_text, error); \
    machine_trace_report_free(&report); \
    return ok; \
}

MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_trace_build_report_dump_from_machine_delta_file,
    MachineDeltaFile, machine_trace_build_report_from_machine_delta_file)
MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_trace_build_report_dump_from_machine_delta_report,
    MachineDeltaReport, machine_trace_build_report_from_machine_delta_report)
MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_trace_build_report_dump_from_machine_observe_file,
    MachineObserveFile, machine_trace_build_report_from_machine_observe_file)
MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_trace_build_report_dump_from_machine_observe_report,
    MachineObserveReport, machine_trace_build_report_from_machine_observe_report)
MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_trace_build_report_dump_from_machine_apply_file,
    MachineApplyFile, machine_trace_build_report_from_machine_apply_file)
MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_trace_build_report_dump_from_machine_apply_report,
    MachineApplyReport, machine_trace_build_report_from_machine_apply_report)
MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_trace_build_report_dump_from_machine_commit_file,
    MachineCommitFile, machine_trace_build_report_from_machine_commit_file)
MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_trace_build_report_dump_from_machine_commit_report,
    MachineCommitReport, machine_trace_build_report_from_machine_commit_report)
MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_trace_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_trace_build_report_from_machine_step_file)
MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_trace_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_trace_build_report_from_machine_step_report)
MACHINE_TRACE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_trace_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_trace_build_report_from_machine_ir_report)

int machine_trace_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTraceError *error) {
    MachineTraceReport report_file;
    int ok;

    machine_trace_report_init(&report_file);
    ok = machine_trace_build_report_from_machine_ir_report_with_profile(report, profile, &report_file, error) &&
        machine_trace_dump_report(&report_file, out_text, error);
    machine_trace_report_free(&report_file);
    return ok;
}
