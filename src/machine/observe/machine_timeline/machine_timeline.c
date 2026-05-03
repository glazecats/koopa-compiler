#include "machine/timeline.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineTimelineStringBuilder;

static void machine_timeline_set_error(
    MachineTimelineError *error,
    int line,
    int column,
    const char *fmt,
    ...);
static int machine_timeline_append_format(
    MachineTimelineStringBuilder *builder,
    const char *fmt,
    ...);
static int machine_timeline_append_targets(
    MachineTimelineStringBuilder *builder,
    const MachineTimelineSummary *timeline_summary);
static int machine_timeline_append_return_immediate(
    MachineTimelineStringBuilder *builder,
    const MachineTimelineSummary *timeline_summary);
static int machine_timeline_append_payload_bytes(
    MachineTimelineStringBuilder *builder,
    const MachineTimelineSummary *timeline_summary);
static int machine_timeline_append_immediate_hint(
    MachineTimelineStringBuilder *builder,
    const MachineTimelineSummary *timeline_summary);
static MachineTimelineKind machine_timeline_classify_kind(
    const MachineHistorySummary *history_summary);

static void machine_timeline_set_error(
    MachineTimelineError *error,
    int line,
    int column,
    const char *fmt,
    ...) {
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

static int machine_timeline_append_format(
    MachineTimelineStringBuilder *builder,
    const char *fmt,
    ...) {
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

static int machine_timeline_append_targets(
    MachineTimelineStringBuilder *builder,
    const MachineTimelineSummary *timeline_summary) {
    if (!builder || !timeline_summary) {
        return 0;
    }
    if (!timeline_summary->has_primary_target_block) {
        return machine_timeline_append_format(builder, " targets=[]");
    }
    if (!timeline_summary->has_secondary_target_block) {
        return machine_timeline_append_format(
            builder, " targets=[%zu]", timeline_summary->primary_target_block_index);
    }
    return machine_timeline_append_format(
        builder,
        " targets=[%zu,%zu]",
        timeline_summary->primary_target_block_index,
        timeline_summary->secondary_target_block_index);
}

static int machine_timeline_append_return_immediate(
    MachineTimelineStringBuilder *builder,
    const MachineTimelineSummary *timeline_summary) {
    if (!builder || !timeline_summary) {
        return 0;
    }
    if (!timeline_summary->has_return_immediate) {
        return machine_timeline_append_format(builder, " return-imm=-");
    }
    return machine_timeline_append_format(builder, " return-imm=%zu", timeline_summary->return_immediate);
}

static int machine_timeline_append_payload_bytes(
    MachineTimelineStringBuilder *builder,
    const MachineTimelineSummary *timeline_summary) {
    size_t index;

    if (!builder || !timeline_summary) {
        return 0;
    }
    if (!machine_timeline_append_format(builder, " payload=[")) {
        return 0;
    }
    for (index = 0u; index < timeline_summary->payload_byte_count; ++index) {
        if (!machine_timeline_append_format(
                builder,
                "%s0x%02x",
                index == 0u ? "" : ",",
                (unsigned int)timeline_summary->payload_bytes[index])) {
            return 0;
        }
    }
    return machine_timeline_append_format(builder, "]");
}

static int machine_timeline_append_immediate_hint(
    MachineTimelineStringBuilder *builder,
    const MachineTimelineSummary *timeline_summary) {
    if (!builder || !timeline_summary) {
        return 0;
    }
    if (!timeline_summary->has_immediate_hint) {
        return machine_timeline_append_format(builder, " imm=-");
    }
    return machine_timeline_append_format(builder, " imm=%zu", timeline_summary->immediate_hint);
}

static MachineTimelineKind machine_timeline_classify_kind(
    const MachineHistorySummary *history_summary) {
    if (!history_summary) {
        return MACHINE_TIMELINE_KIND_NONE;
    }
    if (history_summary->resolution_kind == MACHINE_HISTORY_RESOLUTION_BLOCKED_ON_CONTROL) {
        return MACHINE_TIMELINE_KIND_BLOCKED_CONTROL_TIMELINE;
    }
    if (history_summary->resolution_kind == MACHINE_HISTORY_RESOLUTION_BLOCKED_UNSUPPORTED) {
        return MACHINE_TIMELINE_KIND_BLOCKED_UNSUPPORTED_TIMELINE;
    }
    switch (history_summary->history_kind) {
        case MACHINE_HISTORY_KIND_PROGRAM_STOP_HISTORY:
            return MACHINE_TIMELINE_KIND_PROGRAM_STOP_TIMELINE;
        case MACHINE_HISTORY_KIND_VALUE_HISTORY:
            return MACHINE_TIMELINE_KIND_VALUE_TIMELINE;
        case MACHINE_HISTORY_KIND_LOCAL_UPDATE_HISTORY:
            return MACHINE_TIMELINE_KIND_LOCAL_UPDATE_TIMELINE;
        case MACHINE_HISTORY_KIND_GLOBAL_UPDATE_HISTORY:
            return MACHINE_TIMELINE_KIND_GLOBAL_UPDATE_TIMELINE;
        case MACHINE_HISTORY_KIND_CALL_HISTORY:
            return MACHINE_TIMELINE_KIND_CALL_TIMELINE;
        case MACHINE_HISTORY_KIND_BLOCKED_CONTROL_HISTORY:
            return MACHINE_TIMELINE_KIND_BLOCKED_CONTROL_TIMELINE;
        case MACHINE_HISTORY_KIND_BLOCKED_UNSUPPORTED_HISTORY:
            return MACHINE_TIMELINE_KIND_BLOCKED_UNSUPPORTED_TIMELINE;
        case MACHINE_HISTORY_KIND_NONE:
        default:
            return MACHINE_TIMELINE_KIND_NONE;
    }
}

void machine_timeline_file_init(MachineTimelineFile *timeline_file) {
    if (!timeline_file) {
        return;
    }
    machine_history_file_init(&timeline_file->history_file);
    timeline_file->resolution_kind = MACHINE_TIMELINE_RESOLUTION_NONE;
    timeline_file->timeline_kind = MACHINE_TIMELINE_KIND_NONE;
    timeline_file->timeline_entry_count = 0u;
    timeline_file->timeline_entry_index = 0u;
}

void machine_timeline_file_free(MachineTimelineFile *timeline_file) {
    if (!timeline_file) {
        return;
    }
    machine_history_file_free(&timeline_file->history_file);
    machine_timeline_file_init(timeline_file);
}

void machine_timeline_report_init(MachineTimelineReport *report) {
    if (!report) {
        return;
    }
    machine_timeline_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_history_report_init(&report->history_report);
    memset(&report->timeline_summary, 0, sizeof(report->timeline_summary));
}

void machine_timeline_report_free(MachineTimelineReport *report) {
    if (!report) {
        return;
    }
    machine_history_report_free(&report->history_report);
    machine_timeline_file_free(&report->file);
    machine_timeline_report_init(report);
}

const char *machine_timeline_resolution_kind_name(MachineTimelineResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_TIMELINE_RESOLUTION_NONE:
            return "none";
        case MACHINE_TIMELINE_RESOLUTION_EXACT_TIMELINE:
            return "exact-timeline";
        case MACHINE_TIMELINE_RESOLUTION_PREVIEW_TIMELINE:
            return "preview-timeline";
        case MACHINE_TIMELINE_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_TIMELINE_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_timeline_kind_name(MachineTimelineKind timeline_kind) {
    switch (timeline_kind) {
        case MACHINE_TIMELINE_KIND_NONE:
            return "none";
        case MACHINE_TIMELINE_KIND_PROGRAM_STOP_TIMELINE:
            return "program-stop-timeline";
        case MACHINE_TIMELINE_KIND_VALUE_TIMELINE:
            return "value-timeline";
        case MACHINE_TIMELINE_KIND_LOCAL_UPDATE_TIMELINE:
            return "local-update-timeline";
        case MACHINE_TIMELINE_KIND_GLOBAL_UPDATE_TIMELINE:
            return "global-update-timeline";
        case MACHINE_TIMELINE_KIND_CALL_TIMELINE:
            return "call-timeline";
        case MACHINE_TIMELINE_KIND_BLOCKED_CONTROL_TIMELINE:
            return "blocked-control-timeline";
        case MACHINE_TIMELINE_KIND_BLOCKED_UNSUPPORTED_TIMELINE:
            return "blocked-unsupported-timeline";
    }
    return "unknown";
}

int machine_timeline_get_target_policy_summary(
    MachineElfTargetProfile profile,
    MachineTimelineTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->surfaces_exact_timeline = 1;
    out_summary->surfaces_preview_timeline = 1;
    out_summary->surfaces_single_tick_timeline = 1;
    return 1;
}

int machine_timeline_file_get_target_policy_summary(
    const MachineTimelineFile *timeline_file,
    MachineTimelineTargetPolicySummary *out_summary) {
    if (!timeline_file || !out_summary) {
        return 0;
    }
    return machine_timeline_get_target_policy_summary(
        timeline_file->history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_timeline_file_get_summary(
    const MachineTimelineFile *timeline_file,
    size_t *out_mapped_byte_count) {
    if (!timeline_file) {
        return 0;
    }
    return machine_history_file_get_summary(&timeline_file->history_file, out_mapped_byte_count);
}

int machine_timeline_file_get_source_elf_artifact_summary(
    const MachineTimelineFile *timeline_file,
    MachineElfArtifactSummary *out_summary) {
    if (!timeline_file || !out_summary) {
        return 0;
    }
    return machine_history_file_get_source_elf_artifact_summary(&timeline_file->history_file, out_summary);
}

int machine_timeline_file_get_header_summary(
    const MachineTimelineFile *timeline_file,
    MachineTimelineHeaderSummary *out_summary) {
    MachineHistoryHeaderSummary history_header_summary;

    memset(&history_header_summary, 0, sizeof(history_header_summary));
    if (!timeline_file || !out_summary ||
        !machine_history_file_get_header_summary(&timeline_file->history_file, &history_header_summary)) {
        return 0;
    }
    out_summary->target_profile = history_header_summary.target_profile;
    out_summary->history_resolution_kind = timeline_file->history_file.resolution_kind;
    out_summary->origin_step_status = history_header_summary.origin_step_status;
    out_summary->origin_program_counter = history_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = history_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = history_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = history_header_summary.mapped_byte_count;
    out_summary->timeline_entry_count = timeline_file->timeline_entry_count;
    out_summary->timeline_entry_index = timeline_file->timeline_entry_index;
    return 1;
}

int machine_timeline_file_get_history_summary(
    const MachineTimelineFile *timeline_file,
    MachineHistorySummary *out_summary) {
    if (!timeline_file || !out_summary) {
        return 0;
    }
    return machine_history_file_get_history_summary(&timeline_file->history_file, out_summary);
}

int machine_timeline_file_get_timeline_summary(
    const MachineTimelineFile *timeline_file,
    MachineTimelineSummary *out_summary) {
    MachineHistorySummary history_summary;

    memset(&history_summary, 0, sizeof(history_summary));
    if (!timeline_file || !out_summary ||
        !machine_timeline_file_get_history_summary(timeline_file, &history_summary)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->resolution_kind = timeline_file->resolution_kind;
    out_summary->timeline_kind = timeline_file->timeline_kind;
    out_summary->history_resolution_kind = history_summary.resolution_kind;
    out_summary->history_kind = history_summary.history_kind;
    out_summary->outcome_resolution_kind = history_summary.outcome_resolution_kind;
    out_summary->outcome_kind = history_summary.outcome_kind;
    out_summary->event_resolution_kind = history_summary.event_resolution_kind;
    out_summary->event_kind = history_summary.event_kind;
    out_summary->trace_resolution_kind = history_summary.trace_resolution_kind;
    out_summary->trace_kind = history_summary.trace_kind;
    out_summary->trace_change_class = history_summary.trace_change_class;
    out_summary->delta_resolution_kind = history_summary.delta_resolution_kind;
    out_summary->delta_kind = history_summary.delta_kind;
    out_summary->observe_resolution_kind = history_summary.observe_resolution_kind;
    out_summary->observe_kind = history_summary.observe_kind;
    out_summary->apply_resolution_kind = history_summary.apply_resolution_kind;
    out_summary->apply_kind = history_summary.apply_kind;
    out_summary->commit_resolution_kind = history_summary.commit_resolution_kind;
    out_summary->commit_kind = history_summary.commit_kind;
    out_summary->writeback_resolution_kind = history_summary.writeback_resolution_kind;
    out_summary->writeback_commit_kind = history_summary.writeback_commit_kind;
    out_summary->mutation_resolution_kind = history_summary.mutation_resolution_kind;
    out_summary->mutation_effect_kind = history_summary.mutation_effect_kind;
    out_summary->state_resolution_kind = history_summary.state_resolution_kind;
    out_summary->transition_resolution_kind = history_summary.transition_resolution_kind;
    out_summary->action_kind = history_summary.action_kind;
    out_summary->payload_kind = history_summary.payload_kind;
    out_summary->tag_class = history_summary.tag_class;
    out_summary->raw_byte = history_summary.raw_byte;
    out_summary->tag_value = history_summary.tag_value;
    out_summary->is_known = history_summary.is_known;
    out_summary->tag_name = history_summary.tag_name;
    out_summary->instruction_byte_count = history_summary.instruction_byte_count;
    out_summary->payload_byte_count = history_summary.payload_byte_count;
    memcpy(out_summary->payload_bytes, history_summary.payload_bytes, sizeof(out_summary->payload_bytes));
    out_summary->has_immediate_hint = history_summary.has_immediate_hint;
    out_summary->immediate_hint = history_summary.immediate_hint;
    out_summary->is_exact_timeline = timeline_file->resolution_kind == MACHINE_TIMELINE_RESOLUTION_EXACT_TIMELINE;
    out_summary->is_single_tick_timeline =
        timeline_file->timeline_entry_count == 1u && timeline_file->timeline_entry_index == 0u;
    out_summary->timeline_entry_count = timeline_file->timeline_entry_count;
    out_summary->timeline_entry_index = timeline_file->timeline_entry_index;
    out_summary->origin_status = history_summary.origin_status;
    out_summary->observed_status = history_summary.observed_status;
    out_summary->has_observed_state = history_summary.has_observed_state;
    out_summary->has_status_change = history_summary.has_status_change;
    out_summary->status_changed = history_summary.status_changed;
    out_summary->has_program_counter_change = history_summary.has_program_counter_change;
    out_summary->program_counter_changed = history_summary.program_counter_changed;
    out_summary->has_stack_pointer_change = history_summary.has_stack_pointer_change;
    out_summary->stack_pointer_changed = history_summary.stack_pointer_changed;
    out_summary->has_fetch_change = history_summary.has_fetch_change;
    out_summary->fetch_changed = history_summary.fetch_changed;
    out_summary->has_primary_target_block = history_summary.has_primary_target_block;
    out_summary->primary_target_block_index = history_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = history_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = history_summary.secondary_target_block_index;
    out_summary->has_return_immediate = history_summary.has_return_immediate;
    out_summary->return_immediate = history_summary.return_immediate;
    return 1;
}

int machine_timeline_verify_file(
    const MachineTimelineFile *timeline_file,
    MachineTimelineError *error) {
    MachineHistorySummary history_summary;
    MachineTimelineKind expected_timeline_kind;

    memset(&history_summary, 0, sizeof(history_summary));
    if (!timeline_file) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-100: invalid timeline file");
        return 0;
    }
    if (!machine_history_verify_file(&timeline_file->history_file, NULL) ||
        !machine_timeline_file_get_history_summary(timeline_file, &history_summary)) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-101: invalid history input");
        return 0;
    }
    if (timeline_file->timeline_entry_count != 1u || timeline_file->timeline_entry_index != 0u) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-102: invalid timeline shape");
        return 0;
    }

    expected_timeline_kind = machine_timeline_classify_kind(&history_summary);
    if (timeline_file->timeline_kind != expected_timeline_kind) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-103: invalid timeline classification");
        return 0;
    }

    switch (timeline_file->resolution_kind) {
        case MACHINE_TIMELINE_RESOLUTION_EXACT_TIMELINE:
            if (timeline_file->history_file.resolution_kind != MACHINE_HISTORY_RESOLUTION_EXACT_HISTORY ||
                expected_timeline_kind == MACHINE_TIMELINE_KIND_BLOCKED_CONTROL_TIMELINE ||
                expected_timeline_kind == MACHINE_TIMELINE_KIND_BLOCKED_UNSUPPORTED_TIMELINE ||
                expected_timeline_kind == MACHINE_TIMELINE_KIND_NONE) {
                machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-104: invalid exact timeline");
                return 0;
            }
            break;
        case MACHINE_TIMELINE_RESOLUTION_PREVIEW_TIMELINE:
            if (timeline_file->history_file.resolution_kind != MACHINE_HISTORY_RESOLUTION_PREVIEW_HISTORY ||
                expected_timeline_kind == MACHINE_TIMELINE_KIND_BLOCKED_CONTROL_TIMELINE ||
                expected_timeline_kind == MACHINE_TIMELINE_KIND_BLOCKED_UNSUPPORTED_TIMELINE ||
                expected_timeline_kind == MACHINE_TIMELINE_KIND_NONE) {
                machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-105: invalid preview timeline");
                return 0;
            }
            break;
        case MACHINE_TIMELINE_RESOLUTION_BLOCKED_ON_CONTROL:
            if (timeline_file->history_file.resolution_kind != MACHINE_HISTORY_RESOLUTION_BLOCKED_ON_CONTROL ||
                expected_timeline_kind != MACHINE_TIMELINE_KIND_BLOCKED_CONTROL_TIMELINE) {
                machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-106: invalid blocked-control timeline");
                return 0;
            }
            break;
        case MACHINE_TIMELINE_RESOLUTION_BLOCKED_UNSUPPORTED:
            if (timeline_file->history_file.resolution_kind != MACHINE_HISTORY_RESOLUTION_BLOCKED_UNSUPPORTED ||
                expected_timeline_kind != MACHINE_TIMELINE_KIND_BLOCKED_UNSUPPORTED_TIMELINE) {
                machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-107: invalid blocked-unsupported timeline");
                return 0;
            }
            break;
        case MACHINE_TIMELINE_RESOLUTION_NONE:
        default:
            machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-108: invalid timeline resolution kind");
            return 0;
    }
    return 1;
}

int machine_timeline_clone_file(
    const MachineTimelineFile *source,
    MachineTimelineFile *out_clone,
    MachineTimelineError *error) {
    if (!source || !out_clone) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-109: invalid clone contract");
        return 0;
    }
    if (!machine_timeline_verify_file(source, error)) {
        return 0;
    }

    machine_timeline_file_free(out_clone);
    if (!machine_history_clone_file(&source->history_file, &out_clone->history_file, NULL)) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-110: failed cloning history input");
        machine_timeline_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->timeline_kind = source->timeline_kind;
    out_clone->timeline_entry_count = source->timeline_entry_count;
    out_clone->timeline_entry_index = source->timeline_entry_index;
    return 1;
}

int machine_timeline_build_from_machine_history_file(
    const MachineHistoryFile *history_file,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error) {
    MachineHistorySummary history_summary;

    memset(&history_summary, 0, sizeof(history_summary));
    if (!history_file || !out_timeline_file) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-111: invalid build contract");
        return 0;
    }
    if (!machine_history_verify_file(history_file, NULL) ||
        !machine_history_file_get_history_summary(history_file, &history_summary)) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-112: invalid history input");
        return 0;
    }

    machine_timeline_file_free(out_timeline_file);
    if (!machine_history_clone_file(history_file, &out_timeline_file->history_file, NULL)) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-113: failed cloning history input");
        machine_timeline_file_free(out_timeline_file);
        return 0;
    }

    out_timeline_file->timeline_kind = machine_timeline_classify_kind(&history_summary);
    out_timeline_file->timeline_entry_count = 1u;
    out_timeline_file->timeline_entry_index = 0u;
    switch (history_file->resolution_kind) {
        case MACHINE_HISTORY_RESOLUTION_EXACT_HISTORY:
            out_timeline_file->resolution_kind = MACHINE_TIMELINE_RESOLUTION_EXACT_TIMELINE;
            break;
        case MACHINE_HISTORY_RESOLUTION_PREVIEW_HISTORY:
            out_timeline_file->resolution_kind = MACHINE_TIMELINE_RESOLUTION_PREVIEW_TIMELINE;
            break;
        case MACHINE_HISTORY_RESOLUTION_BLOCKED_ON_CONTROL:
            out_timeline_file->resolution_kind = MACHINE_TIMELINE_RESOLUTION_BLOCKED_ON_CONTROL;
            break;
        case MACHINE_HISTORY_RESOLUTION_BLOCKED_UNSUPPORTED:
            out_timeline_file->resolution_kind = MACHINE_TIMELINE_RESOLUTION_BLOCKED_UNSUPPORTED;
            break;
        case MACHINE_HISTORY_RESOLUTION_NONE:
        default:
            machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-114: unsupported history resolution");
            machine_timeline_file_free(out_timeline_file);
            return 0;
    }

    if (!machine_timeline_verify_file(out_timeline_file, error)) {
        machine_timeline_file_free(out_timeline_file);
        return 0;
    }
    return 1;
}

