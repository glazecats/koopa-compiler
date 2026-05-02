#include "machine/history.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineHistoryStringBuilder;

static void machine_history_set_error(
    MachineHistoryError *error,
    int line,
    int column,
    const char *fmt,
    ...);
static int machine_history_append_format(
    MachineHistoryStringBuilder *builder,
    const char *fmt,
    ...);
static int machine_history_append_targets(
    MachineHistoryStringBuilder *builder,
    const MachineHistorySummary *history_summary);
static int machine_history_append_return_immediate(
    MachineHistoryStringBuilder *builder,
    const MachineHistorySummary *history_summary);
static int machine_history_append_payload_bytes(
    MachineHistoryStringBuilder *builder,
    const MachineHistorySummary *history_summary);
static int machine_history_append_immediate_hint(
    MachineHistoryStringBuilder *builder,
    const MachineHistorySummary *history_summary);
static MachineHistoryKind machine_history_classify_kind(
    const MachineOutcomeSummary *outcome_summary);

static void machine_history_set_error(
    MachineHistoryError *error,
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

static int machine_history_append_format(
    MachineHistoryStringBuilder *builder,
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

static int machine_history_append_targets(
    MachineHistoryStringBuilder *builder,
    const MachineHistorySummary *history_summary) {
    if (!builder || !history_summary) {
        return 0;
    }
    if (!history_summary->has_primary_target_block) {
        return machine_history_append_format(builder, " targets=[]");
    }
    if (!history_summary->has_secondary_target_block) {
        return machine_history_append_format(
            builder, " targets=[%zu]", history_summary->primary_target_block_index);
    }
    return machine_history_append_format(
        builder,
        " targets=[%zu,%zu]",
        history_summary->primary_target_block_index,
        history_summary->secondary_target_block_index);
}

static int machine_history_append_return_immediate(
    MachineHistoryStringBuilder *builder,
    const MachineHistorySummary *history_summary) {
    if (!builder || !history_summary) {
        return 0;
    }
    if (!history_summary->has_return_immediate) {
        return machine_history_append_format(builder, " return-imm=-");
    }
    return machine_history_append_format(builder, " return-imm=%zu", history_summary->return_immediate);
}

static int machine_history_append_payload_bytes(
    MachineHistoryStringBuilder *builder,
    const MachineHistorySummary *history_summary) {
    size_t index;

    if (!builder || !history_summary) {
        return 0;
    }
    if (!machine_history_append_format(builder, " payload=[")) {
        return 0;
    }
    for (index = 0u; index < history_summary->payload_byte_count; ++index) {
        if (!machine_history_append_format(
                builder,
                "%s0x%02x",
                index == 0u ? "" : ",",
                (unsigned int)history_summary->payload_bytes[index])) {
            return 0;
        }
    }
    return machine_history_append_format(builder, "]");
}

static int machine_history_append_immediate_hint(
    MachineHistoryStringBuilder *builder,
    const MachineHistorySummary *history_summary) {
    if (!builder || !history_summary) {
        return 0;
    }
    if (!history_summary->has_immediate_hint) {
        return machine_history_append_format(builder, " imm=-");
    }
    return machine_history_append_format(builder, " imm=%zu", history_summary->immediate_hint);
}

static MachineHistoryKind machine_history_classify_kind(
    const MachineOutcomeSummary *outcome_summary) {
    if (!outcome_summary) {
        return MACHINE_HISTORY_KIND_NONE;
    }
    if (outcome_summary->resolution_kind == MACHINE_OUTCOME_RESOLUTION_BLOCKED_ON_CONTROL) {
        return MACHINE_HISTORY_KIND_BLOCKED_CONTROL_HISTORY;
    }
    if (outcome_summary->resolution_kind == MACHINE_OUTCOME_RESOLUTION_BLOCKED_UNSUPPORTED) {
        return MACHINE_HISTORY_KIND_BLOCKED_UNSUPPORTED_HISTORY;
    }
    switch (outcome_summary->outcome_kind) {
        case MACHINE_OUTCOME_KIND_PROGRAM_STOPPED:
            return MACHINE_HISTORY_KIND_PROGRAM_STOP_HISTORY;
        case MACHINE_OUTCOME_KIND_VALUE_AVAILABLE:
            return MACHINE_HISTORY_KIND_VALUE_HISTORY;
        case MACHINE_OUTCOME_KIND_LOCAL_UPDATED:
            return MACHINE_HISTORY_KIND_LOCAL_UPDATE_HISTORY;
        case MACHINE_OUTCOME_KIND_GLOBAL_UPDATED:
            return MACHINE_HISTORY_KIND_GLOBAL_UPDATE_HISTORY;
        case MACHINE_OUTCOME_KIND_CALL_ISSUED:
            return MACHINE_HISTORY_KIND_CALL_HISTORY;
        case MACHINE_OUTCOME_KIND_BLOCKED_CONTROL:
            return MACHINE_HISTORY_KIND_BLOCKED_CONTROL_HISTORY;
        case MACHINE_OUTCOME_KIND_BLOCKED_UNSUPPORTED:
            return MACHINE_HISTORY_KIND_BLOCKED_UNSUPPORTED_HISTORY;
        case MACHINE_OUTCOME_KIND_NONE:
        default:
            return MACHINE_HISTORY_KIND_NONE;
    }
}

void machine_history_file_init(MachineHistoryFile *history_file) {
    if (!history_file) {
        return;
    }
    machine_outcome_file_init(&history_file->outcome_file);
    history_file->resolution_kind = MACHINE_HISTORY_RESOLUTION_NONE;
    history_file->history_kind = MACHINE_HISTORY_KIND_NONE;
    history_file->history_entry_count = 0u;
}

void machine_history_file_free(MachineHistoryFile *history_file) {
    if (!history_file) {
        return;
    }
    machine_outcome_file_free(&history_file->outcome_file);
    machine_history_file_init(history_file);
}

void machine_history_report_init(MachineHistoryReport *report) {
    if (!report) {
        return;
    }
    machine_history_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_outcome_report_init(&report->outcome_report);
    memset(&report->history_summary, 0, sizeof(report->history_summary));
}

void machine_history_report_free(MachineHistoryReport *report) {
    if (!report) {
        return;
    }
    machine_outcome_report_free(&report->outcome_report);
    machine_history_file_free(&report->file);
    machine_history_report_init(report);
}

const char *machine_history_resolution_kind_name(MachineHistoryResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_HISTORY_RESOLUTION_NONE:
            return "none";
        case MACHINE_HISTORY_RESOLUTION_EXACT_HISTORY:
            return "exact-history";
        case MACHINE_HISTORY_RESOLUTION_PREVIEW_HISTORY:
            return "preview-history";
        case MACHINE_HISTORY_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_HISTORY_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_history_kind_name(MachineHistoryKind history_kind) {
    switch (history_kind) {
        case MACHINE_HISTORY_KIND_NONE:
            return "none";
        case MACHINE_HISTORY_KIND_PROGRAM_STOP_HISTORY:
            return "program-stop-history";
        case MACHINE_HISTORY_KIND_VALUE_HISTORY:
            return "value-history";
        case MACHINE_HISTORY_KIND_LOCAL_UPDATE_HISTORY:
            return "local-update-history";
        case MACHINE_HISTORY_KIND_GLOBAL_UPDATE_HISTORY:
            return "global-update-history";
        case MACHINE_HISTORY_KIND_CALL_HISTORY:
            return "call-history";
        case MACHINE_HISTORY_KIND_BLOCKED_CONTROL_HISTORY:
            return "blocked-control-history";
        case MACHINE_HISTORY_KIND_BLOCKED_UNSUPPORTED_HISTORY:
            return "blocked-unsupported-history";
    }
    return "unknown";
}

int machine_history_get_target_policy_summary(
    MachineElfTargetProfile profile,
    MachineHistoryTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->surfaces_exact_history = 1;
    out_summary->surfaces_preview_history = 1;
    out_summary->surfaces_single_entry_history = 1;
    return 1;
}

int machine_history_file_get_target_policy_summary(
    const MachineHistoryFile *history_file,
    MachineHistoryTargetPolicySummary *out_summary) {
    if (!history_file || !out_summary) {
        return 0;
    }
    return machine_history_get_target_policy_summary(
        history_file->outcome_file.event_file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_history_file_get_summary(
    const MachineHistoryFile *history_file,
    size_t *out_mapped_byte_count) {
    if (!history_file) {
        return 0;
    }
    return machine_outcome_file_get_summary(&history_file->outcome_file, out_mapped_byte_count);
}

int machine_history_file_get_header_summary(
    const MachineHistoryFile *history_file,
    MachineHistoryHeaderSummary *out_summary) {
    MachineOutcomeHeaderSummary outcome_header_summary;

    memset(&outcome_header_summary, 0, sizeof(outcome_header_summary));
    if (!history_file || !out_summary ||
        !machine_outcome_file_get_header_summary(&history_file->outcome_file, &outcome_header_summary)) {
        return 0;
    }
    out_summary->target_profile = outcome_header_summary.target_profile;
    out_summary->outcome_resolution_kind = history_file->outcome_file.resolution_kind;
    out_summary->origin_step_status = outcome_header_summary.origin_step_status;
    out_summary->origin_program_counter = outcome_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = outcome_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = outcome_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = outcome_header_summary.mapped_byte_count;
    out_summary->history_entry_count = history_file->history_entry_count;
    return 1;
}

int machine_history_file_get_outcome_summary(
    const MachineHistoryFile *history_file,
    MachineOutcomeSummary *out_summary) {
    if (!history_file || !out_summary) {
        return 0;
    }
    return machine_outcome_file_get_outcome_summary(&history_file->outcome_file, out_summary);
}

int machine_history_file_get_history_summary(
    const MachineHistoryFile *history_file,
    MachineHistorySummary *out_summary) {
    MachineOutcomeSummary outcome_summary;

    memset(&outcome_summary, 0, sizeof(outcome_summary));
    if (!history_file || !out_summary ||
        !machine_history_file_get_outcome_summary(history_file, &outcome_summary)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->resolution_kind = history_file->resolution_kind;
    out_summary->history_kind = history_file->history_kind;
    out_summary->outcome_resolution_kind = outcome_summary.resolution_kind;
    out_summary->outcome_kind = outcome_summary.outcome_kind;
    out_summary->event_resolution_kind = outcome_summary.event_resolution_kind;
    out_summary->event_kind = outcome_summary.event_kind;
    out_summary->trace_resolution_kind = outcome_summary.trace_resolution_kind;
    out_summary->trace_kind = outcome_summary.trace_kind;
    out_summary->trace_change_class = outcome_summary.trace_change_class;
    out_summary->delta_resolution_kind = outcome_summary.delta_resolution_kind;
    out_summary->delta_kind = outcome_summary.delta_kind;
    out_summary->observe_resolution_kind = outcome_summary.observe_resolution_kind;
    out_summary->observe_kind = outcome_summary.observe_kind;
    out_summary->apply_resolution_kind = outcome_summary.apply_resolution_kind;
    out_summary->apply_kind = outcome_summary.apply_kind;
    out_summary->commit_resolution_kind = outcome_summary.commit_resolution_kind;
    out_summary->commit_kind = outcome_summary.commit_kind;
    out_summary->writeback_resolution_kind = outcome_summary.writeback_resolution_kind;
    out_summary->writeback_commit_kind = outcome_summary.writeback_commit_kind;
    out_summary->mutation_resolution_kind = outcome_summary.mutation_resolution_kind;
    out_summary->mutation_effect_kind = outcome_summary.mutation_effect_kind;
    out_summary->state_resolution_kind = outcome_summary.state_resolution_kind;
    out_summary->transition_resolution_kind = outcome_summary.transition_resolution_kind;
    out_summary->action_kind = outcome_summary.action_kind;
    out_summary->payload_kind = outcome_summary.payload_kind;
    out_summary->tag_class = outcome_summary.tag_class;
    out_summary->raw_byte = outcome_summary.raw_byte;
    out_summary->tag_value = outcome_summary.tag_value;
    out_summary->is_known = outcome_summary.is_known;
    out_summary->tag_name = outcome_summary.tag_name;
    out_summary->instruction_byte_count = outcome_summary.instruction_byte_count;
    out_summary->payload_byte_count = outcome_summary.payload_byte_count;
    memcpy(out_summary->payload_bytes, outcome_summary.payload_bytes, sizeof(out_summary->payload_bytes));
    out_summary->has_immediate_hint = outcome_summary.has_immediate_hint;
    out_summary->immediate_hint = outcome_summary.immediate_hint;
    out_summary->is_exact_history = history_file->resolution_kind == MACHINE_HISTORY_RESOLUTION_EXACT_HISTORY;
    out_summary->is_single_entry_history = history_file->history_entry_count == 1u;
    out_summary->history_entry_count = history_file->history_entry_count;
    out_summary->origin_status = outcome_summary.origin_status;
    out_summary->observed_status = outcome_summary.observed_status;
    out_summary->has_observed_state = outcome_summary.has_observed_state;
    out_summary->has_status_change = outcome_summary.has_status_change;
    out_summary->status_changed = outcome_summary.status_changed;
    out_summary->has_program_counter_change = outcome_summary.has_program_counter_change;
    out_summary->program_counter_changed = outcome_summary.program_counter_changed;
    out_summary->has_stack_pointer_change = outcome_summary.has_stack_pointer_change;
    out_summary->stack_pointer_changed = outcome_summary.stack_pointer_changed;
    out_summary->has_fetch_change = outcome_summary.has_fetch_change;
    out_summary->fetch_changed = outcome_summary.fetch_changed;
    out_summary->has_primary_target_block = outcome_summary.has_primary_target_block;
    out_summary->primary_target_block_index = outcome_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = outcome_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = outcome_summary.secondary_target_block_index;
    out_summary->has_return_immediate = outcome_summary.has_return_immediate;
    out_summary->return_immediate = outcome_summary.return_immediate;
    return 1;
}

int machine_history_verify_file(
    const MachineHistoryFile *history_file,
    MachineHistoryError *error) {
    MachineOutcomeSummary outcome_summary;
    MachineHistoryKind expected_history_kind;

    memset(&outcome_summary, 0, sizeof(outcome_summary));
    if (!history_file) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-100: invalid history file");
        return 0;
    }
    if (!machine_outcome_verify_file(&history_file->outcome_file, NULL) ||
        !machine_history_file_get_outcome_summary(history_file, &outcome_summary)) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-101: invalid outcome input");
        return 0;
    }
    if (history_file->history_entry_count != 1u) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-102: invalid history entry count");
        return 0;
    }

    expected_history_kind = machine_history_classify_kind(&outcome_summary);
    if (history_file->history_kind != expected_history_kind) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-103: invalid history classification");
        return 0;
    }

    switch (history_file->resolution_kind) {
        case MACHINE_HISTORY_RESOLUTION_EXACT_HISTORY:
            if (history_file->outcome_file.resolution_kind != MACHINE_OUTCOME_RESOLUTION_EXACT_OUTCOME ||
                expected_history_kind == MACHINE_HISTORY_KIND_BLOCKED_CONTROL_HISTORY ||
                expected_history_kind == MACHINE_HISTORY_KIND_BLOCKED_UNSUPPORTED_HISTORY ||
                expected_history_kind == MACHINE_HISTORY_KIND_NONE) {
                machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-104: invalid exact history");
                return 0;
            }
            break;
        case MACHINE_HISTORY_RESOLUTION_PREVIEW_HISTORY:
            if (history_file->outcome_file.resolution_kind != MACHINE_OUTCOME_RESOLUTION_PREVIEW_OUTCOME ||
                expected_history_kind == MACHINE_HISTORY_KIND_BLOCKED_CONTROL_HISTORY ||
                expected_history_kind == MACHINE_HISTORY_KIND_BLOCKED_UNSUPPORTED_HISTORY ||
                expected_history_kind == MACHINE_HISTORY_KIND_NONE) {
                machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-105: invalid preview history");
                return 0;
            }
            break;
        case MACHINE_HISTORY_RESOLUTION_BLOCKED_ON_CONTROL:
            if (history_file->outcome_file.resolution_kind != MACHINE_OUTCOME_RESOLUTION_BLOCKED_ON_CONTROL ||
                expected_history_kind != MACHINE_HISTORY_KIND_BLOCKED_CONTROL_HISTORY) {
                machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-106: invalid blocked-control history");
                return 0;
            }
            break;
        case MACHINE_HISTORY_RESOLUTION_BLOCKED_UNSUPPORTED:
            if (history_file->outcome_file.resolution_kind != MACHINE_OUTCOME_RESOLUTION_BLOCKED_UNSUPPORTED ||
                expected_history_kind != MACHINE_HISTORY_KIND_BLOCKED_UNSUPPORTED_HISTORY) {
                machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-107: invalid blocked-unsupported history");
                return 0;
            }
            break;
        case MACHINE_HISTORY_RESOLUTION_NONE:
        default:
            machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-108: invalid history resolution kind");
            return 0;
    }
    return 1;
}

int machine_history_clone_file(
    const MachineHistoryFile *source,
    MachineHistoryFile *out_clone,
    MachineHistoryError *error) {
    if (!source || !out_clone) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-109: invalid clone contract");
        return 0;
    }
    if (!machine_history_verify_file(source, error)) {
        return 0;
    }

    machine_history_file_free(out_clone);
    if (!machine_outcome_clone_file(&source->outcome_file, &out_clone->outcome_file, NULL)) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-110: failed cloning outcome input");
        machine_history_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->history_kind = source->history_kind;
    out_clone->history_entry_count = source->history_entry_count;
    return 1;
}

int machine_history_build_from_machine_outcome_file(
    const MachineOutcomeFile *outcome_file,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error) {
    MachineOutcomeSummary outcome_summary;

    memset(&outcome_summary, 0, sizeof(outcome_summary));
    if (!outcome_file || !out_history_file) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-111: invalid build contract");
        return 0;
    }
    if (!machine_outcome_verify_file(outcome_file, NULL) ||
        !machine_outcome_file_get_outcome_summary(outcome_file, &outcome_summary)) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-112: invalid outcome input");
        return 0;
    }

    machine_history_file_free(out_history_file);
    if (!machine_outcome_clone_file(outcome_file, &out_history_file->outcome_file, NULL)) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-113: failed cloning outcome input");
        machine_history_file_free(out_history_file);
        return 0;
    }

    out_history_file->history_kind = machine_history_classify_kind(&outcome_summary);
    out_history_file->history_entry_count = 1u;
    switch (outcome_file->resolution_kind) {
        case MACHINE_OUTCOME_RESOLUTION_EXACT_OUTCOME:
            out_history_file->resolution_kind = MACHINE_HISTORY_RESOLUTION_EXACT_HISTORY;
            break;
        case MACHINE_OUTCOME_RESOLUTION_PREVIEW_OUTCOME:
            out_history_file->resolution_kind = MACHINE_HISTORY_RESOLUTION_PREVIEW_HISTORY;
            break;
        case MACHINE_OUTCOME_RESOLUTION_BLOCKED_ON_CONTROL:
            out_history_file->resolution_kind = MACHINE_HISTORY_RESOLUTION_BLOCKED_ON_CONTROL;
            break;
        case MACHINE_OUTCOME_RESOLUTION_BLOCKED_UNSUPPORTED:
            out_history_file->resolution_kind = MACHINE_HISTORY_RESOLUTION_BLOCKED_UNSUPPORTED;
            break;
        case MACHINE_OUTCOME_RESOLUTION_NONE:
        default:
            machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-114: unsupported outcome resolution");
            machine_history_file_free(out_history_file);
            return 0;
    }

    if (!machine_history_verify_file(out_history_file, error)) {
        machine_history_file_free(out_history_file);
        return 0;
    }
    return 1;
}

