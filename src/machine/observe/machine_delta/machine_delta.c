#include "machine/delta.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineDeltaStringBuilder;

static void machine_delta_set_error(MachineDeltaError *error, int line, int column, const char *fmt, ...);
static int machine_delta_append_format(MachineDeltaStringBuilder *builder, const char *fmt, ...);
static int machine_delta_append_targets(MachineDeltaStringBuilder *builder,
    const MachineDeltaSummary *delta_summary);
static int machine_delta_append_return_immediate(MachineDeltaStringBuilder *builder,
    const MachineDeltaSummary *delta_summary);
static int machine_delta_append_payload_bytes(MachineDeltaStringBuilder *builder,
    const MachineDeltaSummary *delta_summary);
static int machine_delta_append_immediate_hint(MachineDeltaStringBuilder *builder,
    const MachineDeltaSummary *delta_summary);

static void machine_delta_set_error(MachineDeltaError *error, int line, int column, const char *fmt, ...) {
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

static int machine_delta_append_format(MachineDeltaStringBuilder *builder, const char *fmt, ...) {
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

static int machine_delta_append_targets(MachineDeltaStringBuilder *builder,
    const MachineDeltaSummary *delta_summary) {
    if (!builder || !delta_summary) {
        return 0;
    }
    if (!delta_summary->has_primary_target_block) {
        return machine_delta_append_format(builder, " targets=[]");
    }
    if (!delta_summary->has_secondary_target_block) {
        return machine_delta_append_format(builder, " targets=[%zu]", delta_summary->primary_target_block_index);
    }
    return machine_delta_append_format(
        builder,
        " targets=[%zu,%zu]",
        delta_summary->primary_target_block_index,
        delta_summary->secondary_target_block_index);
}

static int machine_delta_append_return_immediate(MachineDeltaStringBuilder *builder,
    const MachineDeltaSummary *delta_summary) {
    if (!builder || !delta_summary) {
        return 0;
    }
    if (!delta_summary->has_return_immediate) {
        return machine_delta_append_format(builder, " return-imm=-");
    }
    return machine_delta_append_format(builder, " return-imm=%zu", delta_summary->return_immediate);
}

static int machine_delta_append_payload_bytes(MachineDeltaStringBuilder *builder,
    const MachineDeltaSummary *delta_summary) {
    size_t index;

    if (!builder || !delta_summary) {
        return 0;
    }
    if (!machine_delta_append_format(builder, " payload=[")) {
        return 0;
    }
    for (index = 0u; index < delta_summary->payload_byte_count; ++index) {
        if (!machine_delta_append_format(
                builder,
                "%s0x%02x",
                index == 0u ? "" : ",",
                (unsigned int)delta_summary->payload_bytes[index])) {
            return 0;
        }
    }
    return machine_delta_append_format(builder, "]");
}

static int machine_delta_append_immediate_hint(MachineDeltaStringBuilder *builder,
    const MachineDeltaSummary *delta_summary) {
    if (!builder || !delta_summary) {
        return 0;
    }
    if (!delta_summary->has_immediate_hint) {
        return machine_delta_append_format(builder, " imm=-");
    }
    return machine_delta_append_format(builder, " imm=%zu", delta_summary->immediate_hint);
}

void machine_delta_file_init(MachineDeltaFile *delta_file) {
    if (!delta_file) {
        return;
    }
    machine_observe_file_init(&delta_file->observe_file);
    delta_file->resolution_kind = MACHINE_DELTA_RESOLUTION_NONE;
    delta_file->delta_kind = MACHINE_DELTA_KIND_NONE;
    delta_file->has_observed_state = 0;
    delta_file->observed_status = MACHINE_STEP_STATUS_READY;
    delta_file->observed_program_counter = 0u;
    delta_file->observed_stack_pointer = 0u;
    delta_file->has_observed_fetch = 0;
    delta_file->observed_segment_index = 0u;
    delta_file->observed_byte = 0u;
    delta_file->observed_byte_memory_offset = 0u;
}

void machine_delta_file_free(MachineDeltaFile *delta_file) {
    if (!delta_file) {
        return;
    }
    machine_observe_file_free(&delta_file->observe_file);
    machine_delta_file_init(delta_file);
}

void machine_delta_report_init(MachineDeltaReport *report) {
    if (!report) {
        return;
    }
    machine_delta_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_observe_report_init(&report->observe_report);
    memset(&report->delta_summary, 0, sizeof(report->delta_summary));
}

void machine_delta_report_free(MachineDeltaReport *report) {
    if (!report) {
        return;
    }
    machine_observe_report_free(&report->observe_report);
    machine_delta_file_free(&report->file);
    machine_delta_report_init(report);
}

const char *machine_delta_resolution_kind_name(MachineDeltaResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_DELTA_RESOLUTION_NONE:
            return "none";
        case MACHINE_DELTA_RESOLUTION_EXACT_STATE_DELTA:
            return "exact-state-delta";
        case MACHINE_DELTA_RESOLUTION_PREVIEW_STATE_DELTA:
            return "preview-state-delta";
        case MACHINE_DELTA_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_DELTA_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_delta_kind_name(MachineDeltaKind delta_kind) {
    switch (delta_kind) {
        case MACHINE_DELTA_KIND_NONE:
            return "none";
        case MACHINE_DELTA_KIND_STATE:
            return "state";
    }
    return "unknown";
}

int machine_delta_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineDeltaTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->surfaces_exact_delta = 1;
    out_summary->surfaces_preview_delta = 1;
    out_summary->surfaces_change_flags = 1;
    return 1;
}

int machine_delta_file_get_target_policy_summary(const MachineDeltaFile *delta_file,
    MachineDeltaTargetPolicySummary *out_summary) {
    if (!delta_file || !out_summary) {
        return 0;
    }
    return machine_delta_get_target_policy_summary(
        delta_file->observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_delta_file_get_summary(const MachineDeltaFile *delta_file,
    size_t *out_mapped_byte_count) {
    if (!delta_file) {
        return 0;
    }
    return machine_observe_file_get_summary(&delta_file->observe_file, out_mapped_byte_count);
}

int machine_delta_file_get_header_summary(const MachineDeltaFile *delta_file,
    MachineDeltaHeaderSummary *out_summary) {
    MachineObserveHeaderSummary observe_header_summary;

    memset(&observe_header_summary, 0, sizeof(observe_header_summary));
    if (!delta_file || !out_summary ||
        !machine_observe_file_get_header_summary(&delta_file->observe_file, &observe_header_summary)) {
        return 0;
    }
    out_summary->target_profile = observe_header_summary.target_profile;
    out_summary->observe_resolution_kind = delta_file->observe_file.resolution_kind;
    out_summary->origin_step_status = observe_header_summary.origin_step_status;
    out_summary->origin_program_counter = observe_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = observe_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = observe_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = observe_header_summary.mapped_byte_count;
    return 1;
}

int machine_delta_file_get_observe_summary(const MachineDeltaFile *delta_file,
    MachineObserveSummary *out_summary) {
    if (!delta_file || !out_summary) {
        return 0;
    }
    return machine_observe_file_get_observe_summary(&delta_file->observe_file, out_summary);
}

int machine_delta_file_get_delta_summary(const MachineDeltaFile *delta_file,
    MachineDeltaSummary *out_summary) {
    MachineObserveSummary observe_summary;
    MachineStepHeaderSummary step_header_summary;
    MachineStepFetchSummary fetch_summary;

    memset(&observe_summary, 0, sizeof(observe_summary));
    memset(&step_header_summary, 0, sizeof(step_header_summary));
    memset(&fetch_summary, 0, sizeof(fetch_summary));
    if (!delta_file || !out_summary ||
        !machine_delta_file_get_observe_summary(delta_file, &observe_summary) ||
        !machine_step_file_get_header_summary(
            &delta_file->observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file,
            &step_header_summary)) {
        return 0;
    }
    if (step_header_summary.status == MACHINE_STEP_STATUS_READY &&
        !machine_step_file_get_fetch_summary(
            &delta_file->observe_file.apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file,
            &fetch_summary)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->resolution_kind = delta_file->resolution_kind;
    out_summary->delta_kind = delta_file->delta_kind;
    out_summary->observe_resolution_kind = observe_summary.resolution_kind;
    out_summary->observe_kind = observe_summary.observe_kind;
    out_summary->apply_resolution_kind = observe_summary.apply_resolution_kind;
    out_summary->apply_kind = observe_summary.apply_kind;
    out_summary->commit_resolution_kind = observe_summary.commit_resolution_kind;
    out_summary->commit_kind = observe_summary.commit_kind;
    out_summary->writeback_resolution_kind = observe_summary.writeback_resolution_kind;
    out_summary->writeback_commit_kind = observe_summary.writeback_commit_kind;
    out_summary->mutation_resolution_kind = observe_summary.mutation_resolution_kind;
    out_summary->mutation_effect_kind = observe_summary.mutation_effect_kind;
    out_summary->state_resolution_kind = observe_summary.state_resolution_kind;
    out_summary->transition_resolution_kind = observe_summary.transition_resolution_kind;
    out_summary->action_kind = observe_summary.action_kind;
    out_summary->payload_kind = observe_summary.payload_kind;
    out_summary->tag_class = observe_summary.tag_class;
    out_summary->raw_byte = observe_summary.raw_byte;
    out_summary->tag_value = observe_summary.tag_value;
    out_summary->is_known = observe_summary.is_known;
    out_summary->tag_name = observe_summary.tag_name;
    out_summary->instruction_byte_count = observe_summary.instruction_byte_count;
    out_summary->payload_byte_count = observe_summary.payload_byte_count;
    memcpy(out_summary->payload_bytes, observe_summary.payload_bytes, sizeof(out_summary->payload_bytes));
    out_summary->has_immediate_hint = observe_summary.has_immediate_hint;
    out_summary->immediate_hint = observe_summary.immediate_hint;
    out_summary->is_exact_delta = delta_file->resolution_kind == MACHINE_DELTA_RESOLUTION_EXACT_STATE_DELTA;
    out_summary->origin_status = step_header_summary.status;
    out_summary->origin_program_counter = step_header_summary.program_counter;
    out_summary->origin_stack_pointer = step_header_summary.stack_pointer;
    out_summary->has_origin_fetch = step_header_summary.status == MACHINE_STEP_STATUS_READY;
    out_summary->origin_segment_index = fetch_summary.segment_index;
    out_summary->origin_byte = fetch_summary.byte_value;
    out_summary->origin_byte_memory_offset = fetch_summary.byte_memory_offset;
    out_summary->has_observed_state = delta_file->has_observed_state;
    out_summary->observed_status = delta_file->observed_status;
    out_summary->observed_program_counter = delta_file->observed_program_counter;
    out_summary->observed_stack_pointer = delta_file->observed_stack_pointer;
    out_summary->has_observed_fetch = delta_file->has_observed_fetch;
    out_summary->observed_segment_index = delta_file->observed_segment_index;
    out_summary->observed_byte = delta_file->observed_byte;
    out_summary->observed_byte_memory_offset = delta_file->observed_byte_memory_offset;
    out_summary->has_status_change = delta_file->has_observed_state ? 1 : 0;
    out_summary->status_changed = delta_file->has_observed_state ? (step_header_summary.status != delta_file->observed_status) : 0;
    out_summary->has_program_counter_change = delta_file->has_observed_state ? 1 : 0;
    out_summary->program_counter_changed = delta_file->has_observed_state
        ? (step_header_summary.program_counter != delta_file->observed_program_counter)
        : 0;
    out_summary->has_stack_pointer_change = delta_file->has_observed_state ? 1 : 0;
    out_summary->stack_pointer_changed = delta_file->has_observed_state
        ? (step_header_summary.stack_pointer != delta_file->observed_stack_pointer)
        : 0;
    out_summary->has_fetch_change = delta_file->has_observed_state ? 1 : 0;
    out_summary->fetch_changed = delta_file->has_observed_state
        ? ((out_summary->has_origin_fetch != delta_file->has_observed_fetch) ||
            (out_summary->has_origin_fetch &&
                (out_summary->origin_segment_index != delta_file->observed_segment_index ||
                    out_summary->origin_byte != delta_file->observed_byte ||
                    out_summary->origin_byte_memory_offset != delta_file->observed_byte_memory_offset)))
        : 0;
    out_summary->has_primary_target_block = observe_summary.has_primary_target_block;
    out_summary->primary_target_block_index = observe_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = observe_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = observe_summary.secondary_target_block_index;
    out_summary->has_return_immediate = observe_summary.has_return_immediate;
    out_summary->return_immediate = observe_summary.return_immediate;
    return 1;
}

int machine_delta_verify_file(const MachineDeltaFile *delta_file,
    MachineDeltaError *error) {
    MachineObserveSummary observe_summary;
    MachineDeltaSummary delta_summary;

    memset(&observe_summary, 0, sizeof(observe_summary));
    memset(&delta_summary, 0, sizeof(delta_summary));
    if (!delta_file) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-100: invalid delta file");
        return 0;
    }
    if (!machine_observe_verify_file(&delta_file->observe_file, NULL) ||
        !machine_delta_file_get_observe_summary(delta_file, &observe_summary) ||
        !machine_delta_file_get_delta_summary(delta_file, &delta_summary)) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-101: invalid observe input");
        return 0;
    }

    switch (delta_file->resolution_kind) {
        case MACHINE_DELTA_RESOLUTION_EXACT_STATE_DELTA:
            if (delta_file->observe_file.resolution_kind != MACHINE_OBSERVE_RESOLUTION_EXACT_STATE ||
                delta_file->delta_kind != MACHINE_DELTA_KIND_STATE ||
                !delta_file->has_observed_state ||
                !observe_summary.has_observed_state ||
                delta_file->observed_status != observe_summary.observed_status ||
                delta_file->observed_program_counter != observe_summary.program_counter ||
                delta_file->observed_stack_pointer != observe_summary.stack_pointer ||
                delta_file->has_observed_fetch != observe_summary.has_current_fetch ||
                (observe_summary.has_current_fetch &&
                    (delta_file->observed_segment_index != observe_summary.current_segment_index ||
                        delta_file->observed_byte != observe_summary.current_byte ||
                        delta_file->observed_byte_memory_offset != observe_summary.current_byte_memory_offset)) ||
                !delta_summary.is_exact_delta ||
                !delta_summary.has_status_change ||
                !delta_summary.has_program_counter_change ||
                !delta_summary.has_stack_pointer_change ||
                !delta_summary.has_fetch_change) {
                machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-102: invalid exact-state delta");
                return 0;
            }
            break;
        case MACHINE_DELTA_RESOLUTION_PREVIEW_STATE_DELTA:
            if (delta_file->observe_file.resolution_kind != MACHINE_OBSERVE_RESOLUTION_PREVIEW_STATE ||
                delta_file->delta_kind != MACHINE_DELTA_KIND_STATE ||
                !delta_file->has_observed_state ||
                !observe_summary.has_observed_state ||
                delta_file->observed_status != observe_summary.observed_status ||
                delta_file->observed_program_counter != observe_summary.program_counter ||
                delta_file->observed_stack_pointer != observe_summary.stack_pointer ||
                delta_file->has_observed_fetch != observe_summary.has_current_fetch ||
                (observe_summary.has_current_fetch &&
                    (delta_file->observed_segment_index != observe_summary.current_segment_index ||
                        delta_file->observed_byte != observe_summary.current_byte ||
                        delta_file->observed_byte_memory_offset != observe_summary.current_byte_memory_offset)) ||
                delta_summary.is_exact_delta ||
                !delta_summary.has_status_change ||
                !delta_summary.has_program_counter_change ||
                !delta_summary.has_stack_pointer_change ||
                !delta_summary.has_fetch_change) {
                machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-103: invalid preview-state delta");
                return 0;
            }
            break;
        case MACHINE_DELTA_RESOLUTION_BLOCKED_ON_CONTROL:
            if (delta_file->observe_file.resolution_kind != MACHINE_OBSERVE_RESOLUTION_BLOCKED_ON_CONTROL ||
                delta_file->delta_kind != MACHINE_DELTA_KIND_NONE ||
                delta_file->has_observed_state ||
                delta_summary.has_status_change ||
                delta_summary.has_program_counter_change ||
                delta_summary.has_stack_pointer_change ||
                delta_summary.has_fetch_change) {
                machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-104: invalid blocked-control delta");
                return 0;
            }
            break;
        case MACHINE_DELTA_RESOLUTION_BLOCKED_UNSUPPORTED:
            if (delta_file->observe_file.resolution_kind != MACHINE_OBSERVE_RESOLUTION_BLOCKED_UNSUPPORTED ||
                delta_file->delta_kind != MACHINE_DELTA_KIND_NONE ||
                delta_file->has_observed_state ||
                delta_summary.has_status_change ||
                delta_summary.has_program_counter_change ||
                delta_summary.has_stack_pointer_change ||
                delta_summary.has_fetch_change) {
                machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-105: invalid blocked-unsupported delta");
                return 0;
            }
            break;
        case MACHINE_DELTA_RESOLUTION_NONE:
        default:
            machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-106: invalid delta resolution kind");
            return 0;
    }
    return 1;
}

int machine_delta_clone_file(const MachineDeltaFile *source,
    MachineDeltaFile *out_clone,
    MachineDeltaError *error) {
    if (!source || !out_clone) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-107: invalid clone contract");
        return 0;
    }
    if (!machine_delta_verify_file(source, error)) {
        return 0;
    }

    machine_delta_file_free(out_clone);
    if (!machine_observe_clone_file(&source->observe_file, &out_clone->observe_file, NULL)) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-108: failed cloning observe input");
        machine_delta_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->delta_kind = source->delta_kind;
    out_clone->has_observed_state = source->has_observed_state;
    out_clone->observed_status = source->observed_status;
    out_clone->observed_program_counter = source->observed_program_counter;
    out_clone->observed_stack_pointer = source->observed_stack_pointer;
    out_clone->has_observed_fetch = source->has_observed_fetch;
    out_clone->observed_segment_index = source->observed_segment_index;
    out_clone->observed_byte = source->observed_byte;
    out_clone->observed_byte_memory_offset = source->observed_byte_memory_offset;
    return 1;
}

int machine_delta_build_from_machine_observe_file(const MachineObserveFile *observe_file,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error) {
    if (!observe_file || !out_delta_file) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-109: invalid build contract");
        return 0;
    }
    if (!machine_observe_verify_file(observe_file, NULL)) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-110: invalid observe input");
        return 0;
    }

    machine_delta_file_free(out_delta_file);
    if (!machine_observe_clone_file(observe_file, &out_delta_file->observe_file, NULL)) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-111: failed cloning observe input");
        machine_delta_file_free(out_delta_file);
        return 0;
    }

    switch (observe_file->resolution_kind) {
        case MACHINE_OBSERVE_RESOLUTION_EXACT_STATE:
            out_delta_file->resolution_kind = MACHINE_DELTA_RESOLUTION_EXACT_STATE_DELTA;
            out_delta_file->delta_kind = MACHINE_DELTA_KIND_STATE;
            out_delta_file->has_observed_state = 1;
            out_delta_file->observed_status = observe_file->observed_status;
            out_delta_file->observed_program_counter = observe_file->program_counter;
            out_delta_file->observed_stack_pointer = observe_file->stack_pointer;
            out_delta_file->has_observed_fetch = observe_file->has_current_fetch;
            out_delta_file->observed_segment_index = observe_file->current_segment_index;
            out_delta_file->observed_byte = observe_file->current_byte;
            out_delta_file->observed_byte_memory_offset = observe_file->current_byte_memory_offset;
            break;
        case MACHINE_OBSERVE_RESOLUTION_PREVIEW_STATE:
            out_delta_file->resolution_kind = MACHINE_DELTA_RESOLUTION_PREVIEW_STATE_DELTA;
            out_delta_file->delta_kind = MACHINE_DELTA_KIND_STATE;
            out_delta_file->has_observed_state = 1;
            out_delta_file->observed_status = observe_file->observed_status;
            out_delta_file->observed_program_counter = observe_file->program_counter;
            out_delta_file->observed_stack_pointer = observe_file->stack_pointer;
            out_delta_file->has_observed_fetch = observe_file->has_current_fetch;
            out_delta_file->observed_segment_index = observe_file->current_segment_index;
            out_delta_file->observed_byte = observe_file->current_byte;
            out_delta_file->observed_byte_memory_offset = observe_file->current_byte_memory_offset;
            break;
        case MACHINE_OBSERVE_RESOLUTION_BLOCKED_ON_CONTROL:
            out_delta_file->resolution_kind = MACHINE_DELTA_RESOLUTION_BLOCKED_ON_CONTROL;
            out_delta_file->delta_kind = MACHINE_DELTA_KIND_NONE;
            break;
        case MACHINE_OBSERVE_RESOLUTION_BLOCKED_UNSUPPORTED:
            out_delta_file->resolution_kind = MACHINE_DELTA_RESOLUTION_BLOCKED_UNSUPPORTED;
            out_delta_file->delta_kind = MACHINE_DELTA_KIND_NONE;
            break;
        case MACHINE_OBSERVE_RESOLUTION_NONE:
        default:
            machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-112: unsupported observe resolution");
            machine_delta_file_free(out_delta_file);
            return 0;
    }

    if (!machine_delta_verify_file(out_delta_file, error)) {
        machine_delta_file_free(out_delta_file);
        return 0;
    }
    return 1;
}