int machine_timeline_build_from_machine_history_report(
    const MachineHistoryReport *report,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error) {
    const MachineHistoryFile *history_file = NULL;

    if (!report) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-115: invalid build-from-history-report contract");
        return 0;
    }
    if (!machine_history_report_get_file(report, &history_file) || !history_file) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-116: failed recovering history file from report");
        return 0;
    }
    return machine_timeline_build_from_machine_history_file(history_file, out_timeline_file, error);
}

#define MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineTimelineFile *out_timeline_file, MachineTimelineError *error) { \
    MachineHistoryFile history_file; \
    int ok; \
    machine_history_file_init(&history_file); \
    ok = builder(source, &history_file, NULL) && \
        machine_timeline_build_from_machine_history_file(&history_file, out_timeline_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-117: failed building timeline wrapper"); \
    } \
    machine_history_file_free(&history_file); \
    return ok; \
}

MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_outcome_file,
    MachineOutcomeFile, machine_history_build_from_machine_outcome_file)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_outcome_report,
    MachineOutcomeReport, machine_history_build_from_machine_outcome_report)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_event_file,
    MachineEventFile, machine_history_build_from_machine_event_file)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_event_report,
    MachineEventReport, machine_history_build_from_machine_event_report)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_trace_file,
    MachineTraceFile, machine_history_build_from_machine_trace_file)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_trace_report,
    MachineTraceReport, machine_history_build_from_machine_trace_report)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_delta_file,
    MachineDeltaFile, machine_history_build_from_machine_delta_file)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_delta_report,
    MachineDeltaReport, machine_history_build_from_machine_delta_report)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_observe_file,
    MachineObserveFile, machine_history_build_from_machine_observe_file)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_observe_report,
    MachineObserveReport, machine_history_build_from_machine_observe_report)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_apply_file,
    MachineApplyFile, machine_history_build_from_machine_apply_file)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_apply_report,
    MachineApplyReport, machine_history_build_from_machine_apply_report)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_commit_file,
    MachineCommitFile, machine_history_build_from_machine_commit_file)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_commit_report,
    MachineCommitReport, machine_history_build_from_machine_commit_report)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_step_file,
    MachineStepFile, machine_history_build_from_machine_step_file)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_step_report,
    MachineStepReport, machine_history_build_from_machine_step_report)
