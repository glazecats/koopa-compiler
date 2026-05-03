#include "machine/event.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineEventStringBuilder;

static void machine_event_set_error(MachineEventError *error, int line, int column, const char *fmt, ...);
static int machine_event_append_format(MachineEventStringBuilder *builder, const char *fmt, ...);
static int machine_event_append_targets(MachineEventStringBuilder *builder,
    const MachineEventSummary *event_summary);
static int machine_event_append_return_immediate(MachineEventStringBuilder *builder,
    const MachineEventSummary *event_summary);
static int machine_event_append_payload_bytes(MachineEventStringBuilder *builder,
    const MachineEventSummary *event_summary);
static int machine_event_append_immediate_hint(MachineEventStringBuilder *builder,
    const MachineEventSummary *event_summary);
static MachineEventKind machine_event_classify_kind(const MachineTraceSummary *trace_summary);

static void machine_event_set_error(MachineEventError *error, int line, int column, const char *fmt, ...) {
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

static int machine_event_append_format(MachineEventStringBuilder *builder, const char *fmt, ...) {
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

static int machine_event_append_targets(MachineEventStringBuilder *builder,
    const MachineEventSummary *event_summary) {
    if (!builder || !event_summary) {
        return 0;
    }
    if (!event_summary->has_primary_target_block) {
        return machine_event_append_format(builder, " targets=[]");
    }
    if (!event_summary->has_secondary_target_block) {
        return machine_event_append_format(builder, " targets=[%zu]", event_summary->primary_target_block_index);
    }
    return machine_event_append_format(
        builder,
        " targets=[%zu,%zu]",
        event_summary->primary_target_block_index,
        event_summary->secondary_target_block_index);
}

static int machine_event_append_return_immediate(MachineEventStringBuilder *builder,
    const MachineEventSummary *event_summary) {
    if (!builder || !event_summary) {
        return 0;
    }
    if (!event_summary->has_return_immediate) {
        return machine_event_append_format(builder, " return-imm=-");
    }
    return machine_event_append_format(builder, " return-imm=%zu", event_summary->return_immediate);
}

static int machine_event_append_payload_bytes(MachineEventStringBuilder *builder,
    const MachineEventSummary *event_summary) {
    size_t index;

    if (!builder || !event_summary) {
        return 0;
    }
    if (!machine_event_append_format(builder, " payload=[")) {
        return 0;
    }
    for (index = 0u; index < event_summary->payload_byte_count; ++index) {
        if (!machine_event_append_format(
                builder,
                "%s0x%02x",
                index == 0u ? "" : ",",
                (unsigned int)event_summary->payload_bytes[index])) {
            return 0;
        }
    }
    return machine_event_append_format(builder, "]");
}

static int machine_event_append_immediate_hint(MachineEventStringBuilder *builder,
    const MachineEventSummary *event_summary) {
    if (!builder || !event_summary) {
        return 0;
    }
    if (!event_summary->has_immediate_hint) {
        return machine_event_append_format(builder, " imm=-");
    }
    return machine_event_append_format(builder, " imm=%zu", event_summary->immediate_hint);
}

static MachineEventKind machine_event_classify_kind(const MachineTraceSummary *trace_summary) {
    if (!trace_summary) {
        return MACHINE_EVENT_KIND_NONE;
    }
    if (trace_summary->resolution_kind == MACHINE_TRACE_RESOLUTION_BLOCKED_ON_CONTROL) {
        return MACHINE_EVENT_KIND_BLOCKED_CONTROL;
    }
    if (trace_summary->resolution_kind == MACHINE_TRACE_RESOLUTION_BLOCKED_UNSUPPORTED) {
        return MACHINE_EVENT_KIND_BLOCKED_UNSUPPORTED;
    }
    if (trace_summary->action_kind == MACHINE_INTERP_ACTION_HALT) {
        return MACHINE_EVENT_KIND_CONTROL_HALT;
    }
    switch (trace_summary->mutation_effect_kind) {
        case MACHINE_MUTATION_EFFECT_VALUE_RESULT:
            return MACHINE_EVENT_KIND_REGISTER_RESULT;
        case MACHINE_MUTATION_EFFECT_LOCAL_SLOT:
            return MACHINE_EVENT_KIND_LOCAL_STORE;
        case MACHINE_MUTATION_EFFECT_GLOBAL_SLOT:
            return MACHINE_EVENT_KIND_GLOBAL_STORE;
        case MACHINE_MUTATION_EFFECT_CALL:
            return MACHINE_EVENT_KIND_CALL_EFFECT;
        case MACHINE_MUTATION_EFFECT_CONTROL_ONLY:
            return MACHINE_EVENT_KIND_CONTROL_HALT;
        case MACHINE_MUTATION_EFFECT_NONE:
        default:
            return MACHINE_EVENT_KIND_NONE;
    }
}

void machine_event_file_init(MachineEventFile *event_file) {
    if (!event_file) {
        return;
    }
    machine_trace_file_init(&event_file->trace_file);
    event_file->resolution_kind = MACHINE_EVENT_RESOLUTION_NONE;
    event_file->event_kind = MACHINE_EVENT_KIND_NONE;
}

void machine_event_file_free(MachineEventFile *event_file) {
    if (!event_file) {
        return;
    }
    machine_trace_file_free(&event_file->trace_file);
    machine_event_file_init(event_file);
}

void machine_event_report_init(MachineEventReport *report) {
    if (!report) {
        return;
    }
    machine_event_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_trace_report_init(&report->trace_report);
    memset(&report->event_summary, 0, sizeof(report->event_summary));
}

void machine_event_report_free(MachineEventReport *report) {
    if (!report) {
        return;
    }
    machine_trace_report_free(&report->trace_report);
    machine_event_file_free(&report->file);
    machine_event_report_init(report);
}

const char *machine_event_resolution_kind_name(MachineEventResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_EVENT_RESOLUTION_NONE:
            return "none";
        case MACHINE_EVENT_RESOLUTION_EXACT_EVENT:
            return "exact-event";
        case MACHINE_EVENT_RESOLUTION_PREVIEW_EVENT:
            return "preview-event";
        case MACHINE_EVENT_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_EVENT_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_event_kind_name(MachineEventKind event_kind) {
    switch (event_kind) {
        case MACHINE_EVENT_KIND_NONE:
            return "none";
        case MACHINE_EVENT_KIND_CONTROL_HALT:
            return "control-halt";
        case MACHINE_EVENT_KIND_REGISTER_RESULT:
            return "register-result";
        case MACHINE_EVENT_KIND_LOCAL_STORE:
            return "local-store";
        case MACHINE_EVENT_KIND_GLOBAL_STORE:
            return "global-store";
        case MACHINE_EVENT_KIND_CALL_EFFECT:
            return "call-effect";
        case MACHINE_EVENT_KIND_BLOCKED_CONTROL:
            return "blocked-control";
        case MACHINE_EVENT_KIND_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

int machine_event_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineEventTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->surfaces_exact_event = 1;
    out_summary->surfaces_preview_event = 1;
    out_summary->classifies_event_family = 1;
    return 1;
}

int machine_event_file_get_target_policy_summary(const MachineEventFile *event_file,
    MachineEventTargetPolicySummary *out_summary) {
    if (!event_file || !out_summary) {
        return 0;
    }
    return machine_event_get_target_policy_summary(
        event_file->trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_event_file_get_summary(const MachineEventFile *event_file,
    size_t *out_mapped_byte_count) {
    if (!event_file) {
        return 0;
    }
    return machine_trace_file_get_summary(&event_file->trace_file, out_mapped_byte_count);
}

int machine_event_file_get_source_elf_artifact_summary(const MachineEventFile *event_file,
    MachineElfArtifactSummary *out_summary) {
    if (!event_file || !out_summary) {
        return 0;
    }
    return machine_trace_file_get_source_elf_artifact_summary(&event_file->trace_file, out_summary);
}

int machine_event_file_get_header_summary(const MachineEventFile *event_file,
    MachineEventHeaderSummary *out_summary) {
    MachineTraceHeaderSummary trace_header_summary;

    memset(&trace_header_summary, 0, sizeof(trace_header_summary));
    if (!event_file || !out_summary ||
        !machine_trace_file_get_header_summary(&event_file->trace_file, &trace_header_summary)) {
        return 0;
    }
    out_summary->target_profile = trace_header_summary.target_profile;
    out_summary->trace_resolution_kind = event_file->trace_file.resolution_kind;
    out_summary->origin_step_status = trace_header_summary.origin_step_status;
    out_summary->origin_program_counter = trace_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = trace_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = trace_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = trace_header_summary.mapped_byte_count;
    return 1;
}

int machine_event_file_get_trace_summary(const MachineEventFile *event_file,
    MachineTraceSummary *out_summary) {
    if (!event_file || !out_summary) {
        return 0;
    }
    return machine_trace_file_get_trace_summary(&event_file->trace_file, out_summary);
}

int machine_event_file_get_event_summary(const MachineEventFile *event_file,
    MachineEventSummary *out_summary) {
    MachineTraceSummary trace_summary;

    memset(&trace_summary, 0, sizeof(trace_summary));
    if (!event_file || !out_summary ||
        !machine_event_file_get_trace_summary(event_file, &trace_summary)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->resolution_kind = event_file->resolution_kind;
    out_summary->event_kind = event_file->event_kind;
    out_summary->trace_resolution_kind = trace_summary.resolution_kind;
    out_summary->trace_kind = trace_summary.trace_kind;
    out_summary->trace_change_class = trace_summary.change_class;
    out_summary->delta_resolution_kind = trace_summary.delta_resolution_kind;
    out_summary->delta_kind = trace_summary.delta_kind;
    out_summary->observe_resolution_kind = trace_summary.observe_resolution_kind;
    out_summary->observe_kind = trace_summary.observe_kind;
    out_summary->apply_resolution_kind = trace_summary.apply_resolution_kind;
    out_summary->apply_kind = trace_summary.apply_kind;
    out_summary->commit_resolution_kind = trace_summary.commit_resolution_kind;
    out_summary->commit_kind = trace_summary.commit_kind;
    out_summary->writeback_resolution_kind = trace_summary.writeback_resolution_kind;
    out_summary->writeback_commit_kind = trace_summary.writeback_commit_kind;
    out_summary->mutation_resolution_kind = trace_summary.mutation_resolution_kind;
    out_summary->mutation_effect_kind = trace_summary.mutation_effect_kind;
    out_summary->state_resolution_kind = trace_summary.state_resolution_kind;
    out_summary->transition_resolution_kind = trace_summary.transition_resolution_kind;
    out_summary->action_kind = trace_summary.action_kind;
    out_summary->payload_kind = trace_summary.payload_kind;
    out_summary->tag_class = trace_summary.tag_class;
    out_summary->raw_byte = trace_summary.raw_byte;
    out_summary->tag_value = trace_summary.tag_value;
    out_summary->is_known = trace_summary.is_known;
    out_summary->tag_name = trace_summary.tag_name;
    out_summary->instruction_byte_count = trace_summary.instruction_byte_count;
    out_summary->payload_byte_count = trace_summary.payload_byte_count;
    memcpy(out_summary->payload_bytes, trace_summary.payload_bytes, sizeof(out_summary->payload_bytes));
    out_summary->has_immediate_hint = trace_summary.has_immediate_hint;
    out_summary->immediate_hint = trace_summary.immediate_hint;
    out_summary->is_exact_event = event_file->resolution_kind == MACHINE_EVENT_RESOLUTION_EXACT_EVENT;
    out_summary->origin_status = trace_summary.origin_status;
    out_summary->observed_status = trace_summary.observed_status;
    out_summary->has_observed_state = trace_summary.has_observed_state;
    out_summary->has_status_change = trace_summary.has_status_change;
    out_summary->status_changed = trace_summary.status_changed;
    out_summary->has_program_counter_change = trace_summary.has_program_counter_change;
    out_summary->program_counter_changed = trace_summary.program_counter_changed;
    out_summary->has_stack_pointer_change = trace_summary.has_stack_pointer_change;
    out_summary->stack_pointer_changed = trace_summary.stack_pointer_changed;
    out_summary->has_fetch_change = trace_summary.has_fetch_change;
    out_summary->fetch_changed = trace_summary.fetch_changed;
    out_summary->has_primary_target_block = trace_summary.has_primary_target_block;
    out_summary->primary_target_block_index = trace_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = trace_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = trace_summary.secondary_target_block_index;
    out_summary->has_return_immediate = trace_summary.has_return_immediate;
    out_summary->return_immediate = trace_summary.return_immediate;
    return 1;
}

int machine_event_verify_file(const MachineEventFile *event_file,
    MachineEventError *error) {
    MachineTraceSummary trace_summary;
    MachineEventKind expected_event_kind;

    memset(&trace_summary, 0, sizeof(trace_summary));
    if (!event_file) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-100: invalid event file");
        return 0;
    }
    if (!machine_trace_verify_file(&event_file->trace_file, NULL) ||
        !machine_event_file_get_trace_summary(event_file, &trace_summary)) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-101: invalid trace input");
        return 0;
    }

    expected_event_kind = machine_event_classify_kind(&trace_summary);
    if (event_file->event_kind != expected_event_kind) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-102: invalid event classification");
        return 0;
    }

    switch (event_file->resolution_kind) {
        case MACHINE_EVENT_RESOLUTION_EXACT_EVENT:
            if (event_file->trace_file.resolution_kind != MACHINE_TRACE_RESOLUTION_EXACT_TRACE ||
                expected_event_kind == MACHINE_EVENT_KIND_BLOCKED_CONTROL ||
                expected_event_kind == MACHINE_EVENT_KIND_BLOCKED_UNSUPPORTED ||
                expected_event_kind == MACHINE_EVENT_KIND_NONE) {
                machine_event_set_error(error, 0, 0, "MACHINE-EVENT-103: invalid exact event");
                return 0;
            }
            break;
        case MACHINE_EVENT_RESOLUTION_PREVIEW_EVENT:
            if (event_file->trace_file.resolution_kind != MACHINE_TRACE_RESOLUTION_PREVIEW_TRACE ||
                expected_event_kind == MACHINE_EVENT_KIND_CONTROL_HALT ||
                expected_event_kind == MACHINE_EVENT_KIND_BLOCKED_CONTROL ||
                expected_event_kind == MACHINE_EVENT_KIND_BLOCKED_UNSUPPORTED ||
                expected_event_kind == MACHINE_EVENT_KIND_NONE) {
                machine_event_set_error(error, 0, 0, "MACHINE-EVENT-104: invalid preview event");
                return 0;
            }
            break;
        case MACHINE_EVENT_RESOLUTION_BLOCKED_ON_CONTROL:
            if (event_file->trace_file.resolution_kind != MACHINE_TRACE_RESOLUTION_BLOCKED_ON_CONTROL ||
                expected_event_kind != MACHINE_EVENT_KIND_BLOCKED_CONTROL) {
                machine_event_set_error(error, 0, 0, "MACHINE-EVENT-105: invalid blocked-control event");
                return 0;
            }
            break;
        case MACHINE_EVENT_RESOLUTION_BLOCKED_UNSUPPORTED:
            if (event_file->trace_file.resolution_kind != MACHINE_TRACE_RESOLUTION_BLOCKED_UNSUPPORTED ||
                expected_event_kind != MACHINE_EVENT_KIND_BLOCKED_UNSUPPORTED) {
                machine_event_set_error(error, 0, 0, "MACHINE-EVENT-106: invalid blocked-unsupported event");
                return 0;
            }
            break;
        case MACHINE_EVENT_RESOLUTION_NONE:
        default:
            machine_event_set_error(error, 0, 0, "MACHINE-EVENT-107: invalid event resolution kind");
            return 0;
    }
    return 1;
}

int machine_event_clone_file(const MachineEventFile *source,
    MachineEventFile *out_clone,
    MachineEventError *error) {
    if (!source || !out_clone) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-108: invalid clone contract");
        return 0;
    }
    if (!machine_event_verify_file(source, error)) {
        return 0;
    }

    machine_event_file_free(out_clone);
    if (!machine_trace_clone_file(&source->trace_file, &out_clone->trace_file, NULL)) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-109: failed cloning trace input");
        machine_event_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->event_kind = source->event_kind;
    return 1;
}

int machine_event_build_from_machine_trace_file(const MachineTraceFile *trace_file,
    MachineEventFile *out_event_file,
    MachineEventError *error) {
    MachineTraceSummary trace_summary;

    memset(&trace_summary, 0, sizeof(trace_summary));
    if (!trace_file || !out_event_file) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-110: invalid build contract");
        return 0;
    }
    if (!machine_trace_verify_file(trace_file, NULL) ||
        !machine_trace_file_get_trace_summary(trace_file, &trace_summary)) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-111: invalid trace input");
        return 0;
    }

    machine_event_file_free(out_event_file);
    if (!machine_trace_clone_file(trace_file, &out_event_file->trace_file, NULL)) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-112: failed cloning trace input");
        machine_event_file_free(out_event_file);
        return 0;
    }

    out_event_file->event_kind = machine_event_classify_kind(&trace_summary);
    switch (trace_file->resolution_kind) {
        case MACHINE_TRACE_RESOLUTION_EXACT_TRACE:
            out_event_file->resolution_kind = MACHINE_EVENT_RESOLUTION_EXACT_EVENT;
            break;
        case MACHINE_TRACE_RESOLUTION_PREVIEW_TRACE:
            out_event_file->resolution_kind = MACHINE_EVENT_RESOLUTION_PREVIEW_EVENT;
            break;
        case MACHINE_TRACE_RESOLUTION_BLOCKED_ON_CONTROL:
            out_event_file->resolution_kind = MACHINE_EVENT_RESOLUTION_BLOCKED_ON_CONTROL;
            break;
        case MACHINE_TRACE_RESOLUTION_BLOCKED_UNSUPPORTED:
            out_event_file->resolution_kind = MACHINE_EVENT_RESOLUTION_BLOCKED_UNSUPPORTED;
            break;
        case MACHINE_TRACE_RESOLUTION_NONE:
        default:
            machine_event_set_error(error, 0, 0, "MACHINE-EVENT-113: unsupported trace resolution");
            machine_event_file_free(out_event_file);
            return 0;
    }

    if (!machine_event_verify_file(out_event_file, error)) {
        machine_event_file_free(out_event_file);
        return 0;
    }
    return 1;
}