int machine_delta_build_from_machine_observe_report(const MachineObserveReport *report,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error) {
    const MachineObserveFile *observe_file = NULL;

    if (!report) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-113: invalid build-from-observe-report contract");
        return 0;
    }
    if (!machine_observe_report_get_file(report, &observe_file) || !observe_file) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-114: failed recovering observe file from report");
        return 0;
    }
    return machine_delta_build_from_machine_observe_file(observe_file, out_delta_file, error);
}

#define MACHINE_DELTA_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineDeltaFile *out_delta_file, MachineDeltaError *error) { \
    MachineObserveFile observe_file; \
    int ok; \
    machine_observe_file_init(&observe_file); \
    ok = builder(source, &observe_file, NULL) && \
        machine_delta_build_from_machine_observe_file(&observe_file, out_delta_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-115: failed building delta wrapper"); \
    } \
    machine_observe_file_free(&observe_file); \
    return ok; \
}

MACHINE_DELTA_DEFINE_BUILD_FROM_WRAPPER(machine_delta_build_from_machine_apply_file,
    MachineApplyFile, machine_observe_build_from_machine_apply_file)
MACHINE_DELTA_DEFINE_BUILD_FROM_WRAPPER(machine_delta_build_from_machine_apply_report,
    MachineApplyReport, machine_observe_build_from_machine_apply_report)
MACHINE_DELTA_DEFINE_BUILD_FROM_WRAPPER(machine_delta_build_from_machine_commit_file,
    MachineCommitFile, machine_observe_build_from_machine_commit_file)
MACHINE_DELTA_DEFINE_BUILD_FROM_WRAPPER(machine_delta_build_from_machine_commit_report,
    MachineCommitReport, machine_observe_build_from_machine_commit_report)
MACHINE_DELTA_DEFINE_BUILD_FROM_WRAPPER(machine_delta_build_from_machine_step_file,
    MachineStepFile, machine_observe_build_from_machine_step_file)
MACHINE_DELTA_DEFINE_BUILD_FROM_WRAPPER(machine_delta_build_from_machine_step_report,
    MachineStepReport, machine_observe_build_from_machine_step_report)
MACHINE_DELTA_DEFINE_BUILD_FROM_WRAPPER(machine_delta_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_observe_build_from_machine_ir_report)

int machine_delta_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error) {
    MachineObserveFile observe_file;
    int ok;

    machine_observe_file_init(&observe_file);
    ok = machine_observe_build_from_machine_ir_report_with_profile(report, profile, &observe_file, NULL) &&
        machine_delta_build_from_machine_observe_file(&observe_file, out_delta_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-116: failed building profiled delta wrapper");
    }
    machine_observe_file_free(&observe_file);
    return ok;
}

