#include "machine/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineLogStringBuilder;

static void machine_log_set_error(
    MachineLogError *error,
    int line,
    int column,
    const char *fmt,
    ...);
static int machine_log_append_format(
    MachineLogStringBuilder *builder,
    const char *fmt,
    ...);
static int machine_log_append_targets(
    MachineLogStringBuilder *builder,
    const MachineLogSummary *log_summary);
static int machine_log_append_return_immediate(
    MachineLogStringBuilder *builder,
    const MachineLogSummary *log_summary);
static int machine_log_append_payload_bytes(
    MachineLogStringBuilder *builder,
    const MachineLogSummary *log_summary);
static int machine_log_append_immediate_hint(
    MachineLogStringBuilder *builder,
    const MachineLogSummary *log_summary);
static MachineLogKind machine_log_classify_kind(
    const MachineTimelineSummary *timeline_summary);

static void machine_log_set_error(
    MachineLogError *error,
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

static int machine_log_append_format(
    MachineLogStringBuilder *builder,
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

static int machine_log_append_targets(
    MachineLogStringBuilder *builder,
    const MachineLogSummary *log_summary) {
    if (!builder || !log_summary) {
        return 0;
    }
    if (!log_summary->has_primary_target_block) {
        return machine_log_append_format(builder, " targets=[]");
    }
    if (!log_summary->has_secondary_target_block) {
        return machine_log_append_format(
            builder, " targets=[%zu]", log_summary->primary_target_block_index);
    }
    return machine_log_append_format(
        builder,
        " targets=[%zu,%zu]",
        log_summary->primary_target_block_index,
        log_summary->secondary_target_block_index);
}

static int machine_log_append_return_immediate(
    MachineLogStringBuilder *builder,
    const MachineLogSummary *log_summary) {
    if (!builder || !log_summary) {
        return 0;
    }
    if (!log_summary->has_return_immediate) {
        return machine_log_append_format(builder, " return-imm=-");
    }
    return machine_log_append_format(builder, " return-imm=%zu", log_summary->return_immediate);
}

static int machine_log_append_payload_bytes(
    MachineLogStringBuilder *builder,
    const MachineLogSummary *log_summary) {
    size_t index;

    if (!builder || !log_summary) {
        return 0;
    }
    if (!machine_log_append_format(builder, " payload=[")) {
        return 0;
    }
    for (index = 0u; index < log_summary->payload_byte_count; ++index) {
        if (!machine_log_append_format(
                builder,
                "%s0x%02x",
                index == 0u ? "" : ",",
                (unsigned int)log_summary->payload_bytes[index])) {
            return 0;
        }
    }
    return machine_log_append_format(builder, "]");
}

static int machine_log_append_immediate_hint(
    MachineLogStringBuilder *builder,
    const MachineLogSummary *log_summary) {
    if (!builder || !log_summary) {
        return 0;
    }
    if (!log_summary->has_immediate_hint) {
        return machine_log_append_format(builder, " imm=-");
    }
    return machine_log_append_format(builder, " imm=%zu", log_summary->immediate_hint);
}

static MachineLogKind machine_log_classify_kind(
    const MachineTimelineSummary *timeline_summary) {
    if (!timeline_summary) {
        return MACHINE_LOG_KIND_NONE;
    }
    if (timeline_summary->resolution_kind == MACHINE_TIMELINE_RESOLUTION_BLOCKED_ON_CONTROL) {
        return MACHINE_LOG_KIND_BLOCKED_CONTROL_LOG;
    }
    if (timeline_summary->resolution_kind == MACHINE_TIMELINE_RESOLUTION_BLOCKED_UNSUPPORTED) {
        return MACHINE_LOG_KIND_BLOCKED_UNSUPPORTED_LOG;
    }
    switch (timeline_summary->timeline_kind) {
        case MACHINE_TIMELINE_KIND_PROGRAM_STOP_TIMELINE:
            return MACHINE_LOG_KIND_PROGRAM_STOP_LOG;
        case MACHINE_TIMELINE_KIND_VALUE_TIMELINE:
            return MACHINE_LOG_KIND_VALUE_LOG;
        case MACHINE_TIMELINE_KIND_LOCAL_UPDATE_TIMELINE:
            return MACHINE_LOG_KIND_LOCAL_UPDATE_LOG;
        case MACHINE_TIMELINE_KIND_GLOBAL_UPDATE_TIMELINE:
            return MACHINE_LOG_KIND_GLOBAL_UPDATE_LOG;
        case MACHINE_TIMELINE_KIND_CALL_TIMELINE:
            return MACHINE_LOG_KIND_CALL_LOG;
        case MACHINE_TIMELINE_KIND_BLOCKED_CONTROL_TIMELINE:
            return MACHINE_LOG_KIND_BLOCKED_CONTROL_LOG;
        case MACHINE_TIMELINE_KIND_BLOCKED_UNSUPPORTED_TIMELINE:
            return MACHINE_LOG_KIND_BLOCKED_UNSUPPORTED_LOG;
        case MACHINE_TIMELINE_KIND_NONE:
        default:
            return MACHINE_LOG_KIND_NONE;
    }
}

void machine_log_file_init(MachineLogFile *log_file) {
    if (!log_file) {
        return;
    }
    machine_timeline_file_init(&log_file->timeline_file);
    log_file->resolution_kind = MACHINE_LOG_RESOLUTION_NONE;
    log_file->log_kind = MACHINE_LOG_KIND_NONE;
    log_file->log_line_count = 0u;
    log_file->log_line_index = 0u;
}

void machine_log_file_free(MachineLogFile *log_file) {
    if (!log_file) {
        return;
    }
    machine_timeline_file_free(&log_file->timeline_file);
    machine_log_file_init(log_file);
}

void machine_log_report_init(MachineLogReport *report) {
    if (!report) {
        return;
    }
    machine_log_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_timeline_report_init(&report->timeline_report);
    memset(&report->log_summary, 0, sizeof(report->log_summary));
}

void machine_log_report_free(MachineLogReport *report) {
    if (!report) {
        return;
    }
    machine_timeline_report_free(&report->timeline_report);
    machine_log_file_free(&report->file);
    machine_log_report_init(report);
}

const char *machine_log_resolution_kind_name(MachineLogResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_LOG_RESOLUTION_NONE:
            return "none";
        case MACHINE_LOG_RESOLUTION_EXACT_LOG:
            return "exact-log";
        case MACHINE_LOG_RESOLUTION_PREVIEW_LOG:
            return "preview-log";
        case MACHINE_LOG_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_LOG_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_log_kind_name(MachineLogKind log_kind) {
    switch (log_kind) {
        case MACHINE_LOG_KIND_NONE:
            return "none";
        case MACHINE_LOG_KIND_PROGRAM_STOP_LOG:
            return "program-stop-log";
        case MACHINE_LOG_KIND_VALUE_LOG:
            return "value-log";
        case MACHINE_LOG_KIND_LOCAL_UPDATE_LOG:
            return "local-update-log";
        case MACHINE_LOG_KIND_GLOBAL_UPDATE_LOG:
            return "global-update-log";
        case MACHINE_LOG_KIND_CALL_LOG:
            return "call-log";
        case MACHINE_LOG_KIND_BLOCKED_CONTROL_LOG:
            return "blocked-control-log";
        case MACHINE_LOG_KIND_BLOCKED_UNSUPPORTED_LOG:
            return "blocked-unsupported-log";
    }
    return "unknown";
}

int machine_log_get_target_policy_summary(
    MachineElfTargetProfile profile,
    MachineLogTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->surfaces_exact_log = 1;
    out_summary->surfaces_preview_log = 1;
    out_summary->surfaces_single_line_log = 1;
    return 1;
}

int machine_log_file_get_target_policy_summary(
    const MachineLogFile *log_file,
    MachineLogTargetPolicySummary *out_summary) {
    if (!log_file || !out_summary) {
        return 0;
    }
    return machine_log_get_target_policy_summary(
        log_file->timeline_file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_log_file_get_summary(
    const MachineLogFile *log_file,
    size_t *out_mapped_byte_count) {
    if (!log_file) {
        return 0;
    }
    return machine_timeline_file_get_summary(&log_file->timeline_file, out_mapped_byte_count);
}

int machine_log_file_get_source_elf_artifact_summary(
    const MachineLogFile *log_file,
    MachineElfArtifactSummary *out_summary) {
    if (!log_file || !out_summary) {
        return 0;
    }
    return machine_timeline_file_get_source_elf_artifact_summary(&log_file->timeline_file, out_summary);
}

int machine_log_file_get_header_summary(
    const MachineLogFile *log_file,
    MachineLogHeaderSummary *out_summary) {
    MachineTimelineHeaderSummary timeline_header_summary;

    memset(&timeline_header_summary, 0, sizeof(timeline_header_summary));
    if (!log_file || !out_summary ||
        !machine_timeline_file_get_header_summary(&log_file->timeline_file, &timeline_header_summary)) {
        return 0;
    }
    out_summary->target_profile = timeline_header_summary.target_profile;
    out_summary->timeline_resolution_kind = log_file->timeline_file.resolution_kind;
    out_summary->origin_step_status = timeline_header_summary.origin_step_status;
    out_summary->origin_program_counter = timeline_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = timeline_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = timeline_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = timeline_header_summary.mapped_byte_count;
    out_summary->log_line_count = log_file->log_line_count;
    out_summary->log_line_index = log_file->log_line_index;
    return 1;
}

int machine_log_file_get_timeline_summary(
    const MachineLogFile *log_file,
    MachineTimelineSummary *out_summary) {
    if (!log_file || !out_summary) {
        return 0;
    }
    return machine_timeline_file_get_timeline_summary(&log_file->timeline_file, out_summary);
}

int machine_log_file_get_log_summary(
    const MachineLogFile *log_file,
    MachineLogSummary *out_summary) {
    MachineTimelineSummary timeline_summary;

    memset(&timeline_summary, 0, sizeof(timeline_summary));
    if (!log_file || !out_summary ||
        !machine_log_file_get_timeline_summary(log_file, &timeline_summary)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->resolution_kind = log_file->resolution_kind;
    out_summary->log_kind = log_file->log_kind;
    out_summary->timeline_resolution_kind = timeline_summary.resolution_kind;
    out_summary->timeline_kind = timeline_summary.timeline_kind;
    out_summary->history_resolution_kind = timeline_summary.history_resolution_kind;
    out_summary->history_kind = timeline_summary.history_kind;
    out_summary->outcome_resolution_kind = timeline_summary.outcome_resolution_kind;
    out_summary->outcome_kind = timeline_summary.outcome_kind;
    out_summary->event_resolution_kind = timeline_summary.event_resolution_kind;
    out_summary->event_kind = timeline_summary.event_kind;
    out_summary->trace_resolution_kind = timeline_summary.trace_resolution_kind;
    out_summary->trace_kind = timeline_summary.trace_kind;
    out_summary->trace_change_class = timeline_summary.trace_change_class;
    out_summary->delta_resolution_kind = timeline_summary.delta_resolution_kind;
    out_summary->delta_kind = timeline_summary.delta_kind;
    out_summary->observe_resolution_kind = timeline_summary.observe_resolution_kind;
    out_summary->observe_kind = timeline_summary.observe_kind;
    out_summary->apply_resolution_kind = timeline_summary.apply_resolution_kind;
    out_summary->apply_kind = timeline_summary.apply_kind;
    out_summary->commit_resolution_kind = timeline_summary.commit_resolution_kind;
    out_summary->commit_kind = timeline_summary.commit_kind;
    out_summary->writeback_resolution_kind = timeline_summary.writeback_resolution_kind;
    out_summary->writeback_commit_kind = timeline_summary.writeback_commit_kind;
    out_summary->mutation_resolution_kind = timeline_summary.mutation_resolution_kind;
    out_summary->mutation_effect_kind = timeline_summary.mutation_effect_kind;
    out_summary->state_resolution_kind = timeline_summary.state_resolution_kind;
    out_summary->transition_resolution_kind = timeline_summary.transition_resolution_kind;
    out_summary->action_kind = timeline_summary.action_kind;
    out_summary->payload_kind = timeline_summary.payload_kind;
    out_summary->tag_class = timeline_summary.tag_class;
    out_summary->raw_byte = timeline_summary.raw_byte;
    out_summary->tag_value = timeline_summary.tag_value;
    out_summary->is_known = timeline_summary.is_known;
    out_summary->tag_name = timeline_summary.tag_name;
    out_summary->instruction_byte_count = timeline_summary.instruction_byte_count;
    out_summary->payload_byte_count = timeline_summary.payload_byte_count;
    memcpy(out_summary->payload_bytes, timeline_summary.payload_bytes, sizeof(out_summary->payload_bytes));
    out_summary->has_immediate_hint = timeline_summary.has_immediate_hint;
    out_summary->immediate_hint = timeline_summary.immediate_hint;
    out_summary->is_exact_log = log_file->resolution_kind == MACHINE_LOG_RESOLUTION_EXACT_LOG;
    out_summary->is_single_line_log =
        log_file->log_line_count == 1u && log_file->log_line_index == 0u;
    out_summary->log_line_count = log_file->log_line_count;
    out_summary->log_line_index = log_file->log_line_index;
    out_summary->origin_status = timeline_summary.origin_status;
    out_summary->observed_status = timeline_summary.observed_status;
    out_summary->has_observed_state = timeline_summary.has_observed_state;
    out_summary->has_status_change = timeline_summary.has_status_change;
    out_summary->status_changed = timeline_summary.status_changed;
    out_summary->has_program_counter_change = timeline_summary.has_program_counter_change;
    out_summary->program_counter_changed = timeline_summary.program_counter_changed;
    out_summary->has_stack_pointer_change = timeline_summary.has_stack_pointer_change;
    out_summary->stack_pointer_changed = timeline_summary.stack_pointer_changed;
    out_summary->has_fetch_change = timeline_summary.has_fetch_change;
    out_summary->fetch_changed = timeline_summary.fetch_changed;
    out_summary->has_primary_target_block = timeline_summary.has_primary_target_block;
    out_summary->primary_target_block_index = timeline_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = timeline_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = timeline_summary.secondary_target_block_index;
    out_summary->has_return_immediate = timeline_summary.has_return_immediate;
    out_summary->return_immediate = timeline_summary.return_immediate;
    return 1;
}

int machine_log_verify_file(
    const MachineLogFile *log_file,
    MachineLogError *error) {
    MachineTimelineSummary timeline_summary;
    MachineLogKind expected_log_kind;

    memset(&timeline_summary, 0, sizeof(timeline_summary));
    if (!log_file) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-100: invalid log file");
        return 0;
    }
    if (!machine_timeline_verify_file(&log_file->timeline_file, NULL) ||
        !machine_log_file_get_timeline_summary(log_file, &timeline_summary)) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-101: invalid timeline input");
        return 0;
    }
    if (log_file->log_line_count != 1u || log_file->log_line_index != 0u) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-102: invalid log shape");
        return 0;
    }

    expected_log_kind = machine_log_classify_kind(&timeline_summary);
    if (log_file->log_kind != expected_log_kind) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-103: invalid log classification");
        return 0;
    }

    switch (log_file->resolution_kind) {
        case MACHINE_LOG_RESOLUTION_EXACT_LOG:
            if (log_file->timeline_file.resolution_kind != MACHINE_TIMELINE_RESOLUTION_EXACT_TIMELINE ||
                expected_log_kind == MACHINE_LOG_KIND_BLOCKED_CONTROL_LOG ||
                expected_log_kind == MACHINE_LOG_KIND_BLOCKED_UNSUPPORTED_LOG ||
                expected_log_kind == MACHINE_LOG_KIND_NONE) {
                machine_log_set_error(error, 0, 0, "MACHINE-LOG-104: invalid exact log");
                return 0;
            }
            break;
        case MACHINE_LOG_RESOLUTION_PREVIEW_LOG:
            if (log_file->timeline_file.resolution_kind != MACHINE_TIMELINE_RESOLUTION_PREVIEW_TIMELINE ||
                expected_log_kind == MACHINE_LOG_KIND_BLOCKED_CONTROL_LOG ||
                expected_log_kind == MACHINE_LOG_KIND_BLOCKED_UNSUPPORTED_LOG ||
                expected_log_kind == MACHINE_LOG_KIND_NONE) {
                machine_log_set_error(error, 0, 0, "MACHINE-LOG-105: invalid preview log");
                return 0;
            }
            break;
        case MACHINE_LOG_RESOLUTION_BLOCKED_ON_CONTROL:
            if (log_file->timeline_file.resolution_kind != MACHINE_TIMELINE_RESOLUTION_BLOCKED_ON_CONTROL ||
                expected_log_kind != MACHINE_LOG_KIND_BLOCKED_CONTROL_LOG) {
                machine_log_set_error(error, 0, 0, "MACHINE-LOG-106: invalid blocked-control log");
                return 0;
            }
            break;
        case MACHINE_LOG_RESOLUTION_BLOCKED_UNSUPPORTED:
            if (log_file->timeline_file.resolution_kind != MACHINE_TIMELINE_RESOLUTION_BLOCKED_UNSUPPORTED ||
                expected_log_kind != MACHINE_LOG_KIND_BLOCKED_UNSUPPORTED_LOG) {
                machine_log_set_error(error, 0, 0, "MACHINE-LOG-107: invalid blocked-unsupported log");
                return 0;
            }
            break;
        case MACHINE_LOG_RESOLUTION_NONE:
        default:
            machine_log_set_error(error, 0, 0, "MACHINE-LOG-108: invalid log resolution kind");
            return 0;
    }
    return 1;
}

int machine_log_clone_file(
    const MachineLogFile *source,
    MachineLogFile *out_clone,
    MachineLogError *error) {
    if (!source || !out_clone) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-109: invalid clone contract");
        return 0;
    }
    if (!machine_log_verify_file(source, error)) {
        return 0;
    }

    machine_log_file_free(out_clone);
    if (!machine_timeline_clone_file(&source->timeline_file, &out_clone->timeline_file, NULL)) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-110: failed cloning timeline input");
        machine_log_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->log_kind = source->log_kind;
    out_clone->log_line_count = source->log_line_count;
    out_clone->log_line_index = source->log_line_index;
    return 1;
}

int machine_log_build_from_machine_timeline_file(
    const MachineTimelineFile *timeline_file,
    MachineLogFile *out_log_file,
    MachineLogError *error) {
    MachineTimelineSummary timeline_summary;

    memset(&timeline_summary, 0, sizeof(timeline_summary));
    if (!timeline_file || !out_log_file) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-111: invalid build contract");
        return 0;
    }
    if (!machine_timeline_verify_file(timeline_file, NULL) ||
        !machine_timeline_file_get_timeline_summary(timeline_file, &timeline_summary)) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-112: invalid timeline input");
        return 0;
    }

    machine_log_file_free(out_log_file);
    if (!machine_timeline_clone_file(timeline_file, &out_log_file->timeline_file, NULL)) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-113: failed cloning timeline input");
        machine_log_file_free(out_log_file);
        return 0;
    }

    out_log_file->log_kind = machine_log_classify_kind(&timeline_summary);
    out_log_file->log_line_count = 1u;
    out_log_file->log_line_index = 0u;
    switch (timeline_file->resolution_kind) {
        case MACHINE_TIMELINE_RESOLUTION_EXACT_TIMELINE:
            out_log_file->resolution_kind = MACHINE_LOG_RESOLUTION_EXACT_LOG;
            break;
        case MACHINE_TIMELINE_RESOLUTION_PREVIEW_TIMELINE:
            out_log_file->resolution_kind = MACHINE_LOG_RESOLUTION_PREVIEW_LOG;
            break;
        case MACHINE_TIMELINE_RESOLUTION_BLOCKED_ON_CONTROL:
            out_log_file->resolution_kind = MACHINE_LOG_RESOLUTION_BLOCKED_ON_CONTROL;
            break;
        case MACHINE_TIMELINE_RESOLUTION_BLOCKED_UNSUPPORTED:
            out_log_file->resolution_kind = MACHINE_LOG_RESOLUTION_BLOCKED_UNSUPPORTED;
            break;
        case MACHINE_TIMELINE_RESOLUTION_NONE:
        default:
            machine_log_set_error(error, 0, 0, "MACHINE-LOG-114: unsupported timeline resolution");
            machine_log_file_free(out_log_file);
            return 0;
    }

    if (!machine_log_verify_file(out_log_file, error)) {
        machine_log_file_free(out_log_file);
        return 0;
    }
    return 1;
}

