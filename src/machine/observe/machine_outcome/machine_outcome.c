#include "machine/outcome.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineOutcomeStringBuilder;

static void machine_outcome_set_error(
    MachineOutcomeError *error,
    int line,
    int column,
    const char *fmt,
    ...);
static int machine_outcome_append_format(
    MachineOutcomeStringBuilder *builder,
    const char *fmt,
    ...);
static int machine_outcome_append_targets(
    MachineOutcomeStringBuilder *builder,
    const MachineOutcomeSummary *outcome_summary);
static int machine_outcome_append_return_immediate(
    MachineOutcomeStringBuilder *builder,
    const MachineOutcomeSummary *outcome_summary);
static int machine_outcome_append_payload_bytes(
    MachineOutcomeStringBuilder *builder,
    const MachineOutcomeSummary *outcome_summary);
static int machine_outcome_append_immediate_hint(
    MachineOutcomeStringBuilder *builder,
    const MachineOutcomeSummary *outcome_summary);
static MachineOutcomeKind machine_outcome_classify_kind(
    const MachineEventSummary *event_summary);

static void machine_outcome_set_error(
    MachineOutcomeError *error,
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

static int machine_outcome_append_format(
    MachineOutcomeStringBuilder *builder,
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

static int machine_outcome_append_targets(
    MachineOutcomeStringBuilder *builder,
    const MachineOutcomeSummary *outcome_summary) {
    if (!builder || !outcome_summary) {
        return 0;
    }
    if (!outcome_summary->has_primary_target_block) {
        return machine_outcome_append_format(builder, " targets=[]");
    }
    if (!outcome_summary->has_secondary_target_block) {
        return machine_outcome_append_format(
            builder, " targets=[%zu]", outcome_summary->primary_target_block_index);
    }
    return machine_outcome_append_format(
        builder,
        " targets=[%zu,%zu]",
        outcome_summary->primary_target_block_index,
        outcome_summary->secondary_target_block_index);
}

static int machine_outcome_append_return_immediate(
    MachineOutcomeStringBuilder *builder,
    const MachineOutcomeSummary *outcome_summary) {
    if (!builder || !outcome_summary) {
        return 0;
    }
    if (!outcome_summary->has_return_immediate) {
        return machine_outcome_append_format(builder, " return-imm=-");
    }
    return machine_outcome_append_format(builder, " return-imm=%zu", outcome_summary->return_immediate);
}

static int machine_outcome_append_payload_bytes(
    MachineOutcomeStringBuilder *builder,
    const MachineOutcomeSummary *outcome_summary) {
    size_t index;

    if (!builder || !outcome_summary) {
        return 0;
    }
    if (!machine_outcome_append_format(builder, " payload=[")) {
        return 0;
    }
    for (index = 0u; index < outcome_summary->payload_byte_count; ++index) {
        if (!machine_outcome_append_format(
                builder,
                "%s0x%02x",
                index == 0u ? "" : ",",
                (unsigned int)outcome_summary->payload_bytes[index])) {
            return 0;
        }
    }
    return machine_outcome_append_format(builder, "]");
}

static int machine_outcome_append_immediate_hint(
    MachineOutcomeStringBuilder *builder,
    const MachineOutcomeSummary *outcome_summary) {
    if (!builder || !outcome_summary) {
        return 0;
    }
    if (!outcome_summary->has_immediate_hint) {
        return machine_outcome_append_format(builder, " imm=-");
    }
    return machine_outcome_append_format(builder, " imm=%zu", outcome_summary->immediate_hint);
}

static MachineOutcomeKind machine_outcome_classify_kind(
    const MachineEventSummary *event_summary) {
    if (!event_summary) {
        return MACHINE_OUTCOME_KIND_NONE;
    }
    if (event_summary->resolution_kind == MACHINE_EVENT_RESOLUTION_BLOCKED_ON_CONTROL) {
        return MACHINE_OUTCOME_KIND_BLOCKED_CONTROL;
    }
    if (event_summary->resolution_kind == MACHINE_EVENT_RESOLUTION_BLOCKED_UNSUPPORTED) {
        return MACHINE_OUTCOME_KIND_BLOCKED_UNSUPPORTED;
    }
    switch (event_summary->event_kind) {
        case MACHINE_EVENT_KIND_CONTROL_HALT:
            return MACHINE_OUTCOME_KIND_PROGRAM_STOPPED;
        case MACHINE_EVENT_KIND_REGISTER_RESULT:
            return MACHINE_OUTCOME_KIND_VALUE_AVAILABLE;
        case MACHINE_EVENT_KIND_LOCAL_STORE:
            return MACHINE_OUTCOME_KIND_LOCAL_UPDATED;
        case MACHINE_EVENT_KIND_GLOBAL_STORE:
            return MACHINE_OUTCOME_KIND_GLOBAL_UPDATED;
        case MACHINE_EVENT_KIND_CALL_EFFECT:
            return MACHINE_OUTCOME_KIND_CALL_ISSUED;
        case MACHINE_EVENT_KIND_BLOCKED_CONTROL:
            return MACHINE_OUTCOME_KIND_BLOCKED_CONTROL;
        case MACHINE_EVENT_KIND_BLOCKED_UNSUPPORTED:
            return MACHINE_OUTCOME_KIND_BLOCKED_UNSUPPORTED;
        case MACHINE_EVENT_KIND_NONE:
        default:
            return MACHINE_OUTCOME_KIND_NONE;
    }
}

void machine_outcome_file_init(MachineOutcomeFile *outcome_file) {
    if (!outcome_file) {
        return;
    }
    machine_event_file_init(&outcome_file->event_file);
    outcome_file->resolution_kind = MACHINE_OUTCOME_RESOLUTION_NONE;
    outcome_file->outcome_kind = MACHINE_OUTCOME_KIND_NONE;
}

void machine_outcome_file_free(MachineOutcomeFile *outcome_file) {
    if (!outcome_file) {
        return;
    }
    machine_event_file_free(&outcome_file->event_file);
    machine_outcome_file_init(outcome_file);
}

void machine_outcome_report_init(MachineOutcomeReport *report) {
    if (!report) {
        return;
    }
    machine_outcome_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_event_report_init(&report->event_report);
    memset(&report->outcome_summary, 0, sizeof(report->outcome_summary));
}

void machine_outcome_report_free(MachineOutcomeReport *report) {
    if (!report) {
        return;
    }
    machine_event_report_free(&report->event_report);
    machine_outcome_file_free(&report->file);
    machine_outcome_report_init(report);
}

const char *machine_outcome_resolution_kind_name(MachineOutcomeResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_OUTCOME_RESOLUTION_NONE:
            return "none";
        case MACHINE_OUTCOME_RESOLUTION_EXACT_OUTCOME:
            return "exact-outcome";
        case MACHINE_OUTCOME_RESOLUTION_PREVIEW_OUTCOME:
            return "preview-outcome";
        case MACHINE_OUTCOME_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_OUTCOME_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_outcome_kind_name(MachineOutcomeKind outcome_kind) {
    switch (outcome_kind) {
        case MACHINE_OUTCOME_KIND_NONE:
            return "none";
        case MACHINE_OUTCOME_KIND_PROGRAM_STOPPED:
            return "program-stopped";
        case MACHINE_OUTCOME_KIND_VALUE_AVAILABLE:
            return "value-available";
        case MACHINE_OUTCOME_KIND_LOCAL_UPDATED:
            return "local-updated";
        case MACHINE_OUTCOME_KIND_GLOBAL_UPDATED:
            return "global-updated";
        case MACHINE_OUTCOME_KIND_CALL_ISSUED:
            return "call-issued";
        case MACHINE_OUTCOME_KIND_BLOCKED_CONTROL:
            return "blocked-control";
        case MACHINE_OUTCOME_KIND_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

int machine_outcome_get_target_policy_summary(
    MachineElfTargetProfile profile,
    MachineOutcomeTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->surfaces_exact_outcome = 1;
    out_summary->surfaces_preview_outcome = 1;
    out_summary->classifies_outcome_family = 1;
    return 1;
}

int machine_outcome_file_get_target_policy_summary(
    const MachineOutcomeFile *outcome_file,
    MachineOutcomeTargetPolicySummary *out_summary) {
    if (!outcome_file || !out_summary) {
        return 0;
    }
    return machine_outcome_get_target_policy_summary(
        outcome_file->event_file.trace_file.delta_file.observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_outcome_file_get_summary(
    const MachineOutcomeFile *outcome_file,
    size_t *out_mapped_byte_count) {
    if (!outcome_file) {
        return 0;
    }
    return machine_event_file_get_summary(&outcome_file->event_file, out_mapped_byte_count);
}

int machine_outcome_file_get_header_summary(
    const MachineOutcomeFile *outcome_file,
    MachineOutcomeHeaderSummary *out_summary) {
    MachineEventHeaderSummary event_header_summary;

    memset(&event_header_summary, 0, sizeof(event_header_summary));
    if (!outcome_file || !out_summary ||
        !machine_event_file_get_header_summary(&outcome_file->event_file, &event_header_summary)) {
        return 0;
    }
    out_summary->target_profile = event_header_summary.target_profile;
    out_summary->event_resolution_kind = outcome_file->event_file.resolution_kind;
    out_summary->origin_step_status = event_header_summary.origin_step_status;
    out_summary->origin_program_counter = event_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = event_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = event_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = event_header_summary.mapped_byte_count;
    return 1;
}

int machine_outcome_file_get_event_summary(
    const MachineOutcomeFile *outcome_file,
    MachineEventSummary *out_summary) {
    if (!outcome_file || !out_summary) {
        return 0;
    }
    return machine_event_file_get_event_summary(&outcome_file->event_file, out_summary);
}

int machine_outcome_file_get_outcome_summary(
    const MachineOutcomeFile *outcome_file,
    MachineOutcomeSummary *out_summary) {
    MachineEventSummary event_summary;

    memset(&event_summary, 0, sizeof(event_summary));
    if (!outcome_file || !out_summary ||
        !machine_outcome_file_get_event_summary(outcome_file, &event_summary)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->resolution_kind = outcome_file->resolution_kind;
    out_summary->outcome_kind = outcome_file->outcome_kind;
    out_summary->event_resolution_kind = event_summary.resolution_kind;
    out_summary->event_kind = event_summary.event_kind;
    out_summary->trace_resolution_kind = event_summary.trace_resolution_kind;
    out_summary->trace_kind = event_summary.trace_kind;
    out_summary->trace_change_class = event_summary.trace_change_class;
    out_summary->delta_resolution_kind = event_summary.delta_resolution_kind;
    out_summary->delta_kind = event_summary.delta_kind;
    out_summary->observe_resolution_kind = event_summary.observe_resolution_kind;
    out_summary->observe_kind = event_summary.observe_kind;
    out_summary->apply_resolution_kind = event_summary.apply_resolution_kind;
    out_summary->apply_kind = event_summary.apply_kind;
    out_summary->commit_resolution_kind = event_summary.commit_resolution_kind;
    out_summary->commit_kind = event_summary.commit_kind;
    out_summary->writeback_resolution_kind = event_summary.writeback_resolution_kind;
    out_summary->writeback_commit_kind = event_summary.writeback_commit_kind;
    out_summary->mutation_resolution_kind = event_summary.mutation_resolution_kind;
    out_summary->mutation_effect_kind = event_summary.mutation_effect_kind;
    out_summary->state_resolution_kind = event_summary.state_resolution_kind;
    out_summary->transition_resolution_kind = event_summary.transition_resolution_kind;
    out_summary->action_kind = event_summary.action_kind;
    out_summary->payload_kind = event_summary.payload_kind;
    out_summary->tag_class = event_summary.tag_class;
    out_summary->raw_byte = event_summary.raw_byte;
    out_summary->tag_value = event_summary.tag_value;
    out_summary->is_known = event_summary.is_known;
    out_summary->tag_name = event_summary.tag_name;
    out_summary->instruction_byte_count = event_summary.instruction_byte_count;
    out_summary->payload_byte_count = event_summary.payload_byte_count;
    memcpy(out_summary->payload_bytes, event_summary.payload_bytes, sizeof(out_summary->payload_bytes));
    out_summary->has_immediate_hint = event_summary.has_immediate_hint;
    out_summary->immediate_hint = event_summary.immediate_hint;
    out_summary->is_exact_outcome = outcome_file->resolution_kind == MACHINE_OUTCOME_RESOLUTION_EXACT_OUTCOME;
    out_summary->origin_status = event_summary.origin_status;
    out_summary->observed_status = event_summary.observed_status;
    out_summary->has_observed_state = event_summary.has_observed_state;
    out_summary->has_status_change = event_summary.has_status_change;
    out_summary->status_changed = event_summary.status_changed;
    out_summary->has_program_counter_change = event_summary.has_program_counter_change;
    out_summary->program_counter_changed = event_summary.program_counter_changed;
    out_summary->has_stack_pointer_change = event_summary.has_stack_pointer_change;
    out_summary->stack_pointer_changed = event_summary.stack_pointer_changed;
    out_summary->has_fetch_change = event_summary.has_fetch_change;
    out_summary->fetch_changed = event_summary.fetch_changed;
    out_summary->has_primary_target_block = event_summary.has_primary_target_block;
    out_summary->primary_target_block_index = event_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = event_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = event_summary.secondary_target_block_index;
    out_summary->has_return_immediate = event_summary.has_return_immediate;
    out_summary->return_immediate = event_summary.return_immediate;
    return 1;
}

int machine_outcome_verify_file(
    const MachineOutcomeFile *outcome_file,
    MachineOutcomeError *error) {
    MachineEventSummary event_summary;
    MachineOutcomeKind expected_outcome_kind;

    memset(&event_summary, 0, sizeof(event_summary));
    if (!outcome_file) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-100: invalid outcome file");
        return 0;
    }
    if (!machine_event_verify_file(&outcome_file->event_file, NULL) ||
        !machine_outcome_file_get_event_summary(outcome_file, &event_summary)) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-101: invalid event input");
        return 0;
    }

    expected_outcome_kind = machine_outcome_classify_kind(&event_summary);
    if (outcome_file->outcome_kind != expected_outcome_kind) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-102: invalid outcome classification");
        return 0;
    }

    switch (outcome_file->resolution_kind) {
        case MACHINE_OUTCOME_RESOLUTION_EXACT_OUTCOME:
            if (outcome_file->event_file.resolution_kind != MACHINE_EVENT_RESOLUTION_EXACT_EVENT ||
                expected_outcome_kind == MACHINE_OUTCOME_KIND_BLOCKED_CONTROL ||
                expected_outcome_kind == MACHINE_OUTCOME_KIND_BLOCKED_UNSUPPORTED ||
                expected_outcome_kind == MACHINE_OUTCOME_KIND_NONE) {
                machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-103: invalid exact outcome");
                return 0;
            }
            break;
        case MACHINE_OUTCOME_RESOLUTION_PREVIEW_OUTCOME:
            if (outcome_file->event_file.resolution_kind != MACHINE_EVENT_RESOLUTION_PREVIEW_EVENT ||
                expected_outcome_kind == MACHINE_OUTCOME_KIND_BLOCKED_CONTROL ||
                expected_outcome_kind == MACHINE_OUTCOME_KIND_BLOCKED_UNSUPPORTED ||
                expected_outcome_kind == MACHINE_OUTCOME_KIND_NONE) {
                machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-104: invalid preview outcome");
                return 0;
            }
            break;
        case MACHINE_OUTCOME_RESOLUTION_BLOCKED_ON_CONTROL:
            if (outcome_file->event_file.resolution_kind != MACHINE_EVENT_RESOLUTION_BLOCKED_ON_CONTROL ||
                expected_outcome_kind != MACHINE_OUTCOME_KIND_BLOCKED_CONTROL) {
                machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-105: invalid blocked-control outcome");
                return 0;
            }
            break;
        case MACHINE_OUTCOME_RESOLUTION_BLOCKED_UNSUPPORTED:
            if (outcome_file->event_file.resolution_kind != MACHINE_EVENT_RESOLUTION_BLOCKED_UNSUPPORTED ||
                expected_outcome_kind != MACHINE_OUTCOME_KIND_BLOCKED_UNSUPPORTED) {
                machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-106: invalid blocked-unsupported outcome");
                return 0;
            }
            break;
        case MACHINE_OUTCOME_RESOLUTION_NONE:
        default:
            machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-107: invalid outcome resolution kind");
            return 0;
    }
    return 1;
}

int machine_outcome_clone_file(
    const MachineOutcomeFile *source,
    MachineOutcomeFile *out_clone,
    MachineOutcomeError *error) {
    if (!source || !out_clone) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-108: invalid clone contract");
        return 0;
    }
    if (!machine_outcome_verify_file(source, error)) {
        return 0;
    }

    machine_outcome_file_free(out_clone);
    if (!machine_event_clone_file(&source->event_file, &out_clone->event_file, NULL)) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-109: failed cloning event input");
        machine_outcome_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->outcome_kind = source->outcome_kind;
    return 1;
}

int machine_outcome_build_from_machine_event_file(
    const MachineEventFile *event_file,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error) {
    MachineEventSummary event_summary;

    memset(&event_summary, 0, sizeof(event_summary));
    if (!event_file || !out_outcome_file) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-110: invalid build contract");
        return 0;
    }
    if (!machine_event_verify_file(event_file, NULL) ||
        !machine_event_file_get_event_summary(event_file, &event_summary)) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-111: invalid event input");
        return 0;
    }

    machine_outcome_file_free(out_outcome_file);
    if (!machine_event_clone_file(event_file, &out_outcome_file->event_file, NULL)) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-112: failed cloning event input");
        machine_outcome_file_free(out_outcome_file);
        return 0;
    }

    out_outcome_file->outcome_kind = machine_outcome_classify_kind(&event_summary);
    switch (event_file->resolution_kind) {
        case MACHINE_EVENT_RESOLUTION_EXACT_EVENT:
            out_outcome_file->resolution_kind = MACHINE_OUTCOME_RESOLUTION_EXACT_OUTCOME;
            break;
        case MACHINE_EVENT_RESOLUTION_PREVIEW_EVENT:
            out_outcome_file->resolution_kind = MACHINE_OUTCOME_RESOLUTION_PREVIEW_OUTCOME;
            break;
        case MACHINE_EVENT_RESOLUTION_BLOCKED_ON_CONTROL:
            out_outcome_file->resolution_kind = MACHINE_OUTCOME_RESOLUTION_BLOCKED_ON_CONTROL;
            break;
        case MACHINE_EVENT_RESOLUTION_BLOCKED_UNSUPPORTED:
            out_outcome_file->resolution_kind = MACHINE_OUTCOME_RESOLUTION_BLOCKED_UNSUPPORTED;
            break;
        case MACHINE_EVENT_RESOLUTION_NONE:
        default:
            machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-113: unsupported event resolution");
            machine_outcome_file_free(out_outcome_file);
            return 0;
    }

    if (!machine_outcome_verify_file(out_outcome_file, error)) {
        machine_outcome_file_free(out_outcome_file);
        return 0;
    }
    return 1;
}

int machine_outcome_build_from_machine_event_report(
    const MachineEventReport *report,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error) {
    const MachineEventFile *event_file = NULL;

    if (!report) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-114: invalid build-from-event-report contract");
        return 0;
    }
    if (!machine_event_report_get_file(report, &event_file) || !event_file) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-115: failed recovering event file from report");
        return 0;
    }
    return machine_outcome_build_from_machine_event_file(event_file, out_outcome_file, error);
}