int machine_history_build_from_machine_outcome_report(
    const MachineOutcomeReport *report,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error) {
    const MachineOutcomeFile *outcome_file = NULL;

    if (!report) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-115: invalid build-from-outcome-report contract");
        return 0;
    }
    if (!machine_outcome_report_get_file(report, &outcome_file) || !outcome_file) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-116: failed recovering outcome file from report");
        return 0;
    }
    return machine_history_build_from_machine_outcome_file(outcome_file, out_history_file, error);
}

#define MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineHistoryFile *out_history_file, MachineHistoryError *error) { \
    MachineOutcomeFile outcome_file; \
    int ok; \
    machine_outcome_file_init(&outcome_file); \
    ok = builder(source, &outcome_file, NULL) && \
        machine_history_build_from_machine_outcome_file(&outcome_file, out_history_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-117: failed building history wrapper"); \
    } \
    machine_outcome_file_free(&outcome_file); \
    return ok; \
}

MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_event_file,
    MachineEventFile, machine_outcome_build_from_machine_event_file)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_event_report,
    MachineEventReport, machine_outcome_build_from_machine_event_report)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_trace_file,
    MachineTraceFile, machine_outcome_build_from_machine_trace_file)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_trace_report,
    MachineTraceReport, machine_outcome_build_from_machine_trace_report)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_delta_file,
    MachineDeltaFile, machine_outcome_build_from_machine_delta_file)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_delta_report,
    MachineDeltaReport, machine_outcome_build_from_machine_delta_report)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_observe_file,
    MachineObserveFile, machine_outcome_build_from_machine_observe_file)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_observe_report,
    MachineObserveReport, machine_outcome_build_from_machine_observe_report)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_apply_file,
    MachineApplyFile, machine_outcome_build_from_machine_apply_file)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_apply_report,
    MachineApplyReport, machine_outcome_build_from_machine_apply_report)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_commit_file,
    MachineCommitFile, machine_outcome_build_from_machine_commit_file)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_commit_report,
    MachineCommitReport, machine_outcome_build_from_machine_commit_report)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_step_file,
    MachineStepFile, machine_outcome_build_from_machine_step_file)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_step_report,
    MachineStepReport, machine_outcome_build_from_machine_step_report)
