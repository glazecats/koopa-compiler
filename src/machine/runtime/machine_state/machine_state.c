#include "machine/state.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineStateStringBuilder;

static void machine_state_set_error(MachineStateError *error, int line, int column, const char *fmt, ...);
static int machine_state_append_format(MachineStateStringBuilder *builder, const char *fmt, ...);
static int machine_state_append_targets(MachineStateStringBuilder *builder,
    const MachineStateSummary *state_summary);
static int machine_state_append_return_immediate(MachineStateStringBuilder *builder,
    const MachineStateSummary *state_summary);

static void machine_state_set_error(MachineStateError *error, int line, int column, const char *fmt, ...) {
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

static int machine_state_append_format(MachineStateStringBuilder *builder, const char *fmt, ...) {
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

static int machine_state_append_targets(MachineStateStringBuilder *builder,
    const MachineStateSummary *state_summary) {
    if (!builder || !state_summary) {
        return 0;
    }
    if (!state_summary->has_primary_target_block) {
        return machine_state_append_format(builder, " targets=[]");
    }
    if (!state_summary->has_secondary_target_block) {
        return machine_state_append_format(
            builder,
            " targets=[%zu]",
            state_summary->primary_target_block_index);
    }
    return machine_state_append_format(
        builder,
        " targets=[%zu,%zu]",
        state_summary->primary_target_block_index,
        state_summary->secondary_target_block_index);
}

static int machine_state_append_return_immediate(MachineStateStringBuilder *builder,
    const MachineStateSummary *state_summary) {
    if (!builder || !state_summary) {
        return 0;
    }
    if (!state_summary->has_return_immediate) {
        return machine_state_append_format(builder, " return-imm=-");
    }
    return machine_state_append_format(
        builder,
        " return-imm=%zu",
        state_summary->return_immediate);
}

void machine_state_file_init(MachineStateFile *state_file) {
    if (!state_file) {
        return;
    }
    machine_transition_file_init(&state_file->transition_file);
    state_file->resolution_kind = MACHINE_STATE_RESOLUTION_NONE;
    state_file->has_state = 0;
    state_file->state_status = MACHINE_STEP_STATUS_READY;
    state_file->program_counter = 0u;
    state_file->stack_pointer = 0u;
    state_file->has_current_fetch = 0;
    state_file->current_segment_index = 0u;
    state_file->current_byte = 0u;
    state_file->current_byte_memory_offset = 0u;
}

void machine_state_file_free(MachineStateFile *state_file) {
    if (!state_file) {
        return;
    }
    machine_transition_file_free(&state_file->transition_file);
    machine_state_file_init(state_file);
}

void machine_state_report_init(MachineStateReport *report) {
    if (!report) {
        return;
    }
    machine_state_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_transition_report_init(&report->transition_report);
    memset(&report->state_summary, 0, sizeof(report->state_summary));
    memset(&report->current_fetch_summary, 0, sizeof(report->current_fetch_summary));
}

void machine_state_report_free(MachineStateReport *report) {
    if (!report) {
        return;
    }
    machine_transition_report_free(&report->transition_report);
    machine_state_file_free(&report->file);
    machine_state_report_init(report);
}

const char *machine_state_resolution_kind_name(MachineStateResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_STATE_RESOLUTION_NONE:
            return "none";
        case MACHINE_STATE_RESOLUTION_READY:
            return "ready";
        case MACHINE_STATE_RESOLUTION_HALTED:
            return "halted";
        case MACHINE_STATE_RESOLUTION_DEFERRED_CONTROL_TRANSFER:
            return "deferred-control-transfer";
        case MACHINE_STATE_RESOLUTION_UNSUPPORTED:
            return "unsupported";
    }
    return "unknown";
}

int machine_state_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineStateTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->resolves_ready_state = 1;
    out_summary->resolves_halt_state = 1;
    out_summary->defers_control_transfer_state = 1;
    return 1;
}