#define MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineOutcomeFile *out_outcome_file, MachineOutcomeError *error) { \
    MachineEventFile event_file; \
    int ok; \
    machine_event_file_init(&event_file); \
    ok = builder(source, &event_file, NULL) && \
        machine_outcome_build_from_machine_event_file(&event_file, out_outcome_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-116: failed building outcome wrapper"); \
    } \
    machine_event_file_free(&event_file); \
    return ok; \
}

MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_trace_file,
    MachineTraceFile, machine_event_build_from_machine_trace_file)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_trace_report,
    MachineTraceReport, machine_event_build_from_machine_trace_report)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_delta_file,
    MachineDeltaFile, machine_event_build_from_machine_delta_file)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_delta_report,
    MachineDeltaReport, machine_event_build_from_machine_delta_report)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_observe_file,
    MachineObserveFile, machine_event_build_from_machine_observe_file)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_observe_report,
    MachineObserveReport, machine_event_build_from_machine_observe_report)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_apply_file,
    MachineApplyFile, machine_event_build_from_machine_apply_file)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_apply_report,
    MachineApplyReport, machine_event_build_from_machine_apply_report)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_commit_file,
    MachineCommitFile, machine_event_build_from_machine_commit_file)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_commit_report,
    MachineCommitReport, machine_event_build_from_machine_commit_report)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_step_file,
    MachineStepFile, machine_event_build_from_machine_step_file)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_step_report,
    MachineStepReport, machine_event_build_from_machine_step_report)