int machine_log_build_from_machine_timeline_report(
    const MachineTimelineReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error) {
    const MachineTimelineFile *timeline_file = NULL;

    if (!report) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-115: invalid build-from-timeline-report contract");
        return 0;
    }
    if (!machine_timeline_report_get_file(report, &timeline_file) || !timeline_file) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-116: failed recovering timeline file from report");
        return 0;
    }
    return machine_log_build_from_machine_timeline_file(timeline_file, out_log_file, error);
}

#define MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineLogFile *out_log_file, MachineLogError *error) { \
    MachineTimelineFile timeline_file; \
    int ok; \
    machine_timeline_file_init(&timeline_file); \
    ok = builder(source, &timeline_file, NULL) && \
        machine_log_build_from_machine_timeline_file(&timeline_file, out_log_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-117: failed building log wrapper"); \
    } \
    machine_timeline_file_free(&timeline_file); \
    return ok; \
}

MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_history_file,
    MachineHistoryFile, machine_timeline_build_from_machine_history_file)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_history_report,
    MachineHistoryReport, machine_timeline_build_from_machine_history_report)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_outcome_file,
    MachineOutcomeFile, machine_timeline_build_from_machine_outcome_file)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_outcome_report,
    MachineOutcomeReport, machine_timeline_build_from_machine_outcome_report)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_event_file,
    MachineEventFile, machine_timeline_build_from_machine_event_file)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_event_report,
    MachineEventReport, machine_timeline_build_from_machine_event_report)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_trace_file,
    MachineTraceFile, machine_timeline_build_from_machine_trace_file)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_trace_report,
    MachineTraceReport, machine_timeline_build_from_machine_trace_report)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_delta_file,
    MachineDeltaFile, machine_timeline_build_from_machine_delta_file)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_delta_report,
    MachineDeltaReport, machine_timeline_build_from_machine_delta_report)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_observe_file,
    MachineObserveFile, machine_timeline_build_from_machine_observe_file)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_observe_report,
    MachineObserveReport, machine_timeline_build_from_machine_observe_report)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_apply_file,
    MachineApplyFile, machine_timeline_build_from_machine_apply_file)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_apply_report,
    MachineApplyReport, machine_timeline_build_from_machine_apply_report)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_commit_file,
    MachineCommitFile, machine_timeline_build_from_machine_commit_file)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_commit_report,
    MachineCommitReport, machine_timeline_build_from_machine_commit_report)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_step_file,
    MachineStepFile, machine_timeline_build_from_machine_step_file)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_step_report,
    MachineStepReport, machine_timeline_build_from_machine_step_report)