int machine_event_build_from_machine_trace_report(const MachineTraceReport *report,
    MachineEventFile *out_event_file,
    MachineEventError *error) {
    const MachineTraceFile *trace_file = NULL;

    if (!report) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-114: invalid build-from-trace-report contract");
        return 0;
    }
    if (!machine_trace_report_get_file(report, &trace_file) || !trace_file) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-115: failed recovering trace file from report");
        return 0;
    }
    return machine_event_build_from_machine_trace_file(trace_file, out_event_file, error);
}

#define MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineEventFile *out_event_file, MachineEventError *error) { \
    MachineTraceFile trace_file; \
    int ok; \
    machine_trace_file_init(&trace_file); \
    ok = builder(source, &trace_file, NULL) && \
        machine_event_build_from_machine_trace_file(&trace_file, out_event_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-116: failed building event wrapper"); \
    } \
    machine_trace_file_free(&trace_file); \
    return ok; \
}

MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(machine_event_build_from_machine_delta_file,
    MachineDeltaFile, machine_trace_build_from_machine_delta_file)
MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(machine_event_build_from_machine_delta_report,
    MachineDeltaReport, machine_trace_build_from_machine_delta_report)
MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(machine_event_build_from_machine_observe_file,
    MachineObserveFile, machine_trace_build_from_machine_observe_file)
MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(machine_event_build_from_machine_observe_report,
    MachineObserveReport, machine_trace_build_from_machine_observe_report)
MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(machine_event_build_from_machine_apply_file,
    MachineApplyFile, machine_trace_build_from_machine_apply_file)
MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(machine_event_build_from_machine_apply_report,
    MachineApplyReport, machine_trace_build_from_machine_apply_report)
MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(machine_event_build_from_machine_commit_file,
    MachineCommitFile, machine_trace_build_from_machine_commit_file)
MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(machine_event_build_from_machine_commit_report,
    MachineCommitReport, machine_trace_build_from_machine_commit_report)
MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(machine_event_build_from_machine_step_file,
    MachineStepFile, machine_trace_build_from_machine_step_file)
MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(machine_event_build_from_machine_step_report,
    MachineStepReport, machine_trace_build_from_machine_step_report)
MACHINE_EVENT_DEFINE_BUILD_FROM_WRAPPER(machine_event_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_trace_build_from_machine_ir_report)

int machine_event_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineEventFile *out_event_file,
    MachineEventError *error) {
    MachineTraceFile trace_file;
    int ok;

    machine_trace_file_init(&trace_file);
    ok = machine_trace_build_from_machine_ir_report_with_profile(report, profile, &trace_file, NULL) &&
        machine_event_build_from_machine_trace_file(&trace_file, out_event_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-117: failed building profiled event wrapper");
    }
    machine_trace_file_free(&trace_file);
    return ok;
}