int machine_delta_build_report_from_file(const MachineDeltaFile *source,
    MachineDeltaReport *out_report,
    MachineDeltaError *error) {
    if (!source || !out_report) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-117: invalid report build contract");
        return 0;
    }
    if (!machine_delta_verify_file(source, error)) {
        return 0;
    }

    machine_delta_report_free(out_report);
    if (!machine_delta_clone_file(source, &out_report->file, error)) {
        machine_delta_report_free(out_report);
        return 0;
    }
    if (!machine_delta_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_delta_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_observe_build_report_from_file(&out_report->file.observe_file, &out_report->observe_report, NULL) ||
        !machine_delta_file_get_delta_summary(&out_report->file, &out_report->delta_summary)) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-118: failed summarizing delta report");
        machine_delta_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_DELTA_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineDeltaReport *out_report, MachineDeltaError *error) { \
    MachineDeltaFile delta_file; \
    int ok; \
    machine_delta_file_init(&delta_file); \
    ok = builder(source, &delta_file, error) && \
        machine_delta_build_report_from_file(&delta_file, out_report, error); \
    machine_delta_file_free(&delta_file); \
    return ok; \
}

MACHINE_DELTA_DEFINE_REPORT_FROM_WRAPPER(machine_delta_build_report_from_machine_observe_file,
    MachineObserveFile, machine_delta_build_from_machine_observe_file)