MACHINE_HISTORY_DEFINE_BUILD_FROM_WRAPPER(machine_history_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_outcome_build_from_machine_ir_report)

int machine_history_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error) {
    MachineOutcomeFile outcome_file;
    int ok;

    machine_outcome_file_init(&outcome_file);
    ok = machine_outcome_build_from_machine_ir_report_with_profile(report, profile, &outcome_file, NULL) &&
        machine_history_build_from_machine_outcome_file(&outcome_file, out_history_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-118: failed building profiled history wrapper");
    }
    machine_outcome_file_free(&outcome_file);
    return ok;
}

int machine_history_build_report_from_file(
    const MachineHistoryFile *source,
    MachineHistoryReport *out_report,
    MachineHistoryError *error) {
    if (!source || !out_report) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-119: invalid report build contract");
        return 0;
    }
    if (!machine_history_verify_file(source, error)) {
        return 0;
    }

    machine_history_report_free(out_report);
    if (!machine_history_clone_file(source, &out_report->file, error)) {
        machine_history_report_free(out_report);
        return 0;
    }
    if (!machine_history_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_history_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_outcome_build_report_from_file(&out_report->file.outcome_file, &out_report->outcome_report, NULL) ||
        !machine_history_file_get_history_summary(&out_report->file, &out_report->history_summary)) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-120: failed summarizing history report");
        machine_history_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineHistoryReport *out_report, MachineHistoryError *error) { \
    MachineHistoryFile history_file; \
    int ok; \
    machine_history_file_init(&history_file); \
    ok = builder(source, &history_file, error) && \
        machine_history_build_report_from_file(&history_file, out_report, error); \
    machine_history_file_free(&history_file); \
    return ok; \
}

MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_outcome_file,
    MachineOutcomeFile, machine_history_build_from_machine_outcome_file)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_outcome_report,
    MachineOutcomeReport, machine_history_build_from_machine_outcome_report)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_event_file,
    MachineEventFile, machine_history_build_from_machine_event_file)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_event_report,
    MachineEventReport, machine_history_build_from_machine_event_report)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_trace_file,
    MachineTraceFile, machine_history_build_from_machine_trace_file)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_trace_report,
    MachineTraceReport, machine_history_build_from_machine_trace_report)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_delta_file,
    MachineDeltaFile, machine_history_build_from_machine_delta_file)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_delta_report,
    MachineDeltaReport, machine_history_build_from_machine_delta_report)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_observe_file,
    MachineObserveFile, machine_history_build_from_machine_observe_file)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_observe_report,
    MachineObserveReport, machine_history_build_from_machine_observe_report)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_apply_file,
    MachineApplyFile, machine_history_build_from_machine_apply_file)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_apply_report,
    MachineApplyReport, machine_history_build_from_machine_apply_report)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_commit_file,
    MachineCommitFile, machine_history_build_from_machine_commit_file)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_commit_report,
    MachineCommitReport, machine_history_build_from_machine_commit_report)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_step_file,
    MachineStepFile, machine_history_build_from_machine_step_file)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_step_report,
    MachineStepReport, machine_history_build_from_machine_step_report)