int machine_state_file_get_target_policy_summary(const MachineStateFile *state_file,
    MachineStateTargetPolicySummary *out_summary) {
    if (!state_file || !out_summary) {
        return 0;
    }
    return machine_state_get_target_policy_summary(
        state_file->transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_state_file_get_summary(const MachineStateFile *state_file,
    size_t *out_mapped_byte_count) {
    if (!state_file) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count =
            state_file->transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.total_mapped_byte_count;
    }
    return 1;
}

int machine_state_file_get_header_summary(const MachineStateFile *state_file,
    MachineStateHeaderSummary *out_summary) {
    MachineTransitionHeaderSummary transition_header_summary;

    memset(&transition_header_summary, 0, sizeof(transition_header_summary));
    if (!state_file || !out_summary ||
        !machine_transition_file_get_header_summary(&state_file->transition_file, &transition_header_summary)) {
        return 0;
    }
    out_summary->target_profile = transition_header_summary.target_profile;
    out_summary->transition_resolution_kind = state_file->transition_file.resolution_kind;
    out_summary->origin_step_status = transition_header_summary.step_status;
    out_summary->origin_program_counter = transition_header_summary.program_counter;
    out_summary->origin_stack_pointer = transition_header_summary.stack_pointer;
    out_summary->origin_current_segment_index = transition_header_summary.current_segment_index;
    out_summary->mapped_byte_count = transition_header_summary.mapped_byte_count;
    return 1;
}

int machine_state_file_get_runtime_launch_summary(const MachineStateFile *state_file,
    MachineRuntimeLaunchSummary *out_summary) {
    if (!state_file || !out_summary) {
        return 0;
    }
    return machine_transition_file_get_runtime_launch_summary(&state_file->transition_file, out_summary);
}

int machine_state_file_get_initial_stack_summary(const MachineStateFile *state_file,
    MachineRuntimeInitialStackSummary *out_summary) {
    if (!state_file || !out_summary) {
        return 0;
    }
    return machine_transition_file_get_initial_stack_summary(&state_file->transition_file, out_summary);
}

int machine_state_file_get_runtime_memory_summary(const MachineStateFile *state_file,
    MachineRuntimeMemorySummary *out_summary) {
    if (!state_file || !out_summary) {
        return 0;
    }
    return machine_transition_file_get_runtime_memory_summary(&state_file->transition_file, out_summary);
}

int machine_state_file_get_decode_tag_summary(const MachineStateFile *state_file,
    MachineDecodeTagSummary *out_summary) {
    if (!state_file || !out_summary) {
        return 0;
    }
    return machine_transition_file_get_decode_tag_summary(&state_file->transition_file, out_summary);
}

int machine_state_file_get_payload_summary(const MachineStateFile *state_file,
    MachinePayloadDecodeSummary *out_summary) {
    if (!state_file || !out_summary) {
        return 0;
    }
    return machine_transition_file_get_payload_summary(&state_file->transition_file, out_summary);
}

int machine_state_file_get_transition_summary(const MachineStateFile *state_file,
    MachineTransitionSummary *out_summary) {
    if (!state_file || !out_summary) {
        return 0;
    }
    return machine_transition_file_get_transition_summary(&state_file->transition_file, out_summary);
}

int machine_state_file_get_state_summary(const MachineStateFile *state_file,
    MachineStateSummary *out_summary) {
    MachineTransitionSummary transition_summary;

    memset(&transition_summary, 0, sizeof(transition_summary));
    if (!state_file || !out_summary ||
        !machine_state_file_get_transition_summary(state_file, &transition_summary)) {
        return 0;
    }
    out_summary->resolution_kind = state_file->resolution_kind;
    out_summary->transition_resolution_kind = transition_summary.resolution_kind;
    out_summary->action_kind = transition_summary.action_kind;
    out_summary->raw_byte = transition_summary.raw_byte;
    out_summary->tag_value = transition_summary.tag_value;
    out_summary->is_known = transition_summary.is_known;
    out_summary->tag_name = transition_summary.tag_name;
    out_summary->instruction_byte_count = transition_summary.instruction_byte_count;
    out_summary->has_state = state_file->has_state;
    out_summary->state_status = state_file->state_status;
    out_summary->program_counter = state_file->program_counter;
    out_summary->stack_pointer = state_file->stack_pointer;
    out_summary->has_current_fetch = state_file->has_current_fetch;
    out_summary->current_segment_index = state_file->current_segment_index;
    out_summary->current_byte = state_file->current_byte;
    out_summary->current_byte_memory_offset = state_file->current_byte_memory_offset;
    out_summary->has_primary_target_block = transition_summary.has_primary_target_block;
    out_summary->primary_target_block_index = transition_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = transition_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = transition_summary.secondary_target_block_index;
    out_summary->has_return_immediate = transition_summary.has_return_immediate;
    out_summary->return_immediate = transition_summary.return_immediate;
    return 1;
}

int machine_state_file_get_current_fetch_summary(const MachineStateFile *state_file,
    MachineStateCurrentFetchSummary *out_summary) {
    MachineRuntimeSegmentSummary segment_summary;
    const MachineRuntimeSegment *segment = NULL;

    memset(&segment_summary, 0, sizeof(segment_summary));
    if (!state_file || !state_file->has_current_fetch) {
        return 0;
    }
    if (!machine_runtime_file_get_segment(
            &state_file->transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file,
            state_file->current_segment_index,
            &segment) ||
        !segment ||
        !machine_runtime_segment_get_summary(segment, &segment_summary)) {
        return 0;
    }
    if (!out_summary) {
        return 1;
    }
    out_summary->byte_virtual_address = state_file->program_counter;
    out_summary->byte_memory_offset = state_file->current_byte_memory_offset;
    out_summary->segment_index = state_file->current_segment_index;
    out_summary->segment_name = segment_summary.name;
    out_summary->byte_value = state_file->current_byte;
    return 1;
}

int machine_state_verify_file(const MachineStateFile *state_file,
    MachineStateError *error) {
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineTransitionSummary transition_summary;
    const MachineRuntimeSegment *segment = NULL;
    unsigned char current_byte = 0u;
    const MachineStepFile *step_file = NULL;

    memset(&runtime_memory_summary, 0, sizeof(runtime_memory_summary));
    memset(&transition_summary, 0, sizeof(transition_summary));
    if (!state_file) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-100: invalid state file");
        return 0;
    }
    step_file = &state_file->transition_file.interp_file.payload_decode_file.decode_file.step_file;
    if (!machine_transition_verify_file(&state_file->transition_file, NULL) ||
        !machine_state_file_get_runtime_memory_summary(state_file, &runtime_memory_summary) ||
        !machine_state_file_get_transition_summary(state_file, &transition_summary)) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-101: invalid transition input");
        return 0;
    }

    switch (state_file->resolution_kind) {
        case MACHINE_STATE_RESOLUTION_READY:
            if (state_file->transition_file.resolution_kind != MACHINE_TRANSITION_RESOLUTION_NEXT_FETCH ||
                !state_file->has_state ||
                state_file->state_status != MACHINE_STEP_STATUS_READY ||
                state_file->program_counter != state_file->transition_file.next_program_counter ||
                state_file->stack_pointer != step_file->stack_pointer ||
                !state_file->has_current_fetch ||
                state_file->current_segment_index != state_file->transition_file.next_current_segment_index ||
                state_file->current_byte != state_file->transition_file.next_current_byte ||
                state_file->current_byte_memory_offset !=
                    state_file->transition_file.next_current_byte_memory_offset ||
                !machine_runtime_file_get_segment(
                    &state_file->transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file,
                    state_file->current_segment_index,
                    &segment) ||
                !segment || !segment->executable ||
                state_file->program_counter < runtime_memory_summary.base_virtual_address ||
                state_file->current_byte_memory_offset !=
                    state_file->program_counter - runtime_memory_summary.base_virtual_address ||
                !machine_runtime_file_get_memory_byte_at_virtual_address(
                    &state_file->transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file,
                    state_file->program_counter,
                    &current_byte) ||
                current_byte != state_file->current_byte ||
                !machine_state_file_get_current_fetch_summary(state_file, NULL)) {
                machine_state_set_error(error, 0, 0, "MACHINE-STATE-102: invalid ready state");
                return 0;
            }
            break;
        case MACHINE_STATE_RESOLUTION_HALTED:
            if (state_file->transition_file.resolution_kind != MACHINE_TRANSITION_RESOLUTION_HALT ||
                !state_file->has_state ||
                state_file->state_status != MACHINE_STEP_STATUS_HALTED ||
                state_file->program_counter != step_file->program_counter ||
                state_file->stack_pointer != step_file->stack_pointer ||
                state_file->has_current_fetch) {
                machine_state_set_error(error, 0, 0, "MACHINE-STATE-103: invalid halted state");
                return 0;
            }
            break;
        case MACHINE_STATE_RESOLUTION_DEFERRED_CONTROL_TRANSFER:
            if (state_file->transition_file.resolution_kind !=
                    MACHINE_TRANSITION_RESOLUTION_DEFERRED_CONTROL_TRANSFER ||
                state_file->has_state ||
                state_file->has_current_fetch ||
                state_file->state_status != transition_summary.next_status) {
                machine_state_set_error(error, 0, 0, "MACHINE-STATE-104: invalid deferred control state");
                return 0;
            }
            break;
        case MACHINE_STATE_RESOLUTION_UNSUPPORTED:
            if (state_file->transition_file.resolution_kind != MACHINE_TRANSITION_RESOLUTION_UNSUPPORTED ||
                state_file->has_state ||
                state_file->has_current_fetch ||
                state_file->state_status != transition_summary.next_status) {
                machine_state_set_error(error, 0, 0, "MACHINE-STATE-105: invalid unsupported state");
                return 0;
            }
            break;
        case MACHINE_STATE_RESOLUTION_NONE:
        default:
            machine_state_set_error(error, 0, 0, "MACHINE-STATE-106: invalid state resolution kind");
            return 0;
    }
    return 1;
}