MACHINE_DELTA_DEFINE_REPORT_FROM_WRAPPER(machine_delta_build_report_from_machine_observe_report,
    MachineObserveReport, machine_delta_build_from_machine_observe_report)
MACHINE_DELTA_DEFINE_REPORT_FROM_WRAPPER(machine_delta_build_report_from_machine_apply_file,
    MachineApplyFile, machine_delta_build_from_machine_apply_file)
MACHINE_DELTA_DEFINE_REPORT_FROM_WRAPPER(machine_delta_build_report_from_machine_apply_report,
    MachineApplyReport, machine_delta_build_from_machine_apply_report)
MACHINE_DELTA_DEFINE_REPORT_FROM_WRAPPER(machine_delta_build_report_from_machine_commit_file,
    MachineCommitFile, machine_delta_build_from_machine_commit_file)
MACHINE_DELTA_DEFINE_REPORT_FROM_WRAPPER(machine_delta_build_report_from_machine_commit_report,
    MachineCommitReport, machine_delta_build_from_machine_commit_report)
MACHINE_DELTA_DEFINE_REPORT_FROM_WRAPPER(machine_delta_build_report_from_machine_step_file,
    MachineStepFile, machine_delta_build_from_machine_step_file)
MACHINE_DELTA_DEFINE_REPORT_FROM_WRAPPER(machine_delta_build_report_from_machine_step_report,
    MachineStepReport, machine_delta_build_from_machine_step_report)