int machine_event_build_report_from_file(const MachineEventFile *source,
    MachineEventReport *out_report,
    MachineEventError *error) {
    if (!source || !out_report) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-118: invalid report build contract");
        return 0;
    }
    if (!machine_event_verify_file(source, error)) {
        return 0;
    }

    machine_event_report_free(out_report);
    if (!machine_event_clone_file(source, &out_report->file, error)) {
        machine_event_report_free(out_report);
        return 0;
    }
    if (!machine_event_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_event_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_trace_build_report_from_file(&out_report->file.trace_file, &out_report->trace_report, NULL) ||
        !machine_event_file_get_event_summary(&out_report->file, &out_report->event_summary)) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-119: failed summarizing event report");
        machine_event_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineEventReport *out_report, MachineEventError *error) { \
    MachineEventFile event_file; \
    int ok; \
    machine_event_file_init(&event_file); \
    ok = builder(source, &event_file, error) && \
        machine_event_build_report_from_file(&event_file, out_report, error); \
    machine_event_file_free(&event_file); \
    return ok; \
}

MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_trace_file,
    MachineTraceFile, machine_event_build_from_machine_trace_file)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_trace_report,
    MachineTraceReport, machine_event_build_from_machine_trace_report)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_delta_file,
    MachineDeltaFile, machine_event_build_from_machine_delta_file)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_delta_report,
    MachineDeltaReport, machine_event_build_from_machine_delta_report)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_observe_file,
    MachineObserveFile, machine_event_build_from_machine_observe_file)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_observe_report,
    MachineObserveReport, machine_event_build_from_machine_observe_report)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_apply_file,
    MachineApplyFile, machine_event_build_from_machine_apply_file)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_apply_report,
    MachineApplyReport, machine_event_build_from_machine_apply_report)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_commit_file,
    MachineCommitFile, machine_event_build_from_machine_commit_file)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_commit_report,
    MachineCommitReport, machine_event_build_from_machine_commit_report)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_step_file,
    MachineStepFile, machine_event_build_from_machine_step_file)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_step_report,
    MachineStepReport, machine_event_build_from_machine_step_report)