MACHINE_TIMELINE_DEFINE_BUILD_FROM_WRAPPER(machine_timeline_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_history_build_from_machine_ir_report)

int machine_timeline_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error) {
    MachineHistoryFile history_file;
    int ok;

    machine_history_file_init(&history_file);
    ok = machine_history_build_from_machine_ir_report_with_profile(report, profile, &history_file, NULL) &&
        machine_timeline_build_from_machine_history_file(&history_file, out_timeline_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-118: failed building profiled timeline wrapper");
    }
    machine_history_file_free(&history_file);
    return ok;
}

int machine_timeline_build_report_from_file(
    const MachineTimelineFile *source,
    MachineTimelineReport *out_report,
    MachineTimelineError *error) {
    if (!source || !out_report) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-119: invalid report build contract");
        return 0;
    }
    if (!machine_timeline_verify_file(source, error)) {
        return 0;
    }

    machine_timeline_report_free(out_report);
    if (!machine_timeline_clone_file(source, &out_report->file, error)) {
        machine_timeline_report_free(out_report);
        return 0;
    }
    if (!machine_timeline_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_timeline_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_history_build_report_from_file(&out_report->file.history_file, &out_report->history_report, NULL) ||
        !machine_timeline_file_get_timeline_summary(&out_report->file, &out_report->timeline_summary)) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-120: failed summarizing timeline report");
        machine_timeline_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineTimelineReport *out_report, MachineTimelineError *error) { \
    MachineTimelineFile timeline_file; \
    int ok; \
    machine_timeline_file_init(&timeline_file); \
    ok = builder(source, &timeline_file, error) && \
        machine_timeline_build_report_from_file(&timeline_file, out_report, error); \
    machine_timeline_file_free(&timeline_file); \
    return ok; \
}

MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_history_file,
    MachineHistoryFile, machine_timeline_build_from_machine_history_file)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_history_report,
    MachineHistoryReport, machine_timeline_build_from_machine_history_report)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_outcome_file,
    MachineOutcomeFile, machine_timeline_build_from_machine_outcome_file)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_outcome_report,
    MachineOutcomeReport, machine_timeline_build_from_machine_outcome_report)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_event_file,
    MachineEventFile, machine_timeline_build_from_machine_event_file)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_event_report,
    MachineEventReport, machine_timeline_build_from_machine_event_report)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_trace_file,
    MachineTraceFile, machine_timeline_build_from_machine_trace_file)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_trace_report,
    MachineTraceReport, machine_timeline_build_from_machine_trace_report)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_delta_file,
    MachineDeltaFile, machine_timeline_build_from_machine_delta_file)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_delta_report,
    MachineDeltaReport, machine_timeline_build_from_machine_delta_report)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_observe_file,
    MachineObserveFile, machine_timeline_build_from_machine_observe_file)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_observe_report,
    MachineObserveReport, machine_timeline_build_from_machine_observe_report)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_apply_file,
    MachineApplyFile, machine_timeline_build_from_machine_apply_file)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_apply_report,
    MachineApplyReport, machine_timeline_build_from_machine_apply_report)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_commit_file,
    MachineCommitFile, machine_timeline_build_from_machine_commit_file)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_commit_report,
    MachineCommitReport, machine_timeline_build_from_machine_commit_report)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_step_file,
    MachineStepFile, machine_timeline_build_from_machine_step_file)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_step_report,
    MachineStepReport, machine_timeline_build_from_machine_step_report)