int machine_state_clone_file(const MachineStateFile *source,
    MachineStateFile *out_clone,
    MachineStateError *error) {
    if (!source || !out_clone) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-107: invalid clone contract");
        return 0;
    }
    if (!machine_state_verify_file(source, error)) {
        return 0;
    }

    machine_state_file_free(out_clone);
    if (!machine_transition_clone_file(&source->transition_file, &out_clone->transition_file, NULL)) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-108: failed cloning transition input");
        machine_state_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->has_state = source->has_state;
    out_clone->state_status = source->state_status;
    out_clone->program_counter = source->program_counter;
    out_clone->stack_pointer = source->stack_pointer;
    out_clone->has_current_fetch = source->has_current_fetch;
    out_clone->current_segment_index = source->current_segment_index;
    out_clone->current_byte = source->current_byte;
    out_clone->current_byte_memory_offset = source->current_byte_memory_offset;
    return 1;
}

int machine_state_build_from_machine_transition_file(const MachineTransitionFile *transition_file,
    MachineStateFile *out_state_file,
    MachineStateError *error) {
    MachineTransitionSummary transition_summary;
    const MachineStepFile *step_file = NULL;

    memset(&transition_summary, 0, sizeof(transition_summary));
    if (!transition_file || !out_state_file) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-109: invalid build contract");
        return 0;
    }
    if (!machine_transition_verify_file(transition_file, NULL) ||
        !machine_transition_file_get_transition_summary(transition_file, &transition_summary)) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-110: invalid transition input");
        return 0;
    }

    machine_state_file_free(out_state_file);
    if (!machine_transition_clone_file(transition_file, &out_state_file->transition_file, NULL)) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-111: failed cloning transition input");
        machine_state_file_free(out_state_file);
        return 0;
    }
    step_file = &out_state_file->transition_file.interp_file.payload_decode_file.decode_file.step_file;
    out_state_file->state_status = transition_summary.next_status;

    switch (transition_file->resolution_kind) {
        case MACHINE_TRANSITION_RESOLUTION_NEXT_FETCH:
            out_state_file->resolution_kind = MACHINE_STATE_RESOLUTION_READY;
            out_state_file->has_state = 1;
            out_state_file->state_status = MACHINE_STEP_STATUS_READY;
            out_state_file->program_counter = transition_file->next_program_counter;
            out_state_file->stack_pointer = step_file->stack_pointer;
            out_state_file->has_current_fetch = 1;
            out_state_file->current_segment_index = transition_file->next_current_segment_index;
            out_state_file->current_byte = transition_file->next_current_byte;
            out_state_file->current_byte_memory_offset = transition_file->next_current_byte_memory_offset;
            break;
        case MACHINE_TRANSITION_RESOLUTION_HALT:
            out_state_file->resolution_kind = MACHINE_STATE_RESOLUTION_HALTED;
            out_state_file->has_state = 1;
            out_state_file->state_status = MACHINE_STEP_STATUS_HALTED;
            out_state_file->program_counter = step_file->program_counter;
            out_state_file->stack_pointer = step_file->stack_pointer;
            out_state_file->has_current_fetch = 0;
            break;
        case MACHINE_TRANSITION_RESOLUTION_DEFERRED_CONTROL_TRANSFER:
            out_state_file->resolution_kind = MACHINE_STATE_RESOLUTION_DEFERRED_CONTROL_TRANSFER;
            out_state_file->has_state = 0;
            out_state_file->has_current_fetch = 0;
            break;
        case MACHINE_TRANSITION_RESOLUTION_UNSUPPORTED:
            out_state_file->resolution_kind = MACHINE_STATE_RESOLUTION_UNSUPPORTED;
            out_state_file->has_state = 0;
            out_state_file->has_current_fetch = 0;
            break;
        case MACHINE_TRANSITION_RESOLUTION_NONE:
        default:
            machine_state_set_error(error, 0, 0, "MACHINE-STATE-112: unsupported transition resolution");
            machine_state_file_free(out_state_file);
            return 0;
    }

    if (!machine_state_verify_file(out_state_file, error)) {
        machine_state_file_free(out_state_file);
        return 0;
    }
    return 1;
}