MACHINE_EVENT_DEFINE_REPORT_FROM_WRAPPER(machine_event_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_event_build_from_machine_ir_report)

int machine_event_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineEventReport *out_report,
    MachineEventError *error) {
    MachineEventFile event_file;
    int ok;

    machine_event_file_init(&event_file);
    ok = machine_event_build_from_machine_ir_report_with_profile(report, profile, &event_file, error) &&
        machine_event_build_report_from_file(&event_file, out_report, error);
    machine_event_file_free(&event_file);
    return ok;
}

int machine_event_report_refresh(MachineEventReport *report,
    MachineEventError *error) {
    MachineEventReport refreshed_report;
    int ok;

    if (!report) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-120: invalid report refresh contract");
        return 0;
    }
    machine_event_report_init(&refreshed_report);
    ok = machine_event_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_event_report_free(report);
        *report = refreshed_report;
    } else {
        machine_event_report_free(&refreshed_report);
    }
    return ok;
}

int machine_event_dump_file(const MachineEventFile *event_file,
    char **out_text,
    MachineEventError *error) {
    MachineEventStringBuilder builder;
    MachineEventHeaderSummary header_summary;
    MachineEventSummary event_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&event_summary, 0, sizeof(event_summary));
    if (!event_file || !out_text ||
        !machine_event_verify_file(event_file, error) ||
        !machine_event_file_get_header_summary(event_file, &header_summary) ||
        !machine_event_file_get_event_summary(event_file, &event_summary)) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-121: invalid dump contract");
        return 0;
    }

    if (!machine_event_append_format(
            &builder,
            "machine_event profile=%s elf_origin=%s elf_semantics=%s trace=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_elf_target_profile_name(
                event_file->trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file
                    .state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file
                    .runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                event_file->trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file
                    .state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file
                    .runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_trace_resolution_kind_name(header_summary.trace_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_event_append_format(
            &builder,
            "event: resolution=%s kind=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_event_resolution_kind_name(event_summary.resolution_kind),
            machine_event_kind_name(event_summary.event_kind),
            machine_trace_resolution_kind_name(event_summary.trace_resolution_kind),
            machine_trace_change_class_name(event_summary.trace_change_class),
            machine_apply_resolution_kind_name(event_summary.apply_resolution_kind),
            machine_commit_resolution_kind_name(event_summary.commit_resolution_kind),
            machine_writeback_resolution_kind_name(event_summary.writeback_resolution_kind),
            machine_mutation_resolution_kind_name(event_summary.mutation_resolution_kind),
            machine_mutation_effect_kind_name(event_summary.mutation_effect_kind),
            machine_transition_resolution_kind_name(event_summary.transition_resolution_kind),
            machine_interp_action_kind_name(event_summary.action_kind),
            (unsigned int)event_summary.raw_byte,
            (unsigned int)event_summary.tag_value,
            event_summary.is_known ? "yes" : "no",
            event_summary.tag_name ? event_summary.tag_name : "-",
            event_summary.instruction_byte_count) ||
        !machine_event_append_payload_bytes(&builder, &event_summary) ||
        !machine_event_append_immediate_hint(&builder, &event_summary) ||
        !machine_event_append_format(
            &builder,
            " exact=%s origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            event_summary.is_exact_event ? "yes" : "no",
            machine_step_status_name(event_summary.origin_status),
            event_summary.has_observed_state ? machine_step_status_name(event_summary.observed_status) : "-",
            event_summary.has_status_change ? (event_summary.status_changed ? "yes" : "no") : "-",
            event_summary.has_program_counter_change ? (event_summary.program_counter_changed ? "yes" : "no") : "-",
            event_summary.has_stack_pointer_change ? (event_summary.stack_pointer_changed ? "yes" : "no") : "-",
            event_summary.has_fetch_change ? (event_summary.fetch_changed ? "yes" : "no") : "-") ||
        !machine_event_append_targets(&builder, &event_summary) ||
        !machine_event_append_return_immediate(&builder, &event_summary) ||
        !machine_event_append_format(&builder, "\n")) {
        free(builder.data);
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-122: out of memory building dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_event_build_dump_from_file(const MachineEventFile *source,
    char **out_text,
    MachineEventError *error) {
    return machine_event_dump_file(source, out_text, error);
}

#define MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineEventError *error) { \
    MachineEventFile event_file; \
    int ok; \
    machine_event_file_init(&event_file); \
    ok = builder(source, &event_file, error) && \
        machine_event_dump_file(&event_file, out_text, error); \
    machine_event_file_free(&event_file); \
    return ok; \
}

MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_trace_file,
    MachineTraceFile, machine_event_build_from_machine_trace_file)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_trace_report,
    MachineTraceReport, machine_event_build_from_machine_trace_report)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_delta_file,
    MachineDeltaFile, machine_event_build_from_machine_delta_file)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_delta_report,
    MachineDeltaReport, machine_event_build_from_machine_delta_report)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_observe_file,
    MachineObserveFile, machine_event_build_from_machine_observe_file)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_observe_report,
    MachineObserveReport, machine_event_build_from_machine_observe_report)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_apply_file,
    MachineApplyFile, machine_event_build_from_machine_apply_file)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_apply_report,
    MachineApplyReport, machine_event_build_from_machine_apply_report)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_commit_file,
    MachineCommitFile, machine_event_build_from_machine_commit_file)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_commit_report,
    MachineCommitReport, machine_event_build_from_machine_commit_report)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_step_file,
    MachineStepFile, machine_event_build_from_machine_step_file)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_step_report,
    MachineStepReport, machine_event_build_from_machine_step_report)