MACHINE_TIMELINE_DEFINE_REPORT_FROM_WRAPPER(machine_timeline_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_timeline_build_from_machine_ir_report)

int machine_timeline_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTimelineReport *out_report,
    MachineTimelineError *error) {
    MachineTimelineFile timeline_file;
    int ok;

    machine_timeline_file_init(&timeline_file);
    ok = machine_timeline_build_from_machine_ir_report_with_profile(report, profile, &timeline_file, error) &&
        machine_timeline_build_report_from_file(&timeline_file, out_report, error);
    machine_timeline_file_free(&timeline_file);
    return ok;
}

int machine_timeline_report_refresh(
    MachineTimelineReport *report,
    MachineTimelineError *error) {
    MachineTimelineReport refreshed_report;
    int ok;

    if (!report) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-121: invalid report refresh contract");
        return 0;
    }
    machine_timeline_report_init(&refreshed_report);
    ok = machine_timeline_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_timeline_report_free(report);
        *report = refreshed_report;
    } else {
        machine_timeline_report_free(&refreshed_report);
    }
    return ok;
}

int machine_timeline_dump_file(
    const MachineTimelineFile *timeline_file,
    char **out_text,
    MachineTimelineError *error) {
    MachineTimelineStringBuilder builder;
    MachineTimelineHeaderSummary header_summary;
    MachineTimelineSummary timeline_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&timeline_summary, 0, sizeof(timeline_summary));
    if (!timeline_file || !out_text ||
        !machine_timeline_verify_file(timeline_file, error) ||
        !machine_timeline_file_get_header_summary(timeline_file, &header_summary) ||
        !machine_timeline_file_get_timeline_summary(timeline_file, &timeline_summary)) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-122: invalid dump contract");
        return 0;
    }

    if (!machine_timeline_append_format(
            &builder,
            "machine_timeline profile=%s elf_origin=%s elf_semantics=%s history=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu entries=%zu entry-index=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_elf_target_profile_name(
                timeline_file->history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file
                    .commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                timeline_file->history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file
                    .commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.relocation_semantics),
            machine_history_resolution_kind_name(header_summary.history_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count,
            header_summary.timeline_entry_count,
            header_summary.timeline_entry_index) ||
        !machine_timeline_append_format(
            &builder,
            "timeline: resolution=%s kind=%s history=%s outcome=%s event=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_timeline_resolution_kind_name(timeline_summary.resolution_kind),
            machine_timeline_kind_name(timeline_summary.timeline_kind),
            machine_history_kind_name(timeline_summary.history_kind),
            machine_outcome_kind_name(timeline_summary.outcome_kind),
            machine_event_kind_name(timeline_summary.event_kind),
            machine_trace_resolution_kind_name(timeline_summary.trace_resolution_kind),
            machine_trace_change_class_name(timeline_summary.trace_change_class),
            machine_apply_resolution_kind_name(timeline_summary.apply_resolution_kind),
            machine_commit_resolution_kind_name(timeline_summary.commit_resolution_kind),
            machine_writeback_resolution_kind_name(timeline_summary.writeback_resolution_kind),
            machine_mutation_resolution_kind_name(timeline_summary.mutation_resolution_kind),
            machine_mutation_effect_kind_name(timeline_summary.mutation_effect_kind),
            machine_transition_resolution_kind_name(timeline_summary.transition_resolution_kind),
            machine_interp_action_kind_name(timeline_summary.action_kind),
            (unsigned int)timeline_summary.raw_byte,
            (unsigned int)timeline_summary.tag_value,
            timeline_summary.is_known ? "yes" : "no",
            timeline_summary.tag_name ? timeline_summary.tag_name : "-",
            timeline_summary.instruction_byte_count) ||
        !machine_timeline_append_payload_bytes(&builder, &timeline_summary) ||
        !machine_timeline_append_immediate_hint(&builder, &timeline_summary) ||
        !machine_timeline_append_format(
            &builder,
            " exact=%s single-tick=%s entries=%zu entry-index=%zu origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            timeline_summary.is_exact_timeline ? "yes" : "no",
            timeline_summary.is_single_tick_timeline ? "yes" : "no",
            timeline_summary.timeline_entry_count,
            timeline_summary.timeline_entry_index,
            machine_step_status_name(timeline_summary.origin_status),
            timeline_summary.has_observed_state ? machine_step_status_name(timeline_summary.observed_status) : "-",
            timeline_summary.has_status_change ? (timeline_summary.status_changed ? "yes" : "no") : "-",
            timeline_summary.has_program_counter_change ? (timeline_summary.program_counter_changed ? "yes" : "no") : "-",
            timeline_summary.has_stack_pointer_change ? (timeline_summary.stack_pointer_changed ? "yes" : "no") : "-",
            timeline_summary.has_fetch_change ? (timeline_summary.fetch_changed ? "yes" : "no") : "-") ||
        !machine_timeline_append_targets(&builder, &timeline_summary) ||
        !machine_timeline_append_return_immediate(&builder, &timeline_summary) ||
        !machine_timeline_append_format(&builder, "\n")) {
        free(builder.data);
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-123: out of memory building dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_timeline_build_dump_from_file(
    const MachineTimelineFile *source,
    char **out_text,
    MachineTimelineError *error) {
    return machine_timeline_dump_file(source, out_text, error);
}

#define MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineTimelineError *error) { \
    MachineTimelineFile timeline_file; \
    int ok; \
    machine_timeline_file_init(&timeline_file); \
    ok = builder(source, &timeline_file, error) && \
        machine_timeline_dump_file(&timeline_file, out_text, error); \
    machine_timeline_file_free(&timeline_file); \
    return ok; \
}

MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_history_file,
    MachineHistoryFile, machine_timeline_build_from_machine_history_file)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_history_report,
    MachineHistoryReport, machine_timeline_build_from_machine_history_report)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_outcome_file,
    MachineOutcomeFile, machine_timeline_build_from_machine_outcome_file)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_outcome_report,
    MachineOutcomeReport, machine_timeline_build_from_machine_outcome_report)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_event_file,
    MachineEventFile, machine_timeline_build_from_machine_event_file)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_event_report,
    MachineEventReport, machine_timeline_build_from_machine_event_report)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_trace_file,
    MachineTraceFile, machine_timeline_build_from_machine_trace_file)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_trace_report,
    MachineTraceReport, machine_timeline_build_from_machine_trace_report)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_delta_file,
    MachineDeltaFile, machine_timeline_build_from_machine_delta_file)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_delta_report,
    MachineDeltaReport, machine_timeline_build_from_machine_delta_report)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_observe_file,
    MachineObserveFile, machine_timeline_build_from_machine_observe_file)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_observe_report,
    MachineObserveReport, machine_timeline_build_from_machine_observe_report)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_apply_file,
    MachineApplyFile, machine_timeline_build_from_machine_apply_file)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_apply_report,
    MachineApplyReport, machine_timeline_build_from_machine_apply_report)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_commit_file,
    MachineCommitFile, machine_timeline_build_from_machine_commit_file)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_commit_report,
    MachineCommitReport, machine_timeline_build_from_machine_commit_report)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_step_file,
    MachineStepFile, machine_timeline_build_from_machine_step_file)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_step_report,
    MachineStepReport, machine_timeline_build_from_machine_step_report)