MACHINE_DELTA_DEFINE_REPORT_FROM_WRAPPER(machine_delta_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_delta_build_from_machine_ir_report)

int machine_delta_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineDeltaReport *out_report,
    MachineDeltaError *error) {
    MachineDeltaFile delta_file;
    int ok;

    machine_delta_file_init(&delta_file);
    ok = machine_delta_build_from_machine_ir_report_with_profile(report, profile, &delta_file, error) &&
        machine_delta_build_report_from_file(&delta_file, out_report, error);
    machine_delta_file_free(&delta_file);
    return ok;
}

int machine_delta_report_refresh(MachineDeltaReport *report,
    MachineDeltaError *error) {
    MachineDeltaReport refreshed_report;
    int ok;

    if (!report) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-119: invalid report refresh contract");
        return 0;
    }
    machine_delta_report_init(&refreshed_report);
    ok = machine_delta_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_delta_report_free(report);
        *report = refreshed_report;
    } else {
        machine_delta_report_free(&refreshed_report);
    }
    return ok;
}

int machine_delta_dump_file(const MachineDeltaFile *delta_file,
    char **out_text,
    MachineDeltaError *error) {
    MachineDeltaStringBuilder builder;
    MachineDeltaHeaderSummary header_summary;
    MachineDeltaSummary delta_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&delta_summary, 0, sizeof(delta_summary));
    if (!delta_file || !out_text ||
        !machine_delta_verify_file(delta_file, error) ||
        !machine_delta_file_get_header_summary(delta_file, &header_summary) ||
        !machine_delta_file_get_delta_summary(delta_file, &delta_summary)) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-120: invalid dump contract");
        return 0;
    }

    if (!machine_delta_append_format(
            &builder,
            "machine_delta profile=%s observe=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_observe_resolution_kind_name(header_summary.observe_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_delta_append_format(
            &builder,
            "delta: resolution=%s kind=%s observe=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_delta_resolution_kind_name(delta_summary.resolution_kind),
            machine_delta_kind_name(delta_summary.delta_kind),
            machine_observe_resolution_kind_name(delta_summary.observe_resolution_kind),
            machine_apply_resolution_kind_name(delta_summary.apply_resolution_kind),
            machine_commit_resolution_kind_name(delta_summary.commit_resolution_kind),
            machine_writeback_resolution_kind_name(delta_summary.writeback_resolution_kind),
            machine_mutation_resolution_kind_name(delta_summary.mutation_resolution_kind),
            machine_mutation_effect_kind_name(delta_summary.mutation_effect_kind),
            machine_transition_resolution_kind_name(delta_summary.transition_resolution_kind),
            machine_interp_action_kind_name(delta_summary.action_kind),
            (unsigned int)delta_summary.raw_byte,
            (unsigned int)delta_summary.tag_value,
            delta_summary.is_known ? "yes" : "no",
            delta_summary.tag_name ? delta_summary.tag_name : "-",
            delta_summary.instruction_byte_count) ||
        !machine_delta_append_payload_bytes(&builder, &delta_summary) ||
        !machine_delta_append_immediate_hint(&builder, &delta_summary) ||
        !machine_delta_append_format(
            &builder,
            " exact=%s origin-status=%s origin-pc=0x%zx origin-byte=%s",
            delta_summary.is_exact_delta ? "yes" : "no",
            machine_step_status_name(delta_summary.origin_status),
            delta_summary.origin_program_counter,
            delta_summary.has_origin_fetch ? "" : "-")) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-121: out of memory building dump");
        return 0;
    }
    if (delta_summary.has_origin_fetch &&
        !machine_delta_append_format(&builder, "0x%02x", (unsigned int)delta_summary.origin_byte)) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-122: out of memory appending origin byte");
        return 0;
    }
    if (!machine_delta_append_format(
            &builder,
            " observed-status=%s observed-pc=%s",
            delta_summary.has_observed_state ? machine_step_status_name(delta_summary.observed_status) : "-",
            delta_summary.has_observed_state ? "" : "-")) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-123: out of memory appending observed status");
        return 0;
    }
    if (delta_summary.has_observed_state &&
        !machine_delta_append_format(&builder, "0x%zx", delta_summary.observed_program_counter)) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-124: out of memory appending observed pc");
        return 0;
    }
    if (!machine_delta_append_format(
            &builder,
            " observed-byte=%s",
            delta_summary.has_observed_fetch ? "" : "-")) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-125: out of memory appending observed byte header");
        return 0;
    }
    if (delta_summary.has_observed_fetch &&
        !machine_delta_append_format(&builder, "0x%02x", (unsigned int)delta_summary.observed_byte)) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-126: out of memory appending observed byte");
        return 0;
    }
    if (!machine_delta_append_format(
            &builder,
            " status-changed=%s pc-changed=%s fetch-changed=%s",
            delta_summary.has_status_change ? (delta_summary.status_changed ? "yes" : "no") : "-",
            delta_summary.has_program_counter_change ? (delta_summary.program_counter_changed ? "yes" : "no") : "-",
            delta_summary.has_fetch_change ? (delta_summary.fetch_changed ? "yes" : "no") : "-")) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-127: out of memory appending change flags");
        return 0;
    }
    if (!machine_delta_append_targets(&builder, &delta_summary) ||
        !machine_delta_append_return_immediate(&builder, &delta_summary) ||
        !machine_delta_append_format(&builder, "\n")) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-128: out of memory terminating dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_delta_build_dump_from_file(const MachineDeltaFile *source,
    char **out_text,
    MachineDeltaError *error) {
    return machine_delta_dump_file(source, out_text, error);
}