MACHINE_EVENT_DEFINE_DUMP_FROM_WRAPPER(machine_event_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_event_build_from_machine_ir_report)

int machine_event_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineEventError *error) {
    MachineEventFile event_file;
    int ok;

    machine_event_file_init(&event_file);
    ok = machine_event_build_from_machine_ir_report_with_profile(report, profile, &event_file, error) &&
        machine_event_dump_file(&event_file, out_text, error);
    machine_event_file_free(&event_file);
    return ok;
}

int machine_event_report_get_summary(const MachineEventReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_event_report_get_overview_artifact(const MachineEventReport *report,
    MachineEventReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->trace_report = &report->trace_report;
    out_artifact->event_summary = &report->event_summary;
    return 1;
}

int machine_event_report_get_file(const MachineEventReport *report,
    const MachineEventFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_event_report_get_trace_file(const MachineEventReport *report,
    const MachineTraceFile **out_trace_file) {
    if (!report || !out_trace_file) {
        return 0;
    }
    *out_trace_file = &report->file.trace_file;
    return 1;
}

int machine_event_report_get_trace_report(const MachineEventReport *report,
    const MachineTraceReport **out_trace_report) {
    if (!report || !out_trace_report) {
        return 0;
    }
    *out_trace_report = &report->trace_report;
    return 1;
}

int machine_event_report_get_source_elf_artifact_summary_artifact(
    const MachineEventReport *report,
    const MachineElfArtifactSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file
                        .mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file
                        .step_file.launch_file.runtime_file.load_file.exec_file.image_file
                        .source_elf_artifact_summary;
    return 1;
}

#define MACHINE_EVENT_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineEventReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_EVENT_DEFINE_REPORT_ARTIFACT_GETTER(machine_event_report_get_header_summary_artifact,
    header_summary, MachineEventHeaderSummary)
MACHINE_EVENT_DEFINE_REPORT_ARTIFACT_GETTER(machine_event_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineEventTargetPolicySummary)
MACHINE_EVENT_DEFINE_REPORT_ARTIFACT_GETTER(machine_event_report_get_event_summary_artifact,
    event_summary, MachineEventSummary)

int machine_event_report_overview_artifact_get_trace_report(
    const MachineEventReportOverviewArtifact *artifact,
    const MachineTraceReport **out_trace_report) {
    if (!artifact || !out_trace_report) {
        return 0;
    }
    *out_trace_report = artifact->trace_report;
    return 1;
}

int machine_event_report_overview_artifact_get_event_summary_artifact(
    const MachineEventReportOverviewArtifact *artifact,
    const MachineEventSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->event_summary;
    return 1;
}

int machine_event_dump_report(const MachineEventReport *report,
    char **out_text,
    MachineEventError *error) {
    MachineEventStringBuilder builder;
    const MachineEventSummary *event_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_event_report_get_event_summary_artifact(report, &event_summary) ||
        !event_summary) {
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-123: invalid report dump contract");
        return 0;
    }

    if (!machine_event_append_format(
            &builder,
            "machine_event profile=%s elf_origin=%s elf_semantics=%s trace=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file
                    .state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file
                    .runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file
                    .state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file
                    .runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_trace_resolution_kind_name(report->header_summary.trace_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_event_append_format(
            &builder,
            "event: resolution=%s kind=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_event_resolution_kind_name(event_summary->resolution_kind),
            machine_event_kind_name(event_summary->event_kind),
            machine_trace_resolution_kind_name(event_summary->trace_resolution_kind),
            machine_trace_change_class_name(event_summary->trace_change_class),
            machine_apply_resolution_kind_name(event_summary->apply_resolution_kind),
            machine_commit_resolution_kind_name(event_summary->commit_resolution_kind),
            machine_writeback_resolution_kind_name(event_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(event_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(event_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(event_summary->transition_resolution_kind),
            machine_interp_action_kind_name(event_summary->action_kind),
            (unsigned int)event_summary->raw_byte,
            (unsigned int)event_summary->tag_value,
            event_summary->is_known ? "yes" : "no",
            event_summary->tag_name ? event_summary->tag_name : "-",
            event_summary->instruction_byte_count) ||
        !machine_event_append_payload_bytes(&builder, event_summary) ||
        !machine_event_append_immediate_hint(&builder, event_summary) ||
        !machine_event_append_format(
            &builder,
            " exact=%s origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            event_summary->is_exact_event ? "yes" : "no",
            machine_step_status_name(event_summary->origin_status),
            event_summary->has_observed_state ? machine_step_status_name(event_summary->observed_status) : "-",
            event_summary->has_status_change ? (event_summary->status_changed ? "yes" : "no") : "-",
            event_summary->has_program_counter_change ? (event_summary->program_counter_changed ? "yes" : "no") : "-",
            event_summary->has_stack_pointer_change ? (event_summary->stack_pointer_changed ? "yes" : "no") : "-",
            event_summary->has_fetch_change ? (event_summary->fetch_changed ? "yes" : "no") : "-") ||
        !machine_event_append_targets(&builder, event_summary) ||
        !machine_event_append_return_immediate(&builder, event_summary) ||
        !machine_event_append_format(&builder, "\nreport_overview:\n") ||
        !machine_event_append_format(
            &builder,
            "  origin: trace=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_trace_resolution_kind_name(report->header_summary.trace_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer) ||
        !machine_event_append_format(
            &builder,
            "  elf_source: target=%s origin=%s semantics=%s\n",
            machine_elf_target_profile_name(
                report->file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file
                    .state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file
                    .runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file
                    .state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file
                    .runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file
                    .state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file
                    .runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics)) ||
        !machine_event_append_format(
            &builder,
            "  policy: profile=%s exact=%s preview=%s family=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.surfaces_exact_event ? "yes" : "no",
            report->target_policy_summary.surfaces_preview_event ? "yes" : "no",
            report->target_policy_summary.classifies_event_family ? "yes" : "no") ||
        !machine_event_append_format(
            &builder,
            "  event: resolution=%s kind=%s class=%s exact=%s state=%s status=%s pc=%s fetch=%s",
            machine_event_resolution_kind_name(event_summary->resolution_kind),
            machine_event_kind_name(event_summary->event_kind),
            machine_trace_change_class_name(event_summary->trace_change_class),
            event_summary->is_exact_event ? "yes" : "no",
            event_summary->has_observed_state ? "yes" : "no",
            event_summary->has_status_change ? (event_summary->status_changed ? "yes" : "no") : "-",
            event_summary->has_program_counter_change ? (event_summary->program_counter_changed ? "yes" : "no") : "-",
            event_summary->has_fetch_change ? (event_summary->fetch_changed ? "yes" : "no") : "-")) {
        free(builder.data);
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-124: out of memory building report dump");
        return 0;
    }
    if (!machine_event_append_targets(&builder, event_summary) ||
        !machine_event_append_return_immediate(&builder, event_summary) ||
        !machine_event_append_format(&builder, "\n")) {
        free(builder.data);
        machine_event_set_error(error, 0, 0, "MACHINE-EVENT-125: out of memory terminating report dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_event_build_report_dump_from_file(const MachineEventFile *source,
    char **out_text,
    MachineEventError *error) {
    MachineEventReport report;
    int ok;

    machine_event_report_init(&report);
    ok = machine_event_build_report_from_file(source, &report, error) &&
        machine_event_dump_report(&report, out_text, error);
    machine_event_report_free(&report);
    return ok;
}

#define MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineEventError *error) { \
    MachineEventReport report; \
    int ok; \
    machine_event_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_event_dump_report(&report, out_text, error); \
    machine_event_report_free(&report); \
    return ok; \
}

MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_trace_file,
    MachineTraceFile, machine_event_build_report_from_machine_trace_file)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_trace_report,
    MachineTraceReport, machine_event_build_report_from_machine_trace_report)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_delta_file,
    MachineDeltaFile, machine_event_build_report_from_machine_delta_file)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_delta_report,
    MachineDeltaReport, machine_event_build_report_from_machine_delta_report)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_observe_file,
    MachineObserveFile, machine_event_build_report_from_machine_observe_file)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_observe_report,
    MachineObserveReport, machine_event_build_report_from_machine_observe_report)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_apply_file,
    MachineApplyFile, machine_event_build_report_from_machine_apply_file)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_apply_report,
    MachineApplyReport, machine_event_build_report_from_machine_apply_report)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_commit_file,
    MachineCommitFile, machine_event_build_report_from_machine_commit_file)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_commit_report,
    MachineCommitReport, machine_event_build_report_from_machine_commit_report)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_event_build_report_from_machine_step_file)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_event_build_report_from_machine_step_report)
MACHINE_EVENT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_event_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_event_build_report_from_machine_ir_report)

int machine_event_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineEventError *error) {
    MachineEventReport report_file;
    int ok;

    machine_event_report_init(&report_file);
    ok = machine_event_build_report_from_machine_ir_report_with_profile(report, profile, &report_file, error) &&
        machine_event_dump_report(&report_file, out_text, error);
    machine_event_report_free(&report_file);
    return ok;
}