int machine_state_build_from_machine_transition_report(const MachineTransitionReport *report,
    MachineStateFile *out_state_file,
    MachineStateError *error) {
    const MachineTransitionFile *transition_file = NULL;

    if (!report) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-113: invalid transition-report build contract");
        return 0;
    }
    if (!machine_transition_report_get_file(report, &transition_file) || !transition_file) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-114: failed recovering transition file from report");
        return 0;
    }
    return machine_state_build_from_machine_transition_file(transition_file, out_state_file, error);
}

#define MACHINE_STATE_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineStateFile *out_state_file, MachineStateError *error) { \
    MachineTransitionFile transition_file; \
    int ok; \
    machine_transition_file_init(&transition_file); \
    ok = builder(source, &transition_file, NULL) && \
        machine_state_build_from_machine_transition_file(&transition_file, out_state_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-115: failed building state wrapper"); \
    } \
    machine_transition_file_free(&transition_file); \
    return ok; \
}

MACHINE_STATE_DEFINE_BUILD_FROM_WRAPPER(machine_state_build_from_machine_interp_file,
    MachineInterpFile, machine_transition_build_from_machine_interp_file)
MACHINE_STATE_DEFINE_BUILD_FROM_WRAPPER(machine_state_build_from_machine_interp_report,
    MachineInterpReport, machine_transition_build_from_machine_interp_report)
MACHINE_STATE_DEFINE_BUILD_FROM_WRAPPER(machine_state_build_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_transition_build_from_machine_payload_decode_file)
MACHINE_STATE_DEFINE_BUILD_FROM_WRAPPER(machine_state_build_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_transition_build_from_machine_payload_decode_report)
MACHINE_STATE_DEFINE_BUILD_FROM_WRAPPER(machine_state_build_from_machine_decode_file,
    MachineDecodeFile, machine_transition_build_from_machine_decode_file)
MACHINE_STATE_DEFINE_BUILD_FROM_WRAPPER(machine_state_build_from_machine_decode_report,
    MachineDecodeReport, machine_transition_build_from_machine_decode_report)
MACHINE_STATE_DEFINE_BUILD_FROM_WRAPPER(machine_state_build_from_machine_step_file,
    MachineStepFile, machine_transition_build_from_machine_step_file)
MACHINE_STATE_DEFINE_BUILD_FROM_WRAPPER(machine_state_build_from_machine_step_report,
    MachineStepReport, machine_transition_build_from_machine_step_report)
MACHINE_STATE_DEFINE_BUILD_FROM_WRAPPER(machine_state_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_transition_build_from_machine_ir_report)

int machine_state_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineStateFile *out_state_file,
    MachineStateError *error) {
    MachineTransitionFile transition_file;
    int ok;

    machine_transition_file_init(&transition_file);
    ok = machine_transition_build_from_machine_ir_report_with_profile(report, profile, &transition_file, NULL) &&
        machine_state_build_from_machine_transition_file(&transition_file, out_state_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-116: failed building profiled state wrapper");
    }
    machine_transition_file_free(&transition_file);
    return ok;
}