MACHINE_OUTCOME_DEFINE_BUILD_FROM_WRAPPER(machine_outcome_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_event_build_from_machine_ir_report)

int machine_outcome_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error) {
    MachineEventFile event_file;
    int ok;

    machine_event_file_init(&event_file);
    ok = machine_event_build_from_machine_ir_report_with_profile(report, profile, &event_file, NULL) &&
        machine_outcome_build_from_machine_event_file(&event_file, out_outcome_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-117: failed building profiled outcome wrapper");
    }
    machine_event_file_free(&event_file);
    return ok;
}

int machine_outcome_build_report_from_file(
    const MachineOutcomeFile *source,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error) {
    if (!source || !out_report) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-118: invalid report build contract");
        return 0;
    }
    if (!machine_outcome_verify_file(source, error)) {
        return 0;
    }

    machine_outcome_report_free(out_report);
    if (!machine_outcome_clone_file(source, &out_report->file, error)) {
        machine_outcome_report_free(out_report);
        return 0;
    }
    if (!machine_outcome_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_outcome_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_event_build_report_from_file(&out_report->file.event_file, &out_report->event_report, NULL) ||
        !machine_outcome_file_get_outcome_summary(&out_report->file, &out_report->outcome_summary)) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-119: failed summarizing outcome report");
        machine_outcome_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineOutcomeReport *out_report, MachineOutcomeError *error) { \
    MachineOutcomeFile outcome_file; \
    int ok; \
    machine_outcome_file_init(&outcome_file); \
    ok = builder(source, &outcome_file, error) && \
        machine_outcome_build_report_from_file(&outcome_file, out_report, error); \
    machine_outcome_file_free(&outcome_file); \
    return ok; \
}

MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_event_file,
    MachineEventFile, machine_outcome_build_from_machine_event_file)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_event_report,
    MachineEventReport, machine_outcome_build_from_machine_event_report)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_trace_file,
    MachineTraceFile, machine_outcome_build_from_machine_trace_file)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_trace_report,
    MachineTraceReport, machine_outcome_build_from_machine_trace_report)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_delta_file,
    MachineDeltaFile, machine_outcome_build_from_machine_delta_file)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_delta_report,
    MachineDeltaReport, machine_outcome_build_from_machine_delta_report)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_observe_file,
    MachineObserveFile, machine_outcome_build_from_machine_observe_file)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_observe_report,
    MachineObserveReport, machine_outcome_build_from_machine_observe_report)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_apply_file,
    MachineApplyFile, machine_outcome_build_from_machine_apply_file)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_apply_report,
    MachineApplyReport, machine_outcome_build_from_machine_apply_report)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_commit_file,
    MachineCommitFile, machine_outcome_build_from_machine_commit_file)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_commit_report,
    MachineCommitReport, machine_outcome_build_from_machine_commit_report)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_step_file,
    MachineStepFile, machine_outcome_build_from_machine_step_file)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_step_report,
    MachineStepReport, machine_outcome_build_from_machine_step_report)
MACHINE_OUTCOME_DEFINE_REPORT_FROM_WRAPPER(machine_outcome_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_outcome_build_from_machine_ir_report)