MACHINE_TIMELINE_DEFINE_DUMP_FROM_WRAPPER(machine_timeline_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_timeline_build_from_machine_ir_report)

int machine_timeline_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTimelineError *error) {
    MachineTimelineFile timeline_file;
    int ok;

    machine_timeline_file_init(&timeline_file);
    ok = machine_timeline_build_from_machine_ir_report_with_profile(report, profile, &timeline_file, error) &&
        machine_timeline_dump_file(&timeline_file, out_text, error);
    machine_timeline_file_free(&timeline_file);
    return ok;
}

int machine_timeline_report_get_summary(
    const MachineTimelineReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_timeline_report_get_overview_artifact(
    const MachineTimelineReport *report,
    MachineTimelineReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->history_report = &report->history_report;
    out_artifact->timeline_summary = &report->timeline_summary;
    return 1;
}

int machine_timeline_report_get_file(
    const MachineTimelineReport *report,
    const MachineTimelineFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_timeline_report_get_history_file(
    const MachineTimelineReport *report,
    const MachineHistoryFile **out_history_file) {
    if (!report || !out_history_file) {
        return 0;
    }
    *out_history_file = &report->file.history_file;
    return 1;
}

int machine_timeline_report_get_history_report(
    const MachineTimelineReport *report,
    const MachineHistoryReport **out_history_report) {
    if (!report || !out_history_report) {
        return 0;
    }
    *out_history_report = &report->history_report;
    return 1;
}

int machine_timeline_report_get_source_elf_artifact_summary_artifact(
    const MachineTimelineReport *report,
    const MachineElfArtifactSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file
                        .apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file
                        .interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file
                        .load_file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

#define MACHINE_TIMELINE_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineTimelineReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_TIMELINE_DEFINE_REPORT_ARTIFACT_GETTER(machine_timeline_report_get_header_summary_artifact,
    header_summary, MachineTimelineHeaderSummary)
MACHINE_TIMELINE_DEFINE_REPORT_ARTIFACT_GETTER(machine_timeline_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineTimelineTargetPolicySummary)
MACHINE_TIMELINE_DEFINE_REPORT_ARTIFACT_GETTER(machine_timeline_report_get_timeline_summary_artifact,
    timeline_summary, MachineTimelineSummary)

int machine_timeline_report_overview_artifact_get_history_report(
    const MachineTimelineReportOverviewArtifact *artifact,
    const MachineHistoryReport **out_history_report) {
    if (!artifact || !out_history_report) {
        return 0;
    }
    *out_history_report = artifact->history_report;
    return 1;
}

int machine_timeline_report_overview_artifact_get_timeline_summary_artifact(
    const MachineTimelineReportOverviewArtifact *artifact,
    const MachineTimelineSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->timeline_summary;
    return 1;
}

int machine_timeline_dump_report(
    const MachineTimelineReport *report,
    char **out_text,
    MachineTimelineError *error) {
    MachineTimelineStringBuilder builder;
    const MachineTimelineSummary *timeline_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_timeline_report_get_timeline_summary_artifact(report, &timeline_summary) ||
        !timeline_summary) {
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-124: invalid report dump contract");
        return 0;
    }

    if (!machine_timeline_append_format(
            &builder,
            "machine_timeline profile=%s elf_origin=%s elf_semantics=%s history=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu entries=%zu entry-index=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file
                    .commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file
                    .commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.relocation_semantics),
            machine_history_resolution_kind_name(report->header_summary.history_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.timeline_entry_count,
            report->header_summary.timeline_entry_index) ||
        !machine_timeline_append_format(
            &builder,
            "timeline: resolution=%s kind=%s history=%s outcome=%s event=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_timeline_resolution_kind_name(timeline_summary->resolution_kind),
            machine_timeline_kind_name(timeline_summary->timeline_kind),
            machine_history_kind_name(timeline_summary->history_kind),
            machine_outcome_kind_name(timeline_summary->outcome_kind),
            machine_event_kind_name(timeline_summary->event_kind),
            machine_trace_resolution_kind_name(timeline_summary->trace_resolution_kind),
            machine_trace_change_class_name(timeline_summary->trace_change_class),
            machine_apply_resolution_kind_name(timeline_summary->apply_resolution_kind),
            machine_commit_resolution_kind_name(timeline_summary->commit_resolution_kind),
            machine_writeback_resolution_kind_name(timeline_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(timeline_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(timeline_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(timeline_summary->transition_resolution_kind),
            machine_interp_action_kind_name(timeline_summary->action_kind),
            (unsigned int)timeline_summary->raw_byte,
            (unsigned int)timeline_summary->tag_value,
            timeline_summary->is_known ? "yes" : "no",
            timeline_summary->tag_name ? timeline_summary->tag_name : "-",
            timeline_summary->instruction_byte_count) ||
        !machine_timeline_append_payload_bytes(&builder, timeline_summary) ||
        !machine_timeline_append_immediate_hint(&builder, timeline_summary) ||
        !machine_timeline_append_format(
            &builder,
            " exact=%s single-tick=%s entries=%zu entry-index=%zu origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            timeline_summary->is_exact_timeline ? "yes" : "no",
            timeline_summary->is_single_tick_timeline ? "yes" : "no",
            timeline_summary->timeline_entry_count,
            timeline_summary->timeline_entry_index,
            machine_step_status_name(timeline_summary->origin_status),
            timeline_summary->has_observed_state ? machine_step_status_name(timeline_summary->observed_status) : "-",
            timeline_summary->has_status_change ? (timeline_summary->status_changed ? "yes" : "no") : "-",
            timeline_summary->has_program_counter_change ? (timeline_summary->program_counter_changed ? "yes" : "no") : "-",
            timeline_summary->has_stack_pointer_change ? (timeline_summary->stack_pointer_changed ? "yes" : "no") : "-",
            timeline_summary->has_fetch_change ? (timeline_summary->fetch_changed ? "yes" : "no") : "-") ||
        !machine_timeline_append_targets(&builder, timeline_summary) ||
        !machine_timeline_append_return_immediate(&builder, timeline_summary) ||
        !machine_timeline_append_format(&builder, "\nreport_overview:\n") ||
        !machine_timeline_append_format(
            &builder,
            "  origin: history=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx entries=%zu entry-index=%zu\n",
            machine_history_resolution_kind_name(report->header_summary.history_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.timeline_entry_count,
            report->header_summary.timeline_entry_index) ||
        !machine_timeline_append_format(
            &builder,
            "  elf_source: target=%s origin=%s semantics=%s\n",
            machine_elf_target_profile_name(
                report->file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file
                    .commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file
                    .commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file
                    .commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.relocation_semantics)) ||
        !machine_timeline_append_format(
            &builder,
            "  policy: profile=%s exact=%s preview=%s single-tick=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.surfaces_exact_timeline ? "yes" : "no",
            report->target_policy_summary.surfaces_preview_timeline ? "yes" : "no",
            report->target_policy_summary.surfaces_single_tick_timeline ? "yes" : "no") ||
        !machine_timeline_append_format(
            &builder,
            "  timeline: resolution=%s kind=%s history=%s exact=%s single-tick=%s entries=%zu entry-index=%zu state=%s status=%s pc=%s fetch=%s",
            machine_timeline_resolution_kind_name(timeline_summary->resolution_kind),
            machine_timeline_kind_name(timeline_summary->timeline_kind),
            machine_history_kind_name(timeline_summary->history_kind),
            timeline_summary->is_exact_timeline ? "yes" : "no",
            timeline_summary->is_single_tick_timeline ? "yes" : "no",
            timeline_summary->timeline_entry_count,
            timeline_summary->timeline_entry_index,
            timeline_summary->has_observed_state ? "yes" : "no",
            timeline_summary->has_status_change ? (timeline_summary->status_changed ? "yes" : "no") : "-",
            timeline_summary->has_program_counter_change ? (timeline_summary->program_counter_changed ? "yes" : "no") : "-",
            timeline_summary->has_fetch_change ? (timeline_summary->fetch_changed ? "yes" : "no") : "-")) {
        free(builder.data);
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-125: out of memory building report dump");
        return 0;
    }
    if (!machine_timeline_append_targets(&builder, timeline_summary) ||
        !machine_timeline_append_return_immediate(&builder, timeline_summary) ||
        !machine_timeline_append_format(&builder, "\n")) {
        free(builder.data);
        machine_timeline_set_error(error, 0, 0, "MACHINE-TIMELINE-126: out of memory terminating report dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_timeline_build_report_dump_from_file(
    const MachineTimelineFile *source,
    char **out_text,
    MachineTimelineError *error) {
    MachineTimelineReport report;
    int ok;

    machine_timeline_report_init(&report);
    ok = machine_timeline_build_report_from_file(source, &report, error) &&
        machine_timeline_dump_report(&report, out_text, error);
    machine_timeline_report_free(&report);
    return ok;
}

#define MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineTimelineError *error) { \
    MachineTimelineReport report; \
    int ok; \
    machine_timeline_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_timeline_dump_report(&report, out_text, error); \
    machine_timeline_report_free(&report); \
    return ok; \
}

MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_history_file,
    MachineHistoryFile, machine_timeline_build_report_from_machine_history_file)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_history_report,
    MachineHistoryReport, machine_timeline_build_report_from_machine_history_report)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_outcome_file,
    MachineOutcomeFile, machine_timeline_build_report_from_machine_outcome_file)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_outcome_report,
    MachineOutcomeReport, machine_timeline_build_report_from_machine_outcome_report)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_event_file,
    MachineEventFile, machine_timeline_build_report_from_machine_event_file)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_event_report,
    MachineEventReport, machine_timeline_build_report_from_machine_event_report)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_trace_file,
    MachineTraceFile, machine_timeline_build_report_from_machine_trace_file)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_trace_report,
    MachineTraceReport, machine_timeline_build_report_from_machine_trace_report)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_delta_file,
    MachineDeltaFile, machine_timeline_build_report_from_machine_delta_file)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_delta_report,
    MachineDeltaReport, machine_timeline_build_report_from_machine_delta_report)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_observe_file,
    MachineObserveFile, machine_timeline_build_report_from_machine_observe_file)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_observe_report,
    MachineObserveReport, machine_timeline_build_report_from_machine_observe_report)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_apply_file,
    MachineApplyFile, machine_timeline_build_report_from_machine_apply_file)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_apply_report,
    MachineApplyReport, machine_timeline_build_report_from_machine_apply_report)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_commit_file,
    MachineCommitFile, machine_timeline_build_report_from_machine_commit_file)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_commit_report,
    MachineCommitReport, machine_timeline_build_report_from_machine_commit_report)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_timeline_build_report_from_machine_step_file)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_timeline_build_report_from_machine_step_report)
MACHINE_TIMELINE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_timeline_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_timeline_build_report_from_machine_ir_report)

int machine_timeline_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTimelineError *error) {
    MachineTimelineReport report_file;
    int ok;

    machine_timeline_report_init(&report_file);
    ok = machine_timeline_build_report_from_machine_ir_report_with_profile(
            report, profile, &report_file, error) &&
        machine_timeline_dump_report(&report_file, out_text, error);
    machine_timeline_report_free(&report_file);
    return ok;
}