int machine_state_build_report_from_file(const MachineStateFile *source,
    MachineStateReport *out_report,
    MachineStateError *error) {
    if (!source || !out_report) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-117: invalid report build contract");
        return 0;
    }
    if (!machine_state_verify_file(source, error)) {
        return 0;
    }

    machine_state_report_free(out_report);
    if (!machine_state_clone_file(source, &out_report->file, error)) {
        machine_state_report_free(out_report);
        return 0;
    }
    if (!machine_state_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_state_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_transition_build_report_from_file(
            &out_report->file.transition_file, &out_report->transition_report, NULL) ||
        !machine_state_file_get_state_summary(&out_report->file, &out_report->state_summary)) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-118: failed summarizing state report");
        machine_state_report_free(out_report);
        return 0;
    }
    if (out_report->file.has_current_fetch &&
        !machine_state_file_get_current_fetch_summary(&out_report->file, &out_report->current_fetch_summary)) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-119: failed summarizing current fetch");
        machine_state_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineStateReport *out_report, MachineStateError *error) { \
    MachineStateFile state_file; \
    int ok; \
    machine_state_file_init(&state_file); \
    ok = builder(source, &state_file, error) && \
        machine_state_build_report_from_file(&state_file, out_report, error); \
    machine_state_file_free(&state_file); \
    return ok; \
}

MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(machine_state_build_report_from_machine_transition_file,
    MachineTransitionFile, machine_state_build_from_machine_transition_file)
MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(machine_state_build_report_from_machine_transition_report,
    MachineTransitionReport, machine_state_build_from_machine_transition_report)
MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(machine_state_build_report_from_machine_interp_file,
    MachineInterpFile, machine_state_build_from_machine_interp_file)
MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(machine_state_build_report_from_machine_interp_report,
    MachineInterpReport, machine_state_build_from_machine_interp_report)
MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(machine_state_build_report_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_state_build_from_machine_payload_decode_file)
MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(machine_state_build_report_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_state_build_from_machine_payload_decode_report)
MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(machine_state_build_report_from_machine_decode_file,
    MachineDecodeFile, machine_state_build_from_machine_decode_file)
MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(machine_state_build_report_from_machine_decode_report,
    MachineDecodeReport, machine_state_build_from_machine_decode_report)
MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(machine_state_build_report_from_machine_step_file,
    MachineStepFile, machine_state_build_from_machine_step_file)
MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(machine_state_build_report_from_machine_step_report,
    MachineStepReport, machine_state_build_from_machine_step_report)
MACHINE_STATE_DEFINE_REPORT_FROM_WRAPPER(machine_state_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_state_build_from_machine_ir_report)

int machine_state_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineStateReport *out_report,
    MachineStateError *error) {
    MachineStateFile state_file;
    int ok;

    machine_state_file_init(&state_file);
    ok = machine_state_build_from_machine_ir_report_with_profile(report, profile, &state_file, error) &&
        machine_state_build_report_from_file(&state_file, out_report, error);
    machine_state_file_free(&state_file);
    return ok;
}

int machine_state_report_refresh(MachineStateReport *report,
    MachineStateError *error) {
    MachineStateReport refreshed_report;
    int ok;

    if (!report) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-120: invalid report refresh contract");
        return 0;
    }
    machine_state_report_init(&refreshed_report);
    ok = machine_state_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_state_report_free(report);
        *report = refreshed_report;
    } else {
        machine_state_report_free(&refreshed_report);
    }
    return ok;
}