int machine_outcome_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error) {
    MachineOutcomeFile outcome_file;
    int ok;

    machine_outcome_file_init(&outcome_file);
    ok = machine_outcome_build_from_machine_ir_report_with_profile(report, profile, &outcome_file, error) &&
        machine_outcome_build_report_from_file(&outcome_file, out_report, error);
    machine_outcome_file_free(&outcome_file);
    return ok;
}

int machine_outcome_report_refresh(
    MachineOutcomeReport *report,
    MachineOutcomeError *error) {
    MachineOutcomeReport refreshed_report;
    int ok;

    if (!report) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-120: invalid report refresh contract");
        return 0;
    }
    machine_outcome_report_init(&refreshed_report);
    ok = machine_outcome_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_outcome_report_free(report);
        *report = refreshed_report;
    } else {
        machine_outcome_report_free(&refreshed_report);
    }
    return ok;
}

int machine_outcome_dump_file(
    const MachineOutcomeFile *outcome_file,
    char **out_text,
    MachineOutcomeError *error) {
    MachineOutcomeStringBuilder builder;
    MachineOutcomeHeaderSummary header_summary;
    MachineOutcomeSummary outcome_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&outcome_summary, 0, sizeof(outcome_summary));
    if (!outcome_file || !out_text ||
        !machine_outcome_verify_file(outcome_file, error) ||
        !machine_outcome_file_get_header_summary(outcome_file, &header_summary) ||
        !machine_outcome_file_get_outcome_summary(outcome_file, &outcome_summary)) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-121: invalid dump contract");
        return 0;
    }

    if (!machine_outcome_append_format(
            &builder,
            "machine_outcome profile=%s event=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_event_resolution_kind_name(header_summary.event_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_outcome_append_format(
            &builder,
            "outcome: resolution=%s kind=%s event=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_outcome_resolution_kind_name(outcome_summary.resolution_kind),
            machine_outcome_kind_name(outcome_summary.outcome_kind),
            machine_event_kind_name(outcome_summary.event_kind),
            machine_trace_resolution_kind_name(outcome_summary.trace_resolution_kind),
            machine_trace_change_class_name(outcome_summary.trace_change_class),
            machine_apply_resolution_kind_name(outcome_summary.apply_resolution_kind),
            machine_commit_resolution_kind_name(outcome_summary.commit_resolution_kind),
            machine_writeback_resolution_kind_name(outcome_summary.writeback_resolution_kind),
            machine_mutation_resolution_kind_name(outcome_summary.mutation_resolution_kind),
            machine_mutation_effect_kind_name(outcome_summary.mutation_effect_kind),
            machine_transition_resolution_kind_name(outcome_summary.transition_resolution_kind),
            machine_interp_action_kind_name(outcome_summary.action_kind),
            (unsigned int)outcome_summary.raw_byte,
            (unsigned int)outcome_summary.tag_value,
            outcome_summary.is_known ? "yes" : "no",
            outcome_summary.tag_name ? outcome_summary.tag_name : "-",
            outcome_summary.instruction_byte_count) ||
        !machine_outcome_append_payload_bytes(&builder, &outcome_summary) ||
        !machine_outcome_append_immediate_hint(&builder, &outcome_summary) ||
        !machine_outcome_append_format(
            &builder,
            " exact=%s origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            outcome_summary.is_exact_outcome ? "yes" : "no",
            machine_step_status_name(outcome_summary.origin_status),
            outcome_summary.has_observed_state ? machine_step_status_name(outcome_summary.observed_status) : "-",
            outcome_summary.has_status_change ? (outcome_summary.status_changed ? "yes" : "no") : "-",
            outcome_summary.has_program_counter_change ? (outcome_summary.program_counter_changed ? "yes" : "no") : "-",
            outcome_summary.has_stack_pointer_change ? (outcome_summary.stack_pointer_changed ? "yes" : "no") : "-",
            outcome_summary.has_fetch_change ? (outcome_summary.fetch_changed ? "yes" : "no") : "-") ||
        !machine_outcome_append_targets(&builder, &outcome_summary) ||
        !machine_outcome_append_return_immediate(&builder, &outcome_summary) ||
        !machine_outcome_append_format(&builder, "\n")) {
        free(builder.data);
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-122: out of memory building dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_outcome_build_dump_from_file(
    const MachineOutcomeFile *source,
    char **out_text,
    MachineOutcomeError *error) {
    return machine_outcome_dump_file(source, out_text, error);
}

#define MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineOutcomeError *error) { \
    MachineOutcomeFile outcome_file; \
    int ok; \
    machine_outcome_file_init(&outcome_file); \
    ok = builder(source, &outcome_file, error) && \
        machine_outcome_dump_file(&outcome_file, out_text, error); \
    machine_outcome_file_free(&outcome_file); \
    return ok; \
}

MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_event_file,
    MachineEventFile, machine_outcome_build_from_machine_event_file)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_event_report,
    MachineEventReport, machine_outcome_build_from_machine_event_report)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_trace_file,
    MachineTraceFile, machine_outcome_build_from_machine_trace_file)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_trace_report,
    MachineTraceReport, machine_outcome_build_from_machine_trace_report)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_delta_file,
    MachineDeltaFile, machine_outcome_build_from_machine_delta_file)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_delta_report,
    MachineDeltaReport, machine_outcome_build_from_machine_delta_report)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_observe_file,
    MachineObserveFile, machine_outcome_build_from_machine_observe_file)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_observe_report,
    MachineObserveReport, machine_outcome_build_from_machine_observe_report)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_apply_file,
    MachineApplyFile, machine_outcome_build_from_machine_apply_file)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_apply_report,
    MachineApplyReport, machine_outcome_build_from_machine_apply_report)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_commit_file,
    MachineCommitFile, machine_outcome_build_from_machine_commit_file)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_commit_report,
    MachineCommitReport, machine_outcome_build_from_machine_commit_report)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_step_file,
    MachineStepFile, machine_outcome_build_from_machine_step_file)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_step_report,
    MachineStepReport, machine_outcome_build_from_machine_step_report)
MACHINE_OUTCOME_DEFINE_DUMP_FROM_WRAPPER(machine_outcome_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_outcome_build_from_machine_ir_report)