#define MACHINE_DELTA_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineDeltaError *error) { \
    MachineDeltaFile delta_file; \
    int ok; \
    machine_delta_file_init(&delta_file); \
    ok = builder(source, &delta_file, error) && \
        machine_delta_dump_file(&delta_file, out_text, error); \
    machine_delta_file_free(&delta_file); \
    return ok; \
}

MACHINE_DELTA_DEFINE_DUMP_FROM_WRAPPER(machine_delta_build_dump_from_machine_observe_file,
    MachineObserveFile, machine_delta_build_from_machine_observe_file)
MACHINE_DELTA_DEFINE_DUMP_FROM_WRAPPER(machine_delta_build_dump_from_machine_observe_report,
    MachineObserveReport, machine_delta_build_from_machine_observe_report)
MACHINE_DELTA_DEFINE_DUMP_FROM_WRAPPER(machine_delta_build_dump_from_machine_apply_file,
    MachineApplyFile, machine_delta_build_from_machine_apply_file)
MACHINE_DELTA_DEFINE_DUMP_FROM_WRAPPER(machine_delta_build_dump_from_machine_apply_report,
    MachineApplyReport, machine_delta_build_from_machine_apply_report)
MACHINE_DELTA_DEFINE_DUMP_FROM_WRAPPER(machine_delta_build_dump_from_machine_commit_file,
    MachineCommitFile, machine_delta_build_from_machine_commit_file)