MACHINE_HISTORY_DEFINE_REPORT_FROM_WRAPPER(machine_history_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_history_build_from_machine_ir_report)

int machine_history_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineHistoryReport *out_report,
    MachineHistoryError *error) {
    MachineHistoryFile history_file;
    int ok;

    machine_history_file_init(&history_file);
    ok = machine_history_build_from_machine_ir_report_with_profile(report, profile, &history_file, error) &&
        machine_history_build_report_from_file(&history_file, out_report, error);
    machine_history_file_free(&history_file);
    return ok;
}

int machine_history_report_refresh(
    MachineHistoryReport *report,
    MachineHistoryError *error) {
    MachineHistoryReport refreshed_report;
    int ok;

    if (!report) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-121: invalid report refresh contract");
        return 0;
    }
    machine_history_report_init(&refreshed_report);
    ok = machine_history_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_history_report_free(report);
        *report = refreshed_report;
    } else {
        machine_history_report_free(&refreshed_report);
    }
    return ok;
}

int machine_history_dump_file(
    const MachineHistoryFile *history_file,
    char **out_text,
    MachineHistoryError *error) {
    MachineHistoryStringBuilder builder;
    MachineHistoryHeaderSummary header_summary;
    MachineHistorySummary history_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&history_summary, 0, sizeof(history_summary));
    if (!history_file || !out_text ||
        !machine_history_verify_file(history_file, error) ||
        !machine_history_file_get_header_summary(history_file, &header_summary) ||
        !machine_history_file_get_history_summary(history_file, &history_summary)) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-122: invalid dump contract");
        return 0;
    }

    if (!machine_history_append_format(
            &builder,
            "machine_history profile=%s outcome=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu entries=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_outcome_resolution_kind_name(header_summary.outcome_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count,
            header_summary.history_entry_count) ||
        !machine_history_append_format(
            &builder,
            "history: resolution=%s kind=%s outcome=%s event=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_history_resolution_kind_name(history_summary.resolution_kind),
            machine_history_kind_name(history_summary.history_kind),
            machine_outcome_kind_name(history_summary.outcome_kind),
            machine_event_kind_name(history_summary.event_kind),
            machine_trace_resolution_kind_name(history_summary.trace_resolution_kind),
            machine_trace_change_class_name(history_summary.trace_change_class),
            machine_apply_resolution_kind_name(history_summary.apply_resolution_kind),
            machine_commit_resolution_kind_name(history_summary.commit_resolution_kind),
            machine_writeback_resolution_kind_name(history_summary.writeback_resolution_kind),
            machine_mutation_resolution_kind_name(history_summary.mutation_resolution_kind),
            machine_mutation_effect_kind_name(history_summary.mutation_effect_kind),
            machine_transition_resolution_kind_name(history_summary.transition_resolution_kind),
            machine_interp_action_kind_name(history_summary.action_kind),
            (unsigned int)history_summary.raw_byte,
            (unsigned int)history_summary.tag_value,
            history_summary.is_known ? "yes" : "no",
            history_summary.tag_name ? history_summary.tag_name : "-",
            history_summary.instruction_byte_count) ||
        !machine_history_append_payload_bytes(&builder, &history_summary) ||
        !machine_history_append_immediate_hint(&builder, &history_summary) ||
        !machine_history_append_format(
            &builder,
            " exact=%s single-entry=%s entries=%zu origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            history_summary.is_exact_history ? "yes" : "no",
            history_summary.is_single_entry_history ? "yes" : "no",
            history_summary.history_entry_count,
            machine_step_status_name(history_summary.origin_status),
            history_summary.has_observed_state ? machine_step_status_name(history_summary.observed_status) : "-",
            history_summary.has_status_change ? (history_summary.status_changed ? "yes" : "no") : "-",
            history_summary.has_program_counter_change ? (history_summary.program_counter_changed ? "yes" : "no") : "-",
            history_summary.has_stack_pointer_change ? (history_summary.stack_pointer_changed ? "yes" : "no") : "-",
            history_summary.has_fetch_change ? (history_summary.fetch_changed ? "yes" : "no") : "-") ||
        !machine_history_append_targets(&builder, &history_summary) ||
        !machine_history_append_return_immediate(&builder, &history_summary) ||
        !machine_history_append_format(&builder, "\n")) {
        free(builder.data);
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-123: out of memory building dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_history_build_dump_from_file(
    const MachineHistoryFile *source,
    char **out_text,
    MachineHistoryError *error) {
    return machine_history_dump_file(source, out_text, error);
}

#define MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineHistoryError *error) { \
    MachineHistoryFile history_file; \
    int ok; \
    machine_history_file_init(&history_file); \
    ok = builder(source, &history_file, error) && \
        machine_history_dump_file(&history_file, out_text, error); \
    machine_history_file_free(&history_file); \
    return ok; \
}

MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_outcome_file,
    MachineOutcomeFile, machine_history_build_from_machine_outcome_file)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_outcome_report,
    MachineOutcomeReport, machine_history_build_from_machine_outcome_report)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_event_file,
    MachineEventFile, machine_history_build_from_machine_event_file)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_event_report,
    MachineEventReport, machine_history_build_from_machine_event_report)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_trace_file,
    MachineTraceFile, machine_history_build_from_machine_trace_file)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_trace_report,
    MachineTraceReport, machine_history_build_from_machine_trace_report)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_delta_file,
    MachineDeltaFile, machine_history_build_from_machine_delta_file)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_delta_report,
    MachineDeltaReport, machine_history_build_from_machine_delta_report)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_observe_file,
    MachineObserveFile, machine_history_build_from_machine_observe_file)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_observe_report,
    MachineObserveReport, machine_history_build_from_machine_observe_report)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_apply_file,
    MachineApplyFile, machine_history_build_from_machine_apply_file)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_apply_report,
    MachineApplyReport, machine_history_build_from_machine_apply_report)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_commit_file,
    MachineCommitFile, machine_history_build_from_machine_commit_file)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_commit_report,
    MachineCommitReport, machine_history_build_from_machine_commit_report)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_step_file,
    MachineStepFile, machine_history_build_from_machine_step_file)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_step_report,
    MachineStepReport, machine_history_build_from_machine_step_report)