int machine_outcome_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineOutcomeError *error) {
    MachineOutcomeFile outcome_file;
    int ok;

    machine_outcome_file_init(&outcome_file);
    ok = machine_outcome_build_from_machine_ir_report_with_profile(report, profile, &outcome_file, error) &&
        machine_outcome_dump_file(&outcome_file, out_text, error);
    machine_outcome_file_free(&outcome_file);
    return ok;
}

int machine_outcome_report_get_summary(
    const MachineOutcomeReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_outcome_report_get_overview_artifact(
    const MachineOutcomeReport *report,
    MachineOutcomeReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->event_report = &report->event_report;
    out_artifact->outcome_summary = &report->outcome_summary;
    return 1;
}

int machine_outcome_report_get_file(
    const MachineOutcomeReport *report,
    const MachineOutcomeFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_outcome_report_get_event_file(
    const MachineOutcomeReport *report,
    const MachineEventFile **out_event_file) {
    if (!report || !out_event_file) {
        return 0;
    }
    *out_event_file = &report->file.event_file;
    return 1;
}

int machine_outcome_report_get_event_report(
    const MachineOutcomeReport *report,
    const MachineEventReport **out_event_report) {
    if (!report || !out_event_report) {
        return 0;
    }
    *out_event_report = &report->event_report;
    return 1;
}

#define MACHINE_OUTCOME_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineOutcomeReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_OUTCOME_DEFINE_REPORT_ARTIFACT_GETTER(machine_outcome_report_get_header_summary_artifact,
    header_summary, MachineOutcomeHeaderSummary)
MACHINE_OUTCOME_DEFINE_REPORT_ARTIFACT_GETTER(machine_outcome_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineOutcomeTargetPolicySummary)
MACHINE_OUTCOME_DEFINE_REPORT_ARTIFACT_GETTER(machine_outcome_report_get_outcome_summary_artifact,
    outcome_summary, MachineOutcomeSummary)

int machine_outcome_report_overview_artifact_get_event_report(
    const MachineOutcomeReportOverviewArtifact *artifact,
    const MachineEventReport **out_event_report) {
    if (!artifact || !out_event_report) {
        return 0;
    }
    *out_event_report = artifact->event_report;
    return 1;
}

int machine_outcome_report_overview_artifact_get_outcome_summary_artifact(
    const MachineOutcomeReportOverviewArtifact *artifact,
    const MachineOutcomeSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->outcome_summary;
    return 1;
}

int machine_outcome_dump_report(
    const MachineOutcomeReport *report,
    char **out_text,
    MachineOutcomeError *error) {
    MachineOutcomeStringBuilder builder;
    const MachineOutcomeSummary *outcome_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_outcome_report_get_outcome_summary_artifact(report, &outcome_summary) ||
        !outcome_summary) {
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-123: invalid report dump contract");
        return 0;
    }

    if (!machine_outcome_append_format(
            &builder,
            "machine_outcome profile=%s event=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_event_resolution_kind_name(report->header_summary.event_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_outcome_append_format(
            &builder,
            "outcome: resolution=%s kind=%s event=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_outcome_resolution_kind_name(outcome_summary->resolution_kind),
            machine_outcome_kind_name(outcome_summary->outcome_kind),
            machine_event_kind_name(outcome_summary->event_kind),
            machine_trace_resolution_kind_name(outcome_summary->trace_resolution_kind),
            machine_trace_change_class_name(outcome_summary->trace_change_class),
            machine_apply_resolution_kind_name(outcome_summary->apply_resolution_kind),
            machine_commit_resolution_kind_name(outcome_summary->commit_resolution_kind),
            machine_writeback_resolution_kind_name(outcome_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(outcome_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(outcome_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(outcome_summary->transition_resolution_kind),
            machine_interp_action_kind_name(outcome_summary->action_kind),
            (unsigned int)outcome_summary->raw_byte,
            (unsigned int)outcome_summary->tag_value,
            outcome_summary->is_known ? "yes" : "no",
            outcome_summary->tag_name ? outcome_summary->tag_name : "-",
            outcome_summary->instruction_byte_count) ||
        !machine_outcome_append_payload_bytes(&builder, outcome_summary) ||
        !machine_outcome_append_immediate_hint(&builder, outcome_summary) ||
        !machine_outcome_append_format(
            &builder,
            " exact=%s origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            outcome_summary->is_exact_outcome ? "yes" : "no",
            machine_step_status_name(outcome_summary->origin_status),
            outcome_summary->has_observed_state ? machine_step_status_name(outcome_summary->observed_status) : "-",
            outcome_summary->has_status_change ? (outcome_summary->status_changed ? "yes" : "no") : "-",
            outcome_summary->has_program_counter_change ? (outcome_summary->program_counter_changed ? "yes" : "no") : "-",
            outcome_summary->has_stack_pointer_change ? (outcome_summary->stack_pointer_changed ? "yes" : "no") : "-",
            outcome_summary->has_fetch_change ? (outcome_summary->fetch_changed ? "yes" : "no") : "-") ||
        !machine_outcome_append_targets(&builder, outcome_summary) ||
        !machine_outcome_append_return_immediate(&builder, outcome_summary) ||
        !machine_outcome_append_format(&builder, "\nreport_overview:\n") ||
        !machine_outcome_append_format(
            &builder,
            "  origin: event=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_event_resolution_kind_name(report->header_summary.event_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer) ||
        !machine_outcome_append_format(
            &builder,
            "  policy: profile=%s exact=%s preview=%s family=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.surfaces_exact_outcome ? "yes" : "no",
            report->target_policy_summary.surfaces_preview_outcome ? "yes" : "no",
            report->target_policy_summary.classifies_outcome_family ? "yes" : "no") ||
        !machine_outcome_append_format(
            &builder,
            "  outcome: resolution=%s kind=%s event=%s exact=%s state=%s status=%s pc=%s fetch=%s",
            machine_outcome_resolution_kind_name(outcome_summary->resolution_kind),
            machine_outcome_kind_name(outcome_summary->outcome_kind),
            machine_event_kind_name(outcome_summary->event_kind),
            outcome_summary->is_exact_outcome ? "yes" : "no",
            outcome_summary->has_observed_state ? "yes" : "no",
            outcome_summary->has_status_change ? (outcome_summary->status_changed ? "yes" : "no") : "-",
            outcome_summary->has_program_counter_change ? (outcome_summary->program_counter_changed ? "yes" : "no") : "-",
            outcome_summary->has_fetch_change ? (outcome_summary->fetch_changed ? "yes" : "no") : "-")) {
        free(builder.data);
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-124: out of memory building report dump");
        return 0;
    }
    if (!machine_outcome_append_targets(&builder, outcome_summary) ||
        !machine_outcome_append_return_immediate(&builder, outcome_summary) ||
        !machine_outcome_append_format(&builder, "\n")) {
        free(builder.data);
        machine_outcome_set_error(error, 0, 0, "MACHINE-OUTCOME-125: out of memory terminating report dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_outcome_build_report_dump_from_file(
    const MachineOutcomeFile *source,
    char **out_text,
    MachineOutcomeError *error) {
    MachineOutcomeReport report;
    int ok;

    machine_outcome_report_init(&report);
    ok = machine_outcome_build_report_from_file(source, &report, error) &&
        machine_outcome_dump_report(&report, out_text, error);
    machine_outcome_report_free(&report);
    return ok;
}

#define MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineOutcomeError *error) { \
    MachineOutcomeReport report; \
    int ok; \
    machine_outcome_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_outcome_dump_report(&report, out_text, error); \
    machine_outcome_report_free(&report); \
    return ok; \
}

MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_event_file,
    MachineEventFile, machine_outcome_build_report_from_machine_event_file)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_event_report,
    MachineEventReport, machine_outcome_build_report_from_machine_event_report)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_trace_file,
    MachineTraceFile, machine_outcome_build_report_from_machine_trace_file)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_trace_report,
    MachineTraceReport, machine_outcome_build_report_from_machine_trace_report)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_delta_file,
    MachineDeltaFile, machine_outcome_build_report_from_machine_delta_file)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_delta_report,
    MachineDeltaReport, machine_outcome_build_report_from_machine_delta_report)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_observe_file,
    MachineObserveFile, machine_outcome_build_report_from_machine_observe_file)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_observe_report,
    MachineObserveReport, machine_outcome_build_report_from_machine_observe_report)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_apply_file,
    MachineApplyFile, machine_outcome_build_report_from_machine_apply_file)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_apply_report,
    MachineApplyReport, machine_outcome_build_report_from_machine_apply_report)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_commit_file,
    MachineCommitFile, machine_outcome_build_report_from_machine_commit_file)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_commit_report,
    MachineCommitReport, machine_outcome_build_report_from_machine_commit_report)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_outcome_build_report_from_machine_step_file)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_outcome_build_report_from_machine_step_report)
MACHINE_OUTCOME_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_outcome_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_outcome_build_report_from_machine_ir_report)

int machine_outcome_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineOutcomeError *error) {
    MachineOutcomeReport report_file;
    int ok;

    machine_outcome_report_init(&report_file);
    ok = machine_outcome_build_report_from_machine_ir_report_with_profile(
            report, profile, &report_file, error) &&
        machine_outcome_dump_report(&report_file, out_text, error);
    machine_outcome_report_free(&report_file);
    return ok;
}