MACHINE_DELTA_DEFINE_DUMP_FROM_WRAPPER(machine_delta_build_dump_from_machine_commit_report,
    MachineCommitReport, machine_delta_build_from_machine_commit_report)
MACHINE_DELTA_DEFINE_DUMP_FROM_WRAPPER(machine_delta_build_dump_from_machine_step_file,
    MachineStepFile, machine_delta_build_from_machine_step_file)
MACHINE_DELTA_DEFINE_DUMP_FROM_WRAPPER(machine_delta_build_dump_from_machine_step_report,
    MachineStepReport, machine_delta_build_from_machine_step_report)
MACHINE_DELTA_DEFINE_DUMP_FROM_WRAPPER(machine_delta_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_delta_build_from_machine_ir_report)

int machine_delta_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineDeltaError *error) {
    MachineDeltaFile delta_file;
    int ok;

    machine_delta_file_init(&delta_file);
    ok = machine_delta_build_from_machine_ir_report_with_profile(report, profile, &delta_file, error) &&
        machine_delta_dump_file(&delta_file, out_text, error);
    machine_delta_file_free(&delta_file);
    return ok;
}

int machine_delta_report_get_summary(const MachineDeltaReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_delta_report_get_overview_artifact(const MachineDeltaReport *report,
    MachineDeltaReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->observe_report = &report->observe_report;
    out_artifact->delta_summary = &report->delta_summary;
    return 1;
}

int machine_delta_report_get_file(const MachineDeltaReport *report,
    const MachineDeltaFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_delta_report_get_observe_file(const MachineDeltaReport *report,
    const MachineObserveFile **out_observe_file) {
    if (!report || !out_observe_file) {
        return 0;
    }
    *out_observe_file = &report->file.observe_file;
    return 1;
}

int machine_delta_report_get_observe_report(const MachineDeltaReport *report,
    const MachineObserveReport **out_observe_report) {
    if (!report || !out_observe_report) {
        return 0;
    }
    *out_observe_report = &report->observe_report;
    return 1;
}

#define MACHINE_DELTA_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineDeltaReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_DELTA_DEFINE_REPORT_ARTIFACT_GETTER(machine_delta_report_get_header_summary_artifact,
    header_summary, MachineDeltaHeaderSummary)
MACHINE_DELTA_DEFINE_REPORT_ARTIFACT_GETTER(machine_delta_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineDeltaTargetPolicySummary)
MACHINE_DELTA_DEFINE_REPORT_ARTIFACT_GETTER(machine_delta_report_get_delta_summary_artifact,
    delta_summary, MachineDeltaSummary)

int machine_delta_report_overview_artifact_get_observe_report(
    const MachineDeltaReportOverviewArtifact *artifact,
    const MachineObserveReport **out_observe_report) {
    if (!artifact || !out_observe_report) {
        return 0;
    }
    *out_observe_report = artifact->observe_report;
    return 1;
}

int machine_delta_report_overview_artifact_get_delta_summary_artifact(
    const MachineDeltaReportOverviewArtifact *artifact,
    const MachineDeltaSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->delta_summary;
    return 1;
}

int machine_delta_dump_report(const MachineDeltaReport *report,
    char **out_text,
    MachineDeltaError *error) {
    MachineDeltaStringBuilder builder;
    const MachineDeltaSummary *delta_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_delta_report_get_delta_summary_artifact(report, &delta_summary) ||
        !delta_summary) {
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-126: invalid report dump contract");
        return 0;
    }

    if (!machine_delta_append_format(
            &builder,
            "machine_delta profile=%s observe=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_observe_resolution_kind_name(report->header_summary.observe_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_delta_append_format(
            &builder,
            "delta: resolution=%s kind=%s observe=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_delta_resolution_kind_name(delta_summary->resolution_kind),
            machine_delta_kind_name(delta_summary->delta_kind),
            machine_observe_resolution_kind_name(delta_summary->observe_resolution_kind),
            machine_apply_resolution_kind_name(delta_summary->apply_resolution_kind),
            machine_commit_resolution_kind_name(delta_summary->commit_resolution_kind),
            machine_writeback_resolution_kind_name(delta_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(delta_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(delta_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(delta_summary->transition_resolution_kind),
            machine_interp_action_kind_name(delta_summary->action_kind),
            (unsigned int)delta_summary->raw_byte,
            (unsigned int)delta_summary->tag_value,
            delta_summary->is_known ? "yes" : "no",
            delta_summary->tag_name ? delta_summary->tag_name : "-",
            delta_summary->instruction_byte_count) ||
        !machine_delta_append_payload_bytes(&builder, delta_summary) ||
        !machine_delta_append_immediate_hint(&builder, delta_summary) ||
        !machine_delta_append_format(
            &builder,
            " exact=%s origin-status=%s origin-pc=0x%zx origin-byte=%s",
            delta_summary->is_exact_delta ? "yes" : "no",
            machine_step_status_name(delta_summary->origin_status),
            delta_summary->origin_program_counter,
            delta_summary->has_origin_fetch ? "" : "-")) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-127: out of memory building report dump");
        return 0;
    }
    if (delta_summary->has_origin_fetch &&
        !machine_delta_append_format(&builder, "0x%02x", (unsigned int)delta_summary->origin_byte)) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-128: out of memory appending report origin byte");
        return 0;
    }
    if (!machine_delta_append_format(
            &builder,
            " observed-status=%s observed-pc=%s",
            delta_summary->has_observed_state ? machine_step_status_name(delta_summary->observed_status) : "-",
            delta_summary->has_observed_state ? "" : "-")) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-129: out of memory appending report observed status");
        return 0;
    }
    if (delta_summary->has_observed_state &&
        !machine_delta_append_format(&builder, "0x%zx", delta_summary->observed_program_counter)) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-130: out of memory appending report observed pc");
        return 0;
    }
    if (!machine_delta_append_format(
            &builder,
            " observed-byte=%s",
            delta_summary->has_observed_fetch ? "" : "-")) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-131: out of memory appending report observed byte header");
        return 0;
    }
    if (delta_summary->has_observed_fetch &&
        !machine_delta_append_format(&builder, "0x%02x", (unsigned int)delta_summary->observed_byte)) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-132: out of memory appending report observed byte");
        return 0;
    }
    if (!machine_delta_append_format(
            &builder,
            " status-changed=%s pc-changed=%s fetch-changed=%s",
            delta_summary->has_status_change ? (delta_summary->status_changed ? "yes" : "no") : "-",
            delta_summary->has_program_counter_change ? (delta_summary->program_counter_changed ? "yes" : "no") : "-",
            delta_summary->has_fetch_change ? (delta_summary->fetch_changed ? "yes" : "no") : "-")) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-133: out of memory appending report change flags");
        return 0;
    }
    if (!machine_delta_append_targets(&builder, delta_summary) ||
        !machine_delta_append_return_immediate(&builder, delta_summary) ||
        !machine_delta_append_format(&builder, "\nreport_overview:\n") ||
        !machine_delta_append_format(
            &builder,
            "  origin: observe=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_observe_resolution_kind_name(report->header_summary.observe_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer) ||
        !machine_delta_append_format(
            &builder,
            "  policy: profile=%s exact=%s preview=%s changes=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.surfaces_exact_delta ? "yes" : "no",
            report->target_policy_summary.surfaces_preview_delta ? "yes" : "no",
            report->target_policy_summary.surfaces_change_flags ? "yes" : "no") ||
        !machine_delta_append_format(
            &builder,
            "  delta: resolution=%s kind=%s exact=%s state=%s status=%s pc=%s fetch=%s",
            machine_delta_resolution_kind_name(delta_summary->resolution_kind),
            machine_delta_kind_name(delta_summary->delta_kind),
            delta_summary->is_exact_delta ? "yes" : "no",
            delta_summary->has_observed_state ? "yes" : "no",
            delta_summary->has_status_change ? (delta_summary->status_changed ? "yes" : "no") : "-",
            delta_summary->has_program_counter_change ? (delta_summary->program_counter_changed ? "yes" : "no") : "-",
            delta_summary->has_fetch_change ? (delta_summary->fetch_changed ? "yes" : "no") : "-")) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-134: out of memory building report overview");
        return 0;
    }
    if (!machine_delta_append_targets(&builder, delta_summary) ||
        !machine_delta_append_return_immediate(&builder, delta_summary) ||
        !machine_delta_append_format(&builder, "\n")) {
        free(builder.data);
        machine_delta_set_error(error, 0, 0, "MACHINE-DELTA-135: out of memory terminating report overview");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_delta_build_report_dump_from_file(const MachineDeltaFile *source,
    char **out_text,
    MachineDeltaError *error) {
    MachineDeltaReport report;
    int ok;

    machine_delta_report_init(&report);
    ok = machine_delta_build_report_from_file(source, &report, error) &&
        machine_delta_dump_report(&report, out_text, error);
    machine_delta_report_free(&report);
    return ok;
}

#define MACHINE_DELTA_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineDeltaError *error) { \
    MachineDeltaReport report; \
    int ok; \
    machine_delta_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_delta_dump_report(&report, out_text, error); \
    machine_delta_report_free(&report); \
    return ok; \
}