int machine_state_dump_file(const MachineStateFile *state_file,
    char **out_text,
    MachineStateError *error) {
    MachineStateStringBuilder builder;
    MachineStateHeaderSummary header_summary;
    MachineStateSummary state_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&state_summary, 0, sizeof(state_summary));
    if (!state_file || !out_text ||
        !machine_state_verify_file(state_file, error) ||
        !machine_state_file_get_header_summary(state_file, &header_summary) ||
        !machine_state_file_get_state_summary(state_file, &state_summary)) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-121: invalid dump contract");
        return 0;
    }

    if (!machine_state_append_format(
            &builder,
            "machine_state profile=%s elf_origin=%s elf_semantics=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_elf_target_profile_name(
                state_file->transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                state_file->transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_state_append_format(
            &builder,
            "state: resolution=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu has-state=%s status=%s pc=%s",
            machine_state_resolution_kind_name(state_summary.resolution_kind),
            machine_transition_resolution_kind_name(state_summary.transition_resolution_kind),
            machine_interp_action_kind_name(state_summary.action_kind),
            (unsigned int)state_summary.raw_byte,
            (unsigned int)state_summary.tag_value,
            state_summary.is_known ? "yes" : "no",
            state_summary.tag_name ? state_summary.tag_name : "-",
            state_summary.instruction_byte_count,
            state_summary.has_state ? "yes" : "no",
            state_summary.has_state ? machine_step_status_name(state_summary.state_status) : "-",
            state_summary.has_state ? "" : "-")) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-122: out of memory building dump");
        return 0;
    }
    if (state_summary.has_state &&
        !machine_state_append_format(&builder, "0x%zx", state_summary.program_counter)) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-123: out of memory appending state pc");
        return 0;
    }
    if (!machine_state_append_format(
            &builder,
            " sp=%s",
            state_summary.has_state ? "" : "-")) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-124: out of memory appending state sp");
        return 0;
    }
    if (state_summary.has_state &&
        !machine_state_append_format(&builder, "0x%zx", state_summary.stack_pointer)) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-125: out of memory appending state sp value");
        return 0;
    }
    if (!machine_state_append_format(
            &builder,
            " current-segment=%s",
            state_summary.has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-126: out of memory appending current segment");
        return 0;
    }
    if (state_summary.has_current_fetch &&
        !machine_state_append_format(&builder, "%zu", state_summary.current_segment_index)) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-127: out of memory appending current segment value");
        return 0;
    }
    if (!machine_state_append_format(
            &builder,
            " current-byte=%s",
            state_summary.has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-128: out of memory appending current byte");
        return 0;
    }
    if (state_summary.has_current_fetch &&
        !machine_state_append_format(&builder, "0x%02x", (unsigned int)state_summary.current_byte)) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-129: out of memory appending current byte value");
        return 0;
    }
    if (!machine_state_append_targets(&builder, &state_summary) ||
        !machine_state_append_return_immediate(&builder, &state_summary) ||
        !machine_state_append_format(&builder, "\n")) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-130: out of memory terminating dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_state_build_dump_from_file(const MachineStateFile *source,
    char **out_text,
    MachineStateError *error) {
    return machine_state_dump_file(source, out_text, error);
}

#define MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineStateError *error) { \
    MachineStateFile state_file; \
    int ok; \
    machine_state_file_init(&state_file); \
    ok = builder(source, &state_file, error) && \
        machine_state_dump_file(&state_file, out_text, error); \
    machine_state_file_free(&state_file); \
    return ok; \
}

MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(machine_state_build_dump_from_machine_transition_file,
    MachineTransitionFile, machine_state_build_from_machine_transition_file)
MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(machine_state_build_dump_from_machine_transition_report,
    MachineTransitionReport, machine_state_build_from_machine_transition_report)
MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(machine_state_build_dump_from_machine_interp_file,
    MachineInterpFile, machine_state_build_from_machine_interp_file)
MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(machine_state_build_dump_from_machine_interp_report,
    MachineInterpReport, machine_state_build_from_machine_interp_report)
MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(machine_state_build_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_state_build_from_machine_payload_decode_file)
MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(machine_state_build_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_state_build_from_machine_payload_decode_report)
MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(machine_state_build_dump_from_machine_decode_file,
    MachineDecodeFile, machine_state_build_from_machine_decode_file)
MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(machine_state_build_dump_from_machine_decode_report,
    MachineDecodeReport, machine_state_build_from_machine_decode_report)
MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(machine_state_build_dump_from_machine_step_file,
    MachineStepFile, machine_state_build_from_machine_step_file)
MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(machine_state_build_dump_from_machine_step_report,
    MachineStepReport, machine_state_build_from_machine_step_report)