MACHINE_HISTORY_DEFINE_DUMP_FROM_WRAPPER(machine_history_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_history_build_from_machine_ir_report)

int machine_history_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineHistoryError *error) {
    MachineHistoryFile history_file;
    int ok;

    machine_history_file_init(&history_file);
    ok = machine_history_build_from_machine_ir_report_with_profile(report, profile, &history_file, error) &&
        machine_history_dump_file(&history_file, out_text, error);
    machine_history_file_free(&history_file);
    return ok;
}

int machine_history_report_get_summary(
    const MachineHistoryReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_history_report_get_overview_artifact(
    const MachineHistoryReport *report,
    MachineHistoryReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->outcome_report = &report->outcome_report;
    out_artifact->history_summary = &report->history_summary;
    return 1;
}

int machine_history_report_get_file(
    const MachineHistoryReport *report,
    const MachineHistoryFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_history_report_get_outcome_file(
    const MachineHistoryReport *report,
    const MachineOutcomeFile **out_outcome_file) {
    if (!report || !out_outcome_file) {
        return 0;
    }
    *out_outcome_file = &report->file.outcome_file;
    return 1;
}

int machine_history_report_get_outcome_report(
    const MachineHistoryReport *report,
    const MachineOutcomeReport **out_outcome_report) {
    if (!report || !out_outcome_report) {
        return 0;
    }
    *out_outcome_report = &report->outcome_report;
    return 1;
}

#define MACHINE_HISTORY_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineHistoryReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_HISTORY_DEFINE_REPORT_ARTIFACT_GETTER(machine_history_report_get_header_summary_artifact,
    header_summary, MachineHistoryHeaderSummary)
MACHINE_HISTORY_DEFINE_REPORT_ARTIFACT_GETTER(machine_history_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineHistoryTargetPolicySummary)
MACHINE_HISTORY_DEFINE_REPORT_ARTIFACT_GETTER(machine_history_report_get_history_summary_artifact,
    history_summary, MachineHistorySummary)

int machine_history_report_overview_artifact_get_outcome_report(
    const MachineHistoryReportOverviewArtifact *artifact,
    const MachineOutcomeReport **out_outcome_report) {
    if (!artifact || !out_outcome_report) {
        return 0;
    }
    *out_outcome_report = artifact->outcome_report;
    return 1;
}

int machine_history_report_overview_artifact_get_history_summary_artifact(
    const MachineHistoryReportOverviewArtifact *artifact,
    const MachineHistorySummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->history_summary;
    return 1;
}

int machine_history_dump_report(
    const MachineHistoryReport *report,
    char **out_text,
    MachineHistoryError *error) {
    MachineHistoryStringBuilder builder;
    const MachineHistorySummary *history_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_history_report_get_history_summary_artifact(report, &history_summary) ||
        !history_summary) {
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-124: invalid report dump contract");
        return 0;
    }

    if (!machine_history_append_format(
            &builder,
            "machine_history profile=%s outcome=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu entries=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_outcome_resolution_kind_name(report->header_summary.outcome_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.history_entry_count) ||
        !machine_history_append_format(
            &builder,
            "history: resolution=%s kind=%s outcome=%s event=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_history_resolution_kind_name(history_summary->resolution_kind),
            machine_history_kind_name(history_summary->history_kind),
            machine_outcome_kind_name(history_summary->outcome_kind),
            machine_event_kind_name(history_summary->event_kind),
            machine_trace_resolution_kind_name(history_summary->trace_resolution_kind),
            machine_trace_change_class_name(history_summary->trace_change_class),
            machine_apply_resolution_kind_name(history_summary->apply_resolution_kind),
            machine_commit_resolution_kind_name(history_summary->commit_resolution_kind),
            machine_writeback_resolution_kind_name(history_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(history_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(history_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(history_summary->transition_resolution_kind),
            machine_interp_action_kind_name(history_summary->action_kind),
            (unsigned int)history_summary->raw_byte,
            (unsigned int)history_summary->tag_value,
            history_summary->is_known ? "yes" : "no",
            history_summary->tag_name ? history_summary->tag_name : "-",
            history_summary->instruction_byte_count) ||
        !machine_history_append_payload_bytes(&builder, history_summary) ||
        !machine_history_append_immediate_hint(&builder, history_summary) ||
        !machine_history_append_format(
            &builder,
            " exact=%s single-entry=%s entries=%zu origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            history_summary->is_exact_history ? "yes" : "no",
            history_summary->is_single_entry_history ? "yes" : "no",
            history_summary->history_entry_count,
            machine_step_status_name(history_summary->origin_status),
            history_summary->has_observed_state ? machine_step_status_name(history_summary->observed_status) : "-",
            history_summary->has_status_change ? (history_summary->status_changed ? "yes" : "no") : "-",
            history_summary->has_program_counter_change ? (history_summary->program_counter_changed ? "yes" : "no") : "-",
            history_summary->has_stack_pointer_change ? (history_summary->stack_pointer_changed ? "yes" : "no") : "-",
            history_summary->has_fetch_change ? (history_summary->fetch_changed ? "yes" : "no") : "-") ||
        !machine_history_append_targets(&builder, history_summary) ||
        !machine_history_append_return_immediate(&builder, history_summary) ||
        !machine_history_append_format(&builder, "\nreport_overview:\n") ||
        !machine_history_append_format(
            &builder,
            "  origin: outcome=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx entries=%zu\n",
            machine_outcome_resolution_kind_name(report->header_summary.outcome_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.history_entry_count) ||
        !machine_history_append_format(
            &builder,
            "  policy: profile=%s exact=%s preview=%s single-entry=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.surfaces_exact_history ? "yes" : "no",
            report->target_policy_summary.surfaces_preview_history ? "yes" : "no",
            report->target_policy_summary.surfaces_single_entry_history ? "yes" : "no") ||
        !machine_history_append_format(
            &builder,
            "  history: resolution=%s kind=%s outcome=%s exact=%s single-entry=%s entries=%zu state=%s status=%s pc=%s fetch=%s",
            machine_history_resolution_kind_name(history_summary->resolution_kind),
            machine_history_kind_name(history_summary->history_kind),
            machine_outcome_kind_name(history_summary->outcome_kind),
            history_summary->is_exact_history ? "yes" : "no",
            history_summary->is_single_entry_history ? "yes" : "no",
            history_summary->history_entry_count,
            history_summary->has_observed_state ? "yes" : "no",
            history_summary->has_status_change ? (history_summary->status_changed ? "yes" : "no") : "-",
            history_summary->has_program_counter_change ? (history_summary->program_counter_changed ? "yes" : "no") : "-",
            history_summary->has_fetch_change ? (history_summary->fetch_changed ? "yes" : "no") : "-")) {
        free(builder.data);
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-125: out of memory building report dump");
        return 0;
    }
    if (!machine_history_append_targets(&builder, history_summary) ||
        !machine_history_append_return_immediate(&builder, history_summary) ||
        !machine_history_append_format(&builder, "\n")) {
        free(builder.data);
        machine_history_set_error(error, 0, 0, "MACHINE-HISTORY-126: out of memory terminating report dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_history_build_report_dump_from_file(
    const MachineHistoryFile *source,
    char **out_text,
    MachineHistoryError *error) {
    MachineHistoryReport report;
    int ok;

    machine_history_report_init(&report);
    ok = machine_history_build_report_from_file(source, &report, error) &&
        machine_history_dump_report(&report, out_text, error);
    machine_history_report_free(&report);
    return ok;
}

#define MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineHistoryError *error) { \
    MachineHistoryReport report; \
    int ok; \
    machine_history_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_history_dump_report(&report, out_text, error); \
    machine_history_report_free(&report); \
    return ok; \
}

MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_outcome_file,
    MachineOutcomeFile, machine_history_build_report_from_machine_outcome_file)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_outcome_report,
    MachineOutcomeReport, machine_history_build_report_from_machine_outcome_report)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_event_file,
    MachineEventFile, machine_history_build_report_from_machine_event_file)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_event_report,
    MachineEventReport, machine_history_build_report_from_machine_event_report)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_trace_file,
    MachineTraceFile, machine_history_build_report_from_machine_trace_file)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_trace_report,
    MachineTraceReport, machine_history_build_report_from_machine_trace_report)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_delta_file,
    MachineDeltaFile, machine_history_build_report_from_machine_delta_file)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_delta_report,
    MachineDeltaReport, machine_history_build_report_from_machine_delta_report)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_observe_file,
    MachineObserveFile, machine_history_build_report_from_machine_observe_file)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_observe_report,
    MachineObserveReport, machine_history_build_report_from_machine_observe_report)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_apply_file,
    MachineApplyFile, machine_history_build_report_from_machine_apply_file)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_apply_report,
    MachineApplyReport, machine_history_build_report_from_machine_apply_report)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_commit_file,
    MachineCommitFile, machine_history_build_report_from_machine_commit_file)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_commit_report,
    MachineCommitReport, machine_history_build_report_from_machine_commit_report)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_history_build_report_from_machine_step_file)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_history_build_report_from_machine_step_report)
MACHINE_HISTORY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_history_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_history_build_report_from_machine_ir_report)

int machine_history_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineHistoryError *error) {
    MachineHistoryReport report_file;
    int ok;

    machine_history_report_init(&report_file);
    ok = machine_history_build_report_from_machine_ir_report_with_profile(
            report, profile, &report_file, error) &&
        machine_history_dump_report(&report_file, out_text, error);
    machine_history_report_free(&report_file);
    return ok;
}