MACHINE_LOG_DEFINE_BUILD_FROM_WRAPPER(machine_log_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_timeline_build_from_machine_ir_report)

int machine_log_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLogFile *out_log_file,
    MachineLogError *error) {
    MachineTimelineFile timeline_file;
    int ok;

    machine_timeline_file_init(&timeline_file);
    ok = machine_timeline_build_from_machine_ir_report_with_profile(report, profile, &timeline_file, NULL) &&
        machine_log_build_from_machine_timeline_file(&timeline_file, out_log_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-118: failed building profiled log wrapper");
    }
    machine_timeline_file_free(&timeline_file);
    return ok;
}

int machine_log_build_report_from_file(
    const MachineLogFile *source,
    MachineLogReport *out_report,
    MachineLogError *error) {
    if (!source || !out_report) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-119: invalid report build contract");
        return 0;
    }
    if (!machine_log_verify_file(source, error)) {
        return 0;
    }

    machine_log_report_free(out_report);
    if (!machine_log_clone_file(source, &out_report->file, error)) {
        machine_log_report_free(out_report);
        return 0;
    }
    if (!machine_log_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_log_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_timeline_build_report_from_file(&out_report->file.timeline_file, &out_report->timeline_report, NULL) ||
        !machine_log_file_get_log_summary(&out_report->file, &out_report->log_summary)) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-120: failed summarizing log report");
        machine_log_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineLogReport *out_report, MachineLogError *error) { \
    MachineLogFile log_file; \
    int ok; \
    machine_log_file_init(&log_file); \
    ok = builder(source, &log_file, error) && \
        machine_log_build_report_from_file(&log_file, out_report, error); \
    machine_log_file_free(&log_file); \
    return ok; \
}

MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_timeline_file,
    MachineTimelineFile, machine_log_build_from_machine_timeline_file)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_timeline_report,
    MachineTimelineReport, machine_log_build_from_machine_timeline_report)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_history_file,
    MachineHistoryFile, machine_log_build_from_machine_history_file)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_history_report,
    MachineHistoryReport, machine_log_build_from_machine_history_report)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_outcome_file,
    MachineOutcomeFile, machine_log_build_from_machine_outcome_file)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_outcome_report,
    MachineOutcomeReport, machine_log_build_from_machine_outcome_report)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_event_file,
    MachineEventFile, machine_log_build_from_machine_event_file)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_event_report,
    MachineEventReport, machine_log_build_from_machine_event_report)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_trace_file,
    MachineTraceFile, machine_log_build_from_machine_trace_file)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_trace_report,
    MachineTraceReport, machine_log_build_from_machine_trace_report)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_delta_file,
    MachineDeltaFile, machine_log_build_from_machine_delta_file)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_delta_report,
    MachineDeltaReport, machine_log_build_from_machine_delta_report)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_observe_file,
    MachineObserveFile, machine_log_build_from_machine_observe_file)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_observe_report,
    MachineObserveReport, machine_log_build_from_machine_observe_report)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_apply_file,
    MachineApplyFile, machine_log_build_from_machine_apply_file)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_apply_report,
    MachineApplyReport, machine_log_build_from_machine_apply_report)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_commit_file,
    MachineCommitFile, machine_log_build_from_machine_commit_file)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_commit_report,
    MachineCommitReport, machine_log_build_from_machine_commit_report)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_step_file,
    MachineStepFile, machine_log_build_from_machine_step_file)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_step_report,
    MachineStepReport, machine_log_build_from_machine_step_report)