MACHINE_STATE_DEFINE_DUMP_FROM_WRAPPER(machine_state_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_state_build_from_machine_ir_report)

int machine_state_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineStateError *error) {
    MachineStateFile state_file;
    int ok;

    machine_state_file_init(&state_file);
    ok = machine_state_build_from_machine_ir_report_with_profile(report, profile, &state_file, error) &&
        machine_state_dump_file(&state_file, out_text, error);
    machine_state_file_free(&state_file);
    return ok;
}

int machine_state_report_get_summary(const MachineStateReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_state_file_get_source_elf_artifact_summary(const MachineStateFile *state_file,
    MachineElfArtifactSummary *out_summary) {
    if (!state_file || !out_summary) {
        return 0;
    }
    *out_summary =
        state_file->transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file
            .load_file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

int machine_state_report_get_overview_artifact(const MachineStateReport *report,
    MachineStateReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->transition_report = &report->transition_report;
    out_artifact->state_summary = &report->state_summary;
    out_artifact->current_fetch_summary = report->file.has_current_fetch ? &report->current_fetch_summary : NULL;
    return 1;
}

int machine_state_report_get_file(const MachineStateReport *report,
    const MachineStateFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_state_report_get_transition_file(const MachineStateReport *report,
    const MachineTransitionFile **out_transition_file) {
    if (!report || !out_transition_file) {
        return 0;
    }
    *out_transition_file = &report->file.transition_file;
    return 1;
}

int machine_state_report_get_transition_report(const MachineStateReport *report,
    const MachineTransitionReport **out_transition_report) {
    if (!report || !out_transition_report) {
        return 0;
    }
    *out_transition_report = &report->transition_report;
    return 1;
}

int machine_state_report_get_source_elf_artifact_summary_artifact(const MachineStateReport *report,
    const MachineElfArtifactSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file
                         .runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

#define MACHINE_STATE_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineStateReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_STATE_DEFINE_REPORT_ARTIFACT_GETTER(machine_state_report_get_header_summary_artifact,
    header_summary, MachineStateHeaderSummary)
MACHINE_STATE_DEFINE_REPORT_ARTIFACT_GETTER(machine_state_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineStateTargetPolicySummary)
MACHINE_STATE_DEFINE_REPORT_ARTIFACT_GETTER(machine_state_report_get_state_summary_artifact,
    state_summary, MachineStateSummary)

int machine_state_report_get_current_fetch_summary_artifact(const MachineStateReport *report,
    const MachineStateCurrentFetchSummary **out_summary) {
    if (!report || !out_summary || !report->file.has_current_fetch) {
        return 0;
    }
    *out_summary = &report->current_fetch_summary;
    return 1;
}

int machine_state_report_overview_artifact_get_transition_report(
    const MachineStateReportOverviewArtifact *artifact,
    const MachineTransitionReport **out_transition_report) {
    if (!artifact || !out_transition_report) {
        return 0;
    }
    *out_transition_report = artifact->transition_report;
    return 1;
}

int machine_state_report_overview_artifact_get_state_summary_artifact(
    const MachineStateReportOverviewArtifact *artifact,
    const MachineStateSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->state_summary;
    return 1;
}

int machine_state_report_overview_artifact_get_current_fetch_summary_artifact(
    const MachineStateReportOverviewArtifact *artifact,
    const MachineStateCurrentFetchSummary **out_summary) {
    if (!artifact || !out_summary || !artifact->current_fetch_summary) {
        return 0;
    }
    *out_summary = artifact->current_fetch_summary;
    return 1;
}

int machine_state_dump_report(const MachineStateReport *report,
    char **out_text,
    MachineStateError *error) {
    MachineStateStringBuilder builder;
    const MachineStateSummary *state_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_state_report_get_state_summary_artifact(report, &state_summary) ||
        !state_summary) {
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-131: invalid report dump contract");
        return 0;
    }

    if (!machine_state_append_format(
            &builder,
            "machine_state profile=%s elf_origin=%s elf_semantics=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_state_append_format(
            &builder,
            "state: resolution=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu has-state=%s status=%s pc=%s",
            machine_state_resolution_kind_name(state_summary->resolution_kind),
            machine_transition_resolution_kind_name(state_summary->transition_resolution_kind),
            machine_interp_action_kind_name(state_summary->action_kind),
            (unsigned int)state_summary->raw_byte,
            (unsigned int)state_summary->tag_value,
            state_summary->is_known ? "yes" : "no",
            state_summary->tag_name ? state_summary->tag_name : "-",
            state_summary->instruction_byte_count,
            state_summary->has_state ? "yes" : "no",
            state_summary->has_state ? machine_step_status_name(state_summary->state_status) : "-",
            state_summary->has_state ? "" : "-")) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-132: out of memory building report dump");
        return 0;
    }
    if (state_summary->has_state &&
        !machine_state_append_format(&builder, "0x%zx", state_summary->program_counter)) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-133: out of memory appending report state pc");
        return 0;
    }
    if (!machine_state_append_format(
            &builder,
            " sp=%s",
            state_summary->has_state ? "" : "-")) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-134: out of memory appending report state sp");
        return 0;
    }
    if (state_summary->has_state &&
        !machine_state_append_format(&builder, "0x%zx", state_summary->stack_pointer)) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-135: out of memory appending report state sp value");
        return 0;
    }
    if (!machine_state_append_format(
            &builder,
            " current-segment=%s",
            state_summary->has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-136: out of memory appending report current segment");
        return 0;
    }
    if (state_summary->has_current_fetch &&
        !machine_state_append_format(&builder, "%zu", state_summary->current_segment_index)) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-137: out of memory appending report current segment value");
        return 0;
    }
    if (!machine_state_append_format(
            &builder,
            " current-byte=%s",
            state_summary->has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-138: out of memory appending report current byte");
        return 0;
    }
    if (state_summary->has_current_fetch &&
        !machine_state_append_format(&builder, "0x%02x", (unsigned int)state_summary->current_byte)) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-139: out of memory appending report current byte value");
        return 0;
    }
    if (!machine_state_append_targets(&builder, state_summary) ||
        !machine_state_append_return_immediate(&builder, state_summary) ||
        !machine_state_append_format(&builder, "\nreport_overview:\n") ||
        !machine_state_append_format(
            &builder,
            "  origin: status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer) ||
        !machine_state_append_format(
            &builder,
            "  elf_source: target=%s origin=%s semantics=%s\n",
            machine_elf_target_profile_name(
                report->file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics)) ||
        !machine_state_append_format(
            &builder,
            "  policy: profile=%s ready=%s halt=%s deferred-control=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.resolves_ready_state ? "yes" : "no",
            report->target_policy_summary.resolves_halt_state ? "yes" : "no",
            report->target_policy_summary.defers_control_transfer_state ? "yes" : "no") ||
        !machine_state_append_format(
            &builder,
            "  state: resolution=%s has-state=%s status=%s pc=%s",
            machine_state_resolution_kind_name(state_summary->resolution_kind),
            state_summary->has_state ? "yes" : "no",
            state_summary->has_state ? machine_step_status_name(state_summary->state_status) : "-",
            state_summary->has_state ? "" : "-")) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-140: out of memory building report overview");
        return 0;
    }
    if (state_summary->has_state &&
        !machine_state_append_format(&builder, "0x%zx", state_summary->program_counter)) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-141: out of memory appending report overview pc");
        return 0;
    }
    if (!machine_state_append_targets(&builder, state_summary) ||
        !machine_state_append_return_immediate(&builder, state_summary) ||
        !machine_state_append_format(&builder, "\n")) {
        free(builder.data);
        machine_state_set_error(error, 0, 0, "MACHINE-STATE-142: out of memory terminating report overview");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_state_build_report_dump_from_file(const MachineStateFile *source,
    char **out_text,
    MachineStateError *error) {
    MachineStateReport report;
    int ok;

    machine_state_report_init(&report);
    ok = machine_state_build_report_from_file(source, &report, error) &&
        machine_state_dump_report(&report, out_text, error);
    machine_state_report_free(&report);
    return ok;
}

#define MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineStateError *error) { \
    MachineStateReport report; \
    int ok; \
    machine_state_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_state_dump_report(&report, out_text, error); \
    machine_state_report_free(&report); \
    return ok; \
}

MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_state_build_report_dump_from_machine_transition_file,
    MachineTransitionFile, machine_state_build_report_from_machine_transition_file)
MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_state_build_report_dump_from_machine_transition_report,
    MachineTransitionReport, machine_state_build_report_from_machine_transition_report)
MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_state_build_report_dump_from_machine_interp_file,
    MachineInterpFile, machine_state_build_report_from_machine_interp_file)
MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_state_build_report_dump_from_machine_interp_report,
    MachineInterpReport, machine_state_build_report_from_machine_interp_report)
MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_state_build_report_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_state_build_report_from_machine_payload_decode_file)
MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_state_build_report_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_state_build_report_from_machine_payload_decode_report)
MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_state_build_report_dump_from_machine_decode_file,
    MachineDecodeFile, machine_state_build_report_from_machine_decode_file)
MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_state_build_report_dump_from_machine_decode_report,
    MachineDecodeReport, machine_state_build_report_from_machine_decode_report)
MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_state_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_state_build_report_from_machine_step_file)
MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_state_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_state_build_report_from_machine_step_report)
MACHINE_STATE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_state_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_state_build_report_from_machine_ir_report)

int machine_state_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineStateError *error) {
    MachineStateReport report_file;
    int ok;

    machine_state_report_init(&report_file);
    ok = machine_state_build_report_from_machine_ir_report_with_profile(report, profile, &report_file, error) &&
        machine_state_dump_report(&report_file, out_text, error);
    machine_state_report_free(&report_file);
    return ok;
}