MACHINE_DELTA_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_delta_build_report_dump_from_machine_observe_file,
    MachineObserveFile, machine_delta_build_report_from_machine_observe_file)
MACHINE_DELTA_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_delta_build_report_dump_from_machine_observe_report,
    MachineObserveReport, machine_delta_build_report_from_machine_observe_report)
MACHINE_DELTA_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_delta_build_report_dump_from_machine_apply_file,
    MachineApplyFile, machine_delta_build_report_from_machine_apply_file)
MACHINE_DELTA_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_delta_build_report_dump_from_machine_apply_report,
    MachineApplyReport, machine_delta_build_report_from_machine_apply_report)
MACHINE_DELTA_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_delta_build_report_dump_from_machine_commit_file,
    MachineCommitFile, machine_delta_build_report_from_machine_commit_file)
MACHINE_DELTA_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_delta_build_report_dump_from_machine_commit_report,
    MachineCommitReport, machine_delta_build_report_from_machine_commit_report)
MACHINE_DELTA_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_delta_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_delta_build_report_from_machine_step_file)
MACHINE_DELTA_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_delta_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_delta_build_report_from_machine_step_report)
MACHINE_DELTA_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_delta_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_delta_build_report_from_machine_ir_report)

int machine_delta_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineDeltaError *error) {
    MachineDeltaReport report_file;
    int ok;

    machine_delta_report_init(&report_file);
    ok = machine_delta_build_report_from_machine_ir_report_with_profile(report, profile, &report_file, error) &&
        machine_delta_dump_report(&report_file, out_text, error);
    machine_delta_report_free(&report_file);
    return ok;
}