MACHINE_LOG_DEFINE_REPORT_FROM_WRAPPER(machine_log_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_log_build_from_machine_ir_report)

int machine_log_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLogReport *out_report,
    MachineLogError *error) {
    MachineLogFile log_file;
    int ok;

    machine_log_file_init(&log_file);
    ok = machine_log_build_from_machine_ir_report_with_profile(report, profile, &log_file, error) &&
        machine_log_build_report_from_file(&log_file, out_report, error);
    machine_log_file_free(&log_file);
    return ok;
}

int machine_log_report_refresh(
    MachineLogReport *report,
    MachineLogError *error) {
    MachineLogReport refreshed_report;
    int ok;

    if (!report) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-121: invalid report refresh contract");
        return 0;
    }
    machine_log_report_init(&refreshed_report);
    ok = machine_log_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_log_report_free(report);
        *report = refreshed_report;
    } else {
        machine_log_report_free(&refreshed_report);
    }
    return ok;
}

int machine_log_dump_file(
    const MachineLogFile *log_file,
    char **out_text,
    MachineLogError *error) {
    MachineLogStringBuilder builder;
    MachineLogHeaderSummary header_summary;
    MachineLogSummary log_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&log_summary, 0, sizeof(log_summary));
    if (!log_file || !out_text ||
        !machine_log_verify_file(log_file, error) ||
        !machine_log_file_get_header_summary(log_file, &header_summary) ||
        !machine_log_file_get_log_summary(log_file, &log_summary)) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-122: invalid dump contract");
        return 0;
    }

    if (!machine_log_append_format(
            &builder,
            "machine_log profile=%s elf_origin=%s elf_semantics=%s timeline=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu lines=%zu line-index=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_elf_target_profile_name(
                log_file->timeline_file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file
                    .apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                log_file->timeline_file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file
                    .apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.relocation_semantics),
            machine_timeline_resolution_kind_name(header_summary.timeline_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count,
            header_summary.log_line_count,
            header_summary.log_line_index) ||
        !machine_log_append_format(
            &builder,
            "log: resolution=%s kind=%s timeline=%s history=%s outcome=%s event=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_log_resolution_kind_name(log_summary.resolution_kind),
            machine_log_kind_name(log_summary.log_kind),
            machine_timeline_kind_name(log_summary.timeline_kind),
            machine_history_kind_name(log_summary.history_kind),
            machine_outcome_kind_name(log_summary.outcome_kind),
            machine_event_kind_name(log_summary.event_kind),
            machine_trace_resolution_kind_name(log_summary.trace_resolution_kind),
            machine_trace_change_class_name(log_summary.trace_change_class),
            machine_apply_resolution_kind_name(log_summary.apply_resolution_kind),
            machine_commit_resolution_kind_name(log_summary.commit_resolution_kind),
            machine_writeback_resolution_kind_name(log_summary.writeback_resolution_kind),
            machine_mutation_resolution_kind_name(log_summary.mutation_resolution_kind),
            machine_mutation_effect_kind_name(log_summary.mutation_effect_kind),
            machine_transition_resolution_kind_name(log_summary.transition_resolution_kind),
            machine_interp_action_kind_name(log_summary.action_kind),
            (unsigned int)log_summary.raw_byte,
            (unsigned int)log_summary.tag_value,
            log_summary.is_known ? "yes" : "no",
            log_summary.tag_name ? log_summary.tag_name : "-",
            log_summary.instruction_byte_count) ||
        !machine_log_append_payload_bytes(&builder, &log_summary) ||
        !machine_log_append_immediate_hint(&builder, &log_summary) ||
        !machine_log_append_format(
            &builder,
            " exact=%s single-line=%s lines=%zu line-index=%zu origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            log_summary.is_exact_log ? "yes" : "no",
            log_summary.is_single_line_log ? "yes" : "no",
            log_summary.log_line_count,
            log_summary.log_line_index,
            machine_step_status_name(log_summary.origin_status),
            log_summary.has_observed_state ? machine_step_status_name(log_summary.observed_status) : "-",
            log_summary.has_status_change ? (log_summary.status_changed ? "yes" : "no") : "-",
            log_summary.has_program_counter_change ? (log_summary.program_counter_changed ? "yes" : "no") : "-",
            log_summary.has_stack_pointer_change ? (log_summary.stack_pointer_changed ? "yes" : "no") : "-",
            log_summary.has_fetch_change ? (log_summary.fetch_changed ? "yes" : "no") : "-") ||
        !machine_log_append_targets(&builder, &log_summary) ||
        !machine_log_append_return_immediate(&builder, &log_summary) ||
        !machine_log_append_format(&builder, "\n")) {
        free(builder.data);
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-123: out of memory building dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_log_build_dump_from_file(
    const MachineLogFile *source,
    char **out_text,
    MachineLogError *error) {
    return machine_log_dump_file(source, out_text, error);
}

#define MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineLogError *error) { \
    MachineLogFile log_file; \
    int ok; \
    machine_log_file_init(&log_file); \
    ok = builder(source, &log_file, error) && \
        machine_log_dump_file(&log_file, out_text, error); \
    machine_log_file_free(&log_file); \
    return ok; \
}

MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_timeline_file,
    MachineTimelineFile, machine_log_build_from_machine_timeline_file)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_timeline_report,
    MachineTimelineReport, machine_log_build_from_machine_timeline_report)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_history_file,
    MachineHistoryFile, machine_log_build_from_machine_history_file)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_history_report,
    MachineHistoryReport, machine_log_build_from_machine_history_report)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_outcome_file,
    MachineOutcomeFile, machine_log_build_from_machine_outcome_file)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_outcome_report,
    MachineOutcomeReport, machine_log_build_from_machine_outcome_report)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_event_file,
    MachineEventFile, machine_log_build_from_machine_event_file)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_event_report,
    MachineEventReport, machine_log_build_from_machine_event_report)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_trace_file,
    MachineTraceFile, machine_log_build_from_machine_trace_file)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_trace_report,
    MachineTraceReport, machine_log_build_from_machine_trace_report)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_delta_file,
    MachineDeltaFile, machine_log_build_from_machine_delta_file)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_delta_report,
    MachineDeltaReport, machine_log_build_from_machine_delta_report)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_observe_file,
    MachineObserveFile, machine_log_build_from_machine_observe_file)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_observe_report,
    MachineObserveReport, machine_log_build_from_machine_observe_report)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_apply_file,
    MachineApplyFile, machine_log_build_from_machine_apply_file)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_apply_report,
    MachineApplyReport, machine_log_build_from_machine_apply_report)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_commit_file,
    MachineCommitFile, machine_log_build_from_machine_commit_file)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_commit_report,
    MachineCommitReport, machine_log_build_from_machine_commit_report)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_step_file,
    MachineStepFile, machine_log_build_from_machine_step_file)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_step_report,
    MachineStepReport, machine_log_build_from_machine_step_report)
MACHINE_LOG_DEFINE_DUMP_FROM_WRAPPER(machine_log_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_log_build_from_machine_ir_report)

int machine_log_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLogError *error) {
    MachineLogFile log_file;
    int ok;

    machine_log_file_init(&log_file);
    ok = machine_log_build_from_machine_ir_report_with_profile(report, profile, &log_file, error) &&
        machine_log_dump_file(&log_file, out_text, error);
    machine_log_file_free(&log_file);
    return ok;
}

int machine_log_report_get_summary(
    const MachineLogReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_log_report_get_overview_artifact(
    const MachineLogReport *report,
    MachineLogReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->timeline_report = &report->timeline_report;
    out_artifact->log_summary = &report->log_summary;
    return 1;
}

int machine_log_report_get_file(
    const MachineLogReport *report,
    const MachineLogFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_log_report_get_timeline_file(
    const MachineLogReport *report,
    const MachineTimelineFile **out_timeline_file) {
    if (!report || !out_timeline_file) {
        return 0;
    }
    *out_timeline_file = &report->file.timeline_file;
    return 1;
}

int machine_log_report_get_timeline_report(
    const MachineLogReport *report,
    const MachineTimelineReport **out_timeline_report) {
    if (!report || !out_timeline_report) {
        return 0;
    }
    *out_timeline_report = &report->timeline_report;
    return 1;
}

int machine_log_report_get_source_elf_artifact_summary_artifact(
    const MachineLogReport *report,
    const MachineElfArtifactSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->file.timeline_file.history_file.outcome_file.event_file.trace_file.delta_file
                        .observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file
                        .transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file
                        .runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

#define MACHINE_LOG_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineLogReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_LOG_DEFINE_REPORT_ARTIFACT_GETTER(machine_log_report_get_header_summary_artifact,
    header_summary, MachineLogHeaderSummary)
MACHINE_LOG_DEFINE_REPORT_ARTIFACT_GETTER(machine_log_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineLogTargetPolicySummary)
MACHINE_LOG_DEFINE_REPORT_ARTIFACT_GETTER(machine_log_report_get_log_summary_artifact,
    log_summary, MachineLogSummary)

int machine_log_report_overview_artifact_get_timeline_report(
    const MachineLogReportOverviewArtifact *artifact,
    const MachineTimelineReport **out_timeline_report) {
    if (!artifact || !out_timeline_report) {
        return 0;
    }
    *out_timeline_report = artifact->timeline_report;
    return 1;
}

int machine_log_report_overview_artifact_get_log_summary_artifact(
    const MachineLogReportOverviewArtifact *artifact,
    const MachineLogSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->log_summary;
    return 1;
}

int machine_log_dump_report(
    const MachineLogReport *report,
    char **out_text,
    MachineLogError *error) {
    MachineLogStringBuilder builder;
    const MachineLogSummary *log_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_log_report_get_log_summary_artifact(report, &log_summary) ||
        !log_summary) {
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-124: invalid report dump contract");
        return 0;
    }

    if (!machine_log_append_format(
            &builder,
            "machine_log profile=%s elf_origin=%s elf_semantics=%s timeline=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu lines=%zu line-index=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.timeline_file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file
                    .apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.timeline_file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file
                    .apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.relocation_semantics),
            machine_timeline_resolution_kind_name(report->header_summary.timeline_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.log_line_count,
            report->header_summary.log_line_index) ||
        !machine_log_append_format(
            &builder,
            "log: resolution=%s kind=%s timeline=%s history=%s outcome=%s event=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_log_resolution_kind_name(log_summary->resolution_kind),
            machine_log_kind_name(log_summary->log_kind),
            machine_timeline_kind_name(log_summary->timeline_kind),
            machine_history_kind_name(log_summary->history_kind),
            machine_outcome_kind_name(log_summary->outcome_kind),
            machine_event_kind_name(log_summary->event_kind),
            machine_trace_resolution_kind_name(log_summary->trace_resolution_kind),
            machine_trace_change_class_name(log_summary->trace_change_class),
            machine_apply_resolution_kind_name(log_summary->apply_resolution_kind),
            machine_commit_resolution_kind_name(log_summary->commit_resolution_kind),
            machine_writeback_resolution_kind_name(log_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(log_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(log_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(log_summary->transition_resolution_kind),
            machine_interp_action_kind_name(log_summary->action_kind),
            (unsigned int)log_summary->raw_byte,
            (unsigned int)log_summary->tag_value,
            log_summary->is_known ? "yes" : "no",
            log_summary->tag_name ? log_summary->tag_name : "-",
            log_summary->instruction_byte_count) ||
        !machine_log_append_payload_bytes(&builder, log_summary) ||
        !machine_log_append_immediate_hint(&builder, log_summary) ||
        !machine_log_append_format(
            &builder,
            " exact=%s single-line=%s lines=%zu line-index=%zu origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            log_summary->is_exact_log ? "yes" : "no",
            log_summary->is_single_line_log ? "yes" : "no",
            log_summary->log_line_count,
            log_summary->log_line_index,
            machine_step_status_name(log_summary->origin_status),
            log_summary->has_observed_state ? machine_step_status_name(log_summary->observed_status) : "-",
            log_summary->has_status_change ? (log_summary->status_changed ? "yes" : "no") : "-",
            log_summary->has_program_counter_change ? (log_summary->program_counter_changed ? "yes" : "no") : "-",
            log_summary->has_stack_pointer_change ? (log_summary->stack_pointer_changed ? "yes" : "no") : "-",
            log_summary->has_fetch_change ? (log_summary->fetch_changed ? "yes" : "no") : "-") ||
        !machine_log_append_targets(&builder, log_summary) ||
        !machine_log_append_return_immediate(&builder, log_summary) ||
        !machine_log_append_format(&builder, "\nreport_overview:\n") ||
        !machine_log_append_format(
            &builder,
            "  origin: timeline=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx lines=%zu line-index=%zu\n",
            machine_timeline_resolution_kind_name(report->header_summary.timeline_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.log_line_count,
            report->header_summary.log_line_index) ||
        !machine_log_append_format(
            &builder,
            "  elf_source: target=%s origin=%s semantics=%s\n",
            machine_elf_target_profile_name(
                report->file.timeline_file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file
                    .apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.timeline_file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file
                    .apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.timeline_file.history_file.outcome_file.event_file.trace_file.delta_file.observe_file
                    .apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file
                    .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file
                    .image_file.source_elf_artifact_summary.relocation_semantics)) ||
        !machine_log_append_format(
            &builder,
            "  policy: profile=%s exact=%s preview=%s single-line=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.surfaces_exact_log ? "yes" : "no",
            report->target_policy_summary.surfaces_preview_log ? "yes" : "no",
            report->target_policy_summary.surfaces_single_line_log ? "yes" : "no") ||
        !machine_log_append_format(
            &builder,
            "  log: resolution=%s kind=%s timeline=%s exact=%s single-line=%s lines=%zu line-index=%zu state=%s status=%s pc=%s fetch=%s",
            machine_log_resolution_kind_name(log_summary->resolution_kind),
            machine_log_kind_name(log_summary->log_kind),
            machine_timeline_kind_name(log_summary->timeline_kind),
            log_summary->is_exact_log ? "yes" : "no",
            log_summary->is_single_line_log ? "yes" : "no",
            log_summary->log_line_count,
            log_summary->log_line_index,
            log_summary->has_observed_state ? "yes" : "no",
            log_summary->has_status_change ? (log_summary->status_changed ? "yes" : "no") : "-",
            log_summary->has_program_counter_change ? (log_summary->program_counter_changed ? "yes" : "no") : "-",
            log_summary->has_fetch_change ? (log_summary->fetch_changed ? "yes" : "no") : "-")) {
        free(builder.data);
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-125: out of memory building report dump");
        return 0;
    }
    if (!machine_log_append_targets(&builder, log_summary) ||
        !machine_log_append_return_immediate(&builder, log_summary) ||
        !machine_log_append_format(&builder, "\n")) {
        free(builder.data);
        machine_log_set_error(error, 0, 0, "MACHINE-LOG-126: out of memory terminating report dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_log_build_report_dump_from_file(
    const MachineLogFile *source,
    char **out_text,
    MachineLogError *error) {
    MachineLogReport report;
    int ok;

    machine_log_report_init(&report);
    ok = machine_log_build_report_from_file(source, &report, error) &&
        machine_log_dump_report(&report, out_text, error);
    machine_log_report_free(&report);
    return ok;
}

#define MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineLogError *error) { \
    MachineLogReport report; \
    int ok; \
    machine_log_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_log_dump_report(&report, out_text, error); \
    machine_log_report_free(&report); \
    return ok; \
}

MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_timeline_file,
    MachineTimelineFile, machine_log_build_report_from_machine_timeline_file)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_timeline_report,
    MachineTimelineReport, machine_log_build_report_from_machine_timeline_report)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_history_file,
    MachineHistoryFile, machine_log_build_report_from_machine_history_file)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_history_report,
    MachineHistoryReport, machine_log_build_report_from_machine_history_report)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_outcome_file,
    MachineOutcomeFile, machine_log_build_report_from_machine_outcome_file)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_outcome_report,
    MachineOutcomeReport, machine_log_build_report_from_machine_outcome_report)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_event_file,
    MachineEventFile, machine_log_build_report_from_machine_event_file)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_event_report,
    MachineEventReport, machine_log_build_report_from_machine_event_report)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_trace_file,
    MachineTraceFile, machine_log_build_report_from_machine_trace_file)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_trace_report,
    MachineTraceReport, machine_log_build_report_from_machine_trace_report)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_delta_file,
    MachineDeltaFile, machine_log_build_report_from_machine_delta_file)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_delta_report,
    MachineDeltaReport, machine_log_build_report_from_machine_delta_report)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_observe_file,
    MachineObserveFile, machine_log_build_report_from_machine_observe_file)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_observe_report,
    MachineObserveReport, machine_log_build_report_from_machine_observe_report)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_apply_file,
    MachineApplyFile, machine_log_build_report_from_machine_apply_file)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_apply_report,
    MachineApplyReport, machine_log_build_report_from_machine_apply_report)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_commit_file,
    MachineCommitFile, machine_log_build_report_from_machine_commit_file)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_commit_report,
    MachineCommitReport, machine_log_build_report_from_machine_commit_report)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_log_build_report_from_machine_step_file)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_log_build_report_from_machine_step_report)
MACHINE_LOG_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_log_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_log_build_report_from_machine_ir_report)

int machine_log_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLogError *error) {
    MachineLogReport report_file;
    int ok;

    machine_log_report_init(&report_file);
    ok = machine_log_build_report_from_machine_ir_report_with_profile(
            report, profile, &report_file, error) &&
        machine_log_dump_report(&report_file, out_text, error);
    machine_log_report_free(&report_file);
    return ok;
}
