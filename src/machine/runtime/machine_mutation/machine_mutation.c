#include "machine/mutation.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineMutationStringBuilder;

static void machine_mutation_set_error(MachineMutationError *error, int line, int column, const char *fmt, ...);
static int machine_mutation_append_format(MachineMutationStringBuilder *builder, const char *fmt, ...);
static int machine_mutation_append_targets(MachineMutationStringBuilder *builder,
    const MachineMutationSummary *mutation_summary);
static int machine_mutation_append_return_immediate(MachineMutationStringBuilder *builder,
    const MachineMutationSummary *mutation_summary);
static int machine_mutation_classify_known_op(unsigned char tag_value,
    MachineMutationResolutionKind *out_resolution_kind,
    MachineMutationEffectKind *out_effect_kind);

static void machine_mutation_set_error(MachineMutationError *error, int line, int column, const char *fmt, ...) {
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

static int machine_mutation_append_format(MachineMutationStringBuilder *builder, const char *fmt, ...) {
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

static int machine_mutation_append_targets(MachineMutationStringBuilder *builder,
    const MachineMutationSummary *mutation_summary) {
    if (!builder || !mutation_summary) {
        return 0;
    }
    if (!mutation_summary->has_primary_target_block) {
        return machine_mutation_append_format(builder, " targets=[]");
    }
    if (!mutation_summary->has_secondary_target_block) {
        return machine_mutation_append_format(
            builder,
            " targets=[%zu]",
            mutation_summary->primary_target_block_index);
    }
    return machine_mutation_append_format(
        builder,
        " targets=[%zu,%zu]",
        mutation_summary->primary_target_block_index,
        mutation_summary->secondary_target_block_index);
}

static int machine_mutation_append_return_immediate(MachineMutationStringBuilder *builder,
    const MachineMutationSummary *mutation_summary) {
    if (!builder || !mutation_summary) {
        return 0;
    }
    if (!mutation_summary->has_return_immediate) {
        return machine_mutation_append_format(builder, " return-imm=-");
    }
    return machine_mutation_append_format(
        builder,
        " return-imm=%zu",
        mutation_summary->return_immediate);
}

static int machine_mutation_classify_known_op(unsigned char tag_value,
    MachineMutationResolutionKind *out_resolution_kind,
    MachineMutationEffectKind *out_effect_kind) {
    if (!out_resolution_kind || !out_effect_kind) {
        return 0;
    }
    switch ((MachineSelectOpKind)tag_value) {
        case MACHINE_SELECT_OP_COPY:
        case MACHINE_SELECT_OP_MATERIALIZE_IMM:
        case MACHINE_SELECT_OP_ALU:
        case MACHINE_SELECT_OP_ALU_IMM:
        case MACHINE_SELECT_OP_CMP:
        case MACHINE_SELECT_OP_CMP_IMM:
        case MACHINE_SELECT_OP_ADDR_LOCAL:
        case MACHINE_SELECT_OP_ADDR_GLOBAL:
        case MACHINE_SELECT_OP_LOAD_LOCAL:
        case MACHINE_SELECT_OP_LOAD_GLOBAL:
        case MACHINE_SELECT_OP_LOAD_INDIRECT:
            *out_resolution_kind = MACHINE_MUTATION_RESOLUTION_DEFERRED_REGISTER_RESULT;
            *out_effect_kind = MACHINE_MUTATION_EFFECT_VALUE_RESULT;
            return 1;
        case MACHINE_SELECT_OP_STORE_LOCAL:
        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
            *out_resolution_kind = MACHINE_MUTATION_RESOLUTION_DEFERRED_LOCAL_SLOT;
            *out_effect_kind = MACHINE_MUTATION_EFFECT_LOCAL_SLOT;
            return 1;
        case MACHINE_SELECT_OP_STORE_GLOBAL:
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
        case MACHINE_SELECT_OP_STORE_INDIRECT:
            *out_resolution_kind = MACHINE_MUTATION_RESOLUTION_DEFERRED_GLOBAL_SLOT;
            *out_effect_kind = MACHINE_MUTATION_EFFECT_GLOBAL_SLOT;
            return 1;
        case MACHINE_SELECT_OP_CALL:
        case MACHINE_SELECT_OP_CALL_IMM:
        case MACHINE_SELECT_OP_CALL_SPILL:
        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
        case MACHINE_SELECT_OP_CALL_VOID:
        case MACHINE_SELECT_OP_CALL_VOID_IMM:
            *out_resolution_kind = MACHINE_MUTATION_RESOLUTION_DEFERRED_CALL_EFFECT;
            *out_effect_kind = MACHINE_MUTATION_EFFECT_CALL;
            return 1;
    }
    return 0;
}

void machine_mutation_file_init(MachineMutationFile *mutation_file) {
    if (!mutation_file) {
        return;
    }
    machine_state_file_init(&mutation_file->state_file);
    mutation_file->resolution_kind = MACHINE_MUTATION_RESOLUTION_NONE;
    mutation_file->effect_kind = MACHINE_MUTATION_EFFECT_NONE;
}

void machine_mutation_file_free(MachineMutationFile *mutation_file) {
    if (!mutation_file) {
        return;
    }
    machine_state_file_free(&mutation_file->state_file);
    machine_mutation_file_init(mutation_file);
}

void machine_mutation_report_init(MachineMutationReport *report) {
    if (!report) {
        return;
    }
    machine_mutation_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_state_report_init(&report->state_report);
    memset(&report->mutation_summary, 0, sizeof(report->mutation_summary));
}

void machine_mutation_report_free(MachineMutationReport *report) {
    if (!report) {
        return;
    }
    machine_state_report_free(&report->state_report);
    machine_mutation_file_free(&report->file);
    machine_mutation_report_init(report);
}

const char *machine_mutation_resolution_kind_name(MachineMutationResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_MUTATION_RESOLUTION_NONE:
            return "none";
        case MACHINE_MUTATION_RESOLUTION_NO_MUTATION:
            return "no-mutation";
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_REGISTER_RESULT:
            return "deferred-register-result";
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_LOCAL_SLOT:
            return "deferred-local-slot";
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_GLOBAL_SLOT:
            return "deferred-global-slot";
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_CALL_EFFECT:
            return "deferred-call-effect";
        case MACHINE_MUTATION_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_MUTATION_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_mutation_effect_kind_name(MachineMutationEffectKind effect_kind) {
    switch (effect_kind) {
        case MACHINE_MUTATION_EFFECT_NONE:
            return "none";
        case MACHINE_MUTATION_EFFECT_CONTROL_ONLY:
            return "control-only";
        case MACHINE_MUTATION_EFFECT_VALUE_RESULT:
            return "value-result";
        case MACHINE_MUTATION_EFFECT_LOCAL_SLOT:
            return "local-slot";
        case MACHINE_MUTATION_EFFECT_GLOBAL_SLOT:
            return "global-slot";
        case MACHINE_MUTATION_EFFECT_CALL:
            return "call";
    }
    return "unknown";
}

int machine_mutation_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineMutationTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->classifies_control_only = 1;
    out_summary->defers_register_result_mutation = 1;
    out_summary->defers_slot_mutation = 1;
    out_summary->defers_call_effect = 1;
    return 1;
}

int machine_mutation_file_get_target_policy_summary(const MachineMutationFile *mutation_file,
    MachineMutationTargetPolicySummary *out_summary) {
    if (!mutation_file || !out_summary) {
        return 0;
    }
    return machine_mutation_get_target_policy_summary(
        mutation_file->state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_mutation_file_get_summary(const MachineMutationFile *mutation_file,
    size_t *out_mapped_byte_count) {
    if (!mutation_file) {
        return 0;
    }
    return machine_state_file_get_summary(&mutation_file->state_file, out_mapped_byte_count);
}

int machine_mutation_file_get_header_summary(const MachineMutationFile *mutation_file,
    MachineMutationHeaderSummary *out_summary) {
    MachineStateHeaderSummary state_header_summary;

    memset(&state_header_summary, 0, sizeof(state_header_summary));
    if (!mutation_file || !out_summary ||
        !machine_state_file_get_header_summary(&mutation_file->state_file, &state_header_summary)) {
        return 0;
    }
    out_summary->target_profile = state_header_summary.target_profile;
    out_summary->state_resolution_kind = mutation_file->state_file.resolution_kind;
    out_summary->origin_step_status = state_header_summary.origin_step_status;
    out_summary->origin_program_counter = state_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = state_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = state_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = state_header_summary.mapped_byte_count;
    return 1;
}

int machine_mutation_file_get_state_summary(const MachineMutationFile *mutation_file,
    MachineStateSummary *out_summary) {
    if (!mutation_file || !out_summary) {
        return 0;
    }
    return machine_state_file_get_state_summary(&mutation_file->state_file, out_summary);
}

int machine_mutation_file_get_payload_summary(const MachineMutationFile *mutation_file,
    MachinePayloadDecodeSummary *out_summary) {
    if (!mutation_file || !out_summary) {
        return 0;
    }
    return machine_state_file_get_payload_summary(&mutation_file->state_file, out_summary);
}

int machine_mutation_file_get_decode_tag_summary(const MachineMutationFile *mutation_file,
    MachineDecodeTagSummary *out_summary) {
    if (!mutation_file || !out_summary) {
        return 0;
    }
    return machine_state_file_get_decode_tag_summary(&mutation_file->state_file, out_summary);
}

int machine_mutation_file_get_mutation_summary(const MachineMutationFile *mutation_file,
    MachineMutationSummary *out_summary) {
    MachineStateSummary state_summary;
    MachinePayloadDecodeSummary payload_summary;
    MachineDecodeTagSummary decode_tag_summary;

    memset(&state_summary, 0, sizeof(state_summary));
    memset(&payload_summary, 0, sizeof(payload_summary));
    memset(&decode_tag_summary, 0, sizeof(decode_tag_summary));
    if (!mutation_file || !out_summary ||
        !machine_mutation_file_get_state_summary(mutation_file, &state_summary) ||
        !machine_mutation_file_get_payload_summary(mutation_file, &payload_summary) ||
        !machine_mutation_file_get_decode_tag_summary(mutation_file, &decode_tag_summary)) {
        return 0;
    }
    out_summary->resolution_kind = mutation_file->resolution_kind;
    out_summary->effect_kind = mutation_file->effect_kind;
    out_summary->state_resolution_kind = state_summary.resolution_kind;
    out_summary->transition_resolution_kind = state_summary.transition_resolution_kind;
    out_summary->action_kind = state_summary.action_kind;
    out_summary->payload_kind = payload_summary.kind;
    out_summary->tag_class = decode_tag_summary.tag_class;
    out_summary->raw_byte = state_summary.raw_byte;
    out_summary->tag_value = state_summary.tag_value;
    out_summary->is_known = state_summary.is_known;
    out_summary->tag_name = state_summary.tag_name;
    out_summary->instruction_byte_count = state_summary.instruction_byte_count;
    out_summary->has_state = state_summary.has_state;
    out_summary->state_status = state_summary.state_status;
    out_summary->program_counter = state_summary.program_counter;
    out_summary->stack_pointer = state_summary.stack_pointer;
    out_summary->has_current_fetch = state_summary.has_current_fetch;
    out_summary->current_segment_index = state_summary.current_segment_index;
    out_summary->current_byte = state_summary.current_byte;
    out_summary->current_byte_memory_offset = state_summary.current_byte_memory_offset;
    out_summary->has_primary_target_block = state_summary.has_primary_target_block;
    out_summary->primary_target_block_index = state_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = state_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = state_summary.secondary_target_block_index;
    out_summary->has_return_immediate = state_summary.has_return_immediate;
    out_summary->return_immediate = state_summary.return_immediate;
    return 1;
}

int machine_mutation_verify_file(const MachineMutationFile *mutation_file,
    MachineMutationError *error) {
    MachineStateSummary state_summary;
    MachinePayloadDecodeSummary payload_summary;
    MachineDecodeTagSummary decode_tag_summary;
    MachineMutationResolutionKind expected_resolution_kind;
    MachineMutationEffectKind expected_effect_kind;

    memset(&state_summary, 0, sizeof(state_summary));
    memset(&payload_summary, 0, sizeof(payload_summary));
    memset(&decode_tag_summary, 0, sizeof(decode_tag_summary));
    expected_resolution_kind = MACHINE_MUTATION_RESOLUTION_NONE;
    expected_effect_kind = MACHINE_MUTATION_EFFECT_NONE;
    if (!mutation_file) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-100: invalid mutation file");
        return 0;
    }
    if (!machine_state_verify_file(&mutation_file->state_file, NULL) ||
        !machine_mutation_file_get_state_summary(mutation_file, &state_summary) ||
        !machine_mutation_file_get_payload_summary(mutation_file, &payload_summary) ||
        !machine_mutation_file_get_decode_tag_summary(mutation_file, &decode_tag_summary)) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-101: invalid state input");
        return 0;
    }

    switch (mutation_file->state_file.resolution_kind) {
        case MACHINE_STATE_RESOLUTION_READY:
            if (payload_summary.kind != MACHINE_PAYLOAD_DECODE_KIND_OP ||
                decode_tag_summary.tag_class != MACHINE_DECODE_TAG_OP ||
                !machine_mutation_classify_known_op(
                    decode_tag_summary.tag_value, &expected_resolution_kind, &expected_effect_kind)) {
                machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-102: invalid ready-state mutation shape");
                return 0;
            }
            break;
        case MACHINE_STATE_RESOLUTION_HALTED:
            if (payload_summary.kind != MACHINE_PAYLOAD_DECODE_KIND_TERMINATOR ||
                decode_tag_summary.tag_class != MACHINE_DECODE_TAG_TERMINATOR ||
                state_summary.action_kind != MACHINE_INTERP_ACTION_HALT) {
                machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-103: invalid halted-state mutation shape");
                return 0;
            }
            expected_resolution_kind = MACHINE_MUTATION_RESOLUTION_NO_MUTATION;
            expected_effect_kind = MACHINE_MUTATION_EFFECT_CONTROL_ONLY;
            break;
        case MACHINE_STATE_RESOLUTION_DEFERRED_CONTROL_TRANSFER:
            if (payload_summary.kind != MACHINE_PAYLOAD_DECODE_KIND_TERMINATOR ||
                decode_tag_summary.tag_class != MACHINE_DECODE_TAG_TERMINATOR ||
                state_summary.action_kind != MACHINE_INTERP_ACTION_CONTROL_TRANSFER) {
                machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-104: invalid control-blocked mutation shape");
                return 0;
            }
            expected_resolution_kind = MACHINE_MUTATION_RESOLUTION_BLOCKED_ON_CONTROL;
            expected_effect_kind = MACHINE_MUTATION_EFFECT_CONTROL_ONLY;
            break;
        case MACHINE_STATE_RESOLUTION_UNSUPPORTED:
            expected_resolution_kind = MACHINE_MUTATION_RESOLUTION_BLOCKED_UNSUPPORTED;
            expected_effect_kind = MACHINE_MUTATION_EFFECT_NONE;
            break;
        case MACHINE_STATE_RESOLUTION_NONE:
        default:
            machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-105: invalid state resolution kind");
            return 0;
    }

    if (mutation_file->resolution_kind != expected_resolution_kind ||
        mutation_file->effect_kind != expected_effect_kind) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-106: mutation classification mismatch");
        return 0;
    }
    return 1;
}

int machine_mutation_clone_file(const MachineMutationFile *source,
    MachineMutationFile *out_clone,
    MachineMutationError *error) {
    if (!source || !out_clone) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-107: invalid clone contract");
        return 0;
    }
    if (!machine_mutation_verify_file(source, error)) {
        return 0;
    }

    machine_mutation_file_free(out_clone);
    if (!machine_state_clone_file(&source->state_file, &out_clone->state_file, NULL)) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-108: failed cloning state input");
        machine_mutation_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->effect_kind = source->effect_kind;
    return 1;
}

int machine_mutation_build_from_machine_state_file(const MachineStateFile *state_file,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error) {
    MachineStateSummary state_summary;
    MachineDecodeTagSummary decode_tag_summary;
    MachineMutationResolutionKind resolution_kind;
    MachineMutationEffectKind effect_kind;

    memset(&state_summary, 0, sizeof(state_summary));
    memset(&decode_tag_summary, 0, sizeof(decode_tag_summary));
    resolution_kind = MACHINE_MUTATION_RESOLUTION_NONE;
    effect_kind = MACHINE_MUTATION_EFFECT_NONE;
    if (!state_file || !out_mutation_file) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-109: invalid build contract");
        return 0;
    }
    if (!machine_state_verify_file(state_file, NULL) ||
        !machine_state_file_get_state_summary(state_file, &state_summary) ||
        !machine_state_file_get_decode_tag_summary(state_file, &decode_tag_summary)) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-110: invalid state input");
        return 0;
    }

    switch (state_file->resolution_kind) {
        case MACHINE_STATE_RESOLUTION_READY:
            if (!machine_mutation_classify_known_op(
                    decode_tag_summary.tag_value, &resolution_kind, &effect_kind)) {
                machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-111: unsupported ready-state op family");
                return 0;
            }
            break;
        case MACHINE_STATE_RESOLUTION_HALTED:
            resolution_kind = MACHINE_MUTATION_RESOLUTION_NO_MUTATION;
            effect_kind = MACHINE_MUTATION_EFFECT_CONTROL_ONLY;
            break;
        case MACHINE_STATE_RESOLUTION_DEFERRED_CONTROL_TRANSFER:
            resolution_kind = MACHINE_MUTATION_RESOLUTION_BLOCKED_ON_CONTROL;
            effect_kind = MACHINE_MUTATION_EFFECT_CONTROL_ONLY;
            break;
        case MACHINE_STATE_RESOLUTION_UNSUPPORTED:
            resolution_kind = MACHINE_MUTATION_RESOLUTION_BLOCKED_UNSUPPORTED;
            effect_kind = MACHINE_MUTATION_EFFECT_NONE;
            break;
        case MACHINE_STATE_RESOLUTION_NONE:
        default:
            machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-112: unsupported state resolution");
            return 0;
    }

    machine_mutation_file_free(out_mutation_file);
    if (!machine_state_clone_file(state_file, &out_mutation_file->state_file, NULL)) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-113: failed cloning state input");
        machine_mutation_file_free(out_mutation_file);
        return 0;
    }
    out_mutation_file->resolution_kind = resolution_kind;
    out_mutation_file->effect_kind = effect_kind;

    if (!machine_mutation_verify_file(out_mutation_file, error)) {
        machine_mutation_file_free(out_mutation_file);
        return 0;
    }
    return 1;
}

int machine_mutation_build_from_machine_state_report(const MachineStateReport *report,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error) {
    const MachineStateFile *state_file = NULL;

    if (!report) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-114: invalid state-report build contract");
        return 0;
    }
    if (!machine_state_report_get_file(report, &state_file) || !state_file) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-115: failed recovering state file from report");
        return 0;
    }
    return machine_mutation_build_from_machine_state_file(state_file, out_mutation_file, error);
}

#define MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineMutationFile *out_mutation_file, MachineMutationError *error) { \
    MachineStateFile state_file; \
    int ok; \
    machine_state_file_init(&state_file); \
    ok = builder(source, &state_file, NULL) && \
        machine_mutation_build_from_machine_state_file(&state_file, out_mutation_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-116: failed building mutation wrapper"); \
    } \
    machine_state_file_free(&state_file); \
    return ok; \
}

MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(machine_mutation_build_from_machine_transition_file,
    MachineTransitionFile, machine_state_build_from_machine_transition_file)
MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(machine_mutation_build_from_machine_transition_report,
    MachineTransitionReport, machine_state_build_from_machine_transition_report)
MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(machine_mutation_build_from_machine_interp_file,
    MachineInterpFile, machine_state_build_from_machine_interp_file)
MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(machine_mutation_build_from_machine_interp_report,
    MachineInterpReport, machine_state_build_from_machine_interp_report)
MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(machine_mutation_build_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_state_build_from_machine_payload_decode_file)
MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(machine_mutation_build_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_state_build_from_machine_payload_decode_report)
MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(machine_mutation_build_from_machine_decode_file,
    MachineDecodeFile, machine_state_build_from_machine_decode_file)
MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(machine_mutation_build_from_machine_decode_report,
    MachineDecodeReport, machine_state_build_from_machine_decode_report)
MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(machine_mutation_build_from_machine_step_file,
    MachineStepFile, machine_state_build_from_machine_step_file)
MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(machine_mutation_build_from_machine_step_report,
    MachineStepReport, machine_state_build_from_machine_step_report)
MACHINE_MUTATION_DEFINE_BUILD_FROM_WRAPPER(machine_mutation_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_state_build_from_machine_ir_report)

int machine_mutation_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error) {
    MachineStateFile state_file;
    int ok;

    machine_state_file_init(&state_file);
    ok = machine_state_build_from_machine_ir_report_with_profile(report, profile, &state_file, NULL) &&
        machine_mutation_build_from_machine_state_file(&state_file, out_mutation_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-117: failed building profiled mutation wrapper");
    }
    machine_state_file_free(&state_file);
    return ok;
}

int machine_mutation_build_report_from_file(const MachineMutationFile *source,
    MachineMutationReport *out_report,
    MachineMutationError *error) {
    if (!source || !out_report) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-118: invalid report build contract");
        return 0;
    }
    if (!machine_mutation_verify_file(source, error)) {
        return 0;
    }

    machine_mutation_report_free(out_report);
    if (!machine_mutation_clone_file(source, &out_report->file, error)) {
        machine_mutation_report_free(out_report);
        return 0;
    }
    if (!machine_mutation_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_mutation_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_state_build_report_from_file(&out_report->file.state_file, &out_report->state_report, NULL) ||
        !machine_mutation_file_get_mutation_summary(&out_report->file, &out_report->mutation_summary)) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-119: failed summarizing mutation report");
        machine_mutation_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineMutationReport *out_report, MachineMutationError *error) { \
    MachineMutationFile mutation_file; \
    int ok; \
    machine_mutation_file_init(&mutation_file); \
    ok = builder(source, &mutation_file, error) && \
        machine_mutation_build_report_from_file(&mutation_file, out_report, error); \
    machine_mutation_file_free(&mutation_file); \
    return ok; \
}

MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_state_file,
    MachineStateFile, machine_mutation_build_from_machine_state_file)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_state_report,
    MachineStateReport, machine_mutation_build_from_machine_state_report)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_transition_file,
    MachineTransitionFile, machine_mutation_build_from_machine_transition_file)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_transition_report,
    MachineTransitionReport, machine_mutation_build_from_machine_transition_report)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_interp_file,
    MachineInterpFile, machine_mutation_build_from_machine_interp_file)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_interp_report,
    MachineInterpReport, machine_mutation_build_from_machine_interp_report)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_mutation_build_from_machine_payload_decode_file)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_mutation_build_from_machine_payload_decode_report)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_decode_file,
    MachineDecodeFile, machine_mutation_build_from_machine_decode_file)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_decode_report,
    MachineDecodeReport, machine_mutation_build_from_machine_decode_report)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_step_file,
    MachineStepFile, machine_mutation_build_from_machine_step_file)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_step_report,
    MachineStepReport, machine_mutation_build_from_machine_step_report)
MACHINE_MUTATION_DEFINE_REPORT_FROM_WRAPPER(machine_mutation_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_mutation_build_from_machine_ir_report)

int machine_mutation_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineMutationReport *out_report,
    MachineMutationError *error) {
    MachineMutationFile mutation_file;
    int ok;

    machine_mutation_file_init(&mutation_file);
    ok = machine_mutation_build_from_machine_ir_report_with_profile(report, profile, &mutation_file, error) &&
        machine_mutation_build_report_from_file(&mutation_file, out_report, error);
    machine_mutation_file_free(&mutation_file);
    return ok;
}

int machine_mutation_report_refresh(MachineMutationReport *report,
    MachineMutationError *error) {
    MachineMutationReport refreshed_report;
    int ok;

    if (!report) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-120: invalid report refresh contract");
        return 0;
    }
    machine_mutation_report_init(&refreshed_report);
    ok = machine_mutation_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_mutation_report_free(report);
        *report = refreshed_report;
    } else {
        machine_mutation_report_free(&refreshed_report);
    }
    return ok;
}

int machine_mutation_dump_file(const MachineMutationFile *mutation_file,
    char **out_text,
    MachineMutationError *error) {
    MachineMutationStringBuilder builder;
    MachineMutationHeaderSummary header_summary;
    MachineMutationSummary mutation_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&mutation_summary, 0, sizeof(mutation_summary));
    if (!mutation_file || !out_text ||
        !machine_mutation_verify_file(mutation_file, error) ||
        !machine_mutation_file_get_header_summary(mutation_file, &header_summary) ||
        !machine_mutation_file_get_mutation_summary(mutation_file, &mutation_summary)) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-121: invalid dump contract");
        return 0;
    }

    if (!machine_mutation_append_format(
            &builder,
            "machine_mutation profile=%s elf_origin=%s elf_semantics=%s state=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_elf_target_profile_name(
                mutation_file->state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                mutation_file->state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_state_resolution_kind_name(header_summary.state_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_mutation_append_format(
            &builder,
            "mutation: resolution=%s effect=%s transition=%s action=%s payload-kind=%s tag-class=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu has-state=%s status=%s pc=%s",
            machine_mutation_resolution_kind_name(mutation_summary.resolution_kind),
            machine_mutation_effect_kind_name(mutation_summary.effect_kind),
            machine_transition_resolution_kind_name(mutation_summary.transition_resolution_kind),
            machine_interp_action_kind_name(mutation_summary.action_kind),
            machine_payload_decode_kind_name(mutation_summary.payload_kind),
            machine_decode_tag_class_name(mutation_summary.tag_class),
            (unsigned int)mutation_summary.raw_byte,
            (unsigned int)mutation_summary.tag_value,
            mutation_summary.is_known ? "yes" : "no",
            mutation_summary.tag_name ? mutation_summary.tag_name : "-",
            mutation_summary.instruction_byte_count,
            mutation_summary.has_state ? "yes" : "no",
            mutation_summary.has_state ? machine_step_status_name(mutation_summary.state_status) : "-",
            mutation_summary.has_state ? "" : "-")) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-122: out of memory building dump");
        return 0;
    }
    if (mutation_summary.has_state &&
        !machine_mutation_append_format(&builder, "0x%zx", mutation_summary.program_counter)) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-123: out of memory appending mutation pc");
        return 0;
    }
    if (!machine_mutation_append_format(
            &builder,
            " current-segment=%s",
            mutation_summary.has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-124: out of memory appending current segment");
        return 0;
    }
    if (mutation_summary.has_current_fetch &&
        !machine_mutation_append_format(&builder, "%zu", mutation_summary.current_segment_index)) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-125: out of memory appending current segment value");
        return 0;
    }
    if (!machine_mutation_append_format(
            &builder,
            " current-byte=%s",
            mutation_summary.has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-126: out of memory appending current byte");
        return 0;
    }
    if (mutation_summary.has_current_fetch &&
        !machine_mutation_append_format(&builder, "0x%02x", (unsigned int)mutation_summary.current_byte)) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-127: out of memory appending current byte value");
        return 0;
    }
    if (!machine_mutation_append_targets(&builder, &mutation_summary) ||
        !machine_mutation_append_return_immediate(&builder, &mutation_summary) ||
        !machine_mutation_append_format(&builder, "\n")) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-128: out of memory terminating dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_mutation_build_dump_from_file(const MachineMutationFile *source,
    char **out_text,
    MachineMutationError *error) {
    return machine_mutation_dump_file(source, out_text, error);
}

#define MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineMutationError *error) { \
    MachineMutationFile mutation_file; \
    int ok; \
    machine_mutation_file_init(&mutation_file); \
    ok = builder(source, &mutation_file, error) && \
        machine_mutation_dump_file(&mutation_file, out_text, error); \
    machine_mutation_file_free(&mutation_file); \
    return ok; \
}

MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_state_file,
    MachineStateFile, machine_mutation_build_from_machine_state_file)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_state_report,
    MachineStateReport, machine_mutation_build_from_machine_state_report)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_transition_file,
    MachineTransitionFile, machine_mutation_build_from_machine_transition_file)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_transition_report,
    MachineTransitionReport, machine_mutation_build_from_machine_transition_report)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_interp_file,
    MachineInterpFile, machine_mutation_build_from_machine_interp_file)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_interp_report,
    MachineInterpReport, machine_mutation_build_from_machine_interp_report)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_mutation_build_from_machine_payload_decode_file)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_mutation_build_from_machine_payload_decode_report)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_decode_file,
    MachineDecodeFile, machine_mutation_build_from_machine_decode_file)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_decode_report,
    MachineDecodeReport, machine_mutation_build_from_machine_decode_report)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_step_file,
    MachineStepFile, machine_mutation_build_from_machine_step_file)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_step_report,
    MachineStepReport, machine_mutation_build_from_machine_step_report)
MACHINE_MUTATION_DEFINE_DUMP_FROM_WRAPPER(machine_mutation_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_mutation_build_from_machine_ir_report)

int machine_mutation_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineMutationError *error) {
    MachineMutationFile mutation_file;
    int ok;

    machine_mutation_file_init(&mutation_file);
    ok = machine_mutation_build_from_machine_ir_report_with_profile(report, profile, &mutation_file, error) &&
        machine_mutation_dump_file(&mutation_file, out_text, error);
    machine_mutation_file_free(&mutation_file);
    return ok;
}

int machine_mutation_report_get_summary(const MachineMutationReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_mutation_file_get_source_elf_artifact_summary(const MachineMutationFile *mutation_file,
    MachineElfArtifactSummary *out_summary) {
    if (!mutation_file || !out_summary) {
        return 0;
    }
    *out_summary =
        mutation_file->state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file
            .runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

int machine_mutation_report_get_overview_artifact(const MachineMutationReport *report,
    MachineMutationReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->state_report = &report->state_report;
    out_artifact->mutation_summary = &report->mutation_summary;
    return 1;
}

int machine_mutation_report_get_file(const MachineMutationReport *report,
    const MachineMutationFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_mutation_report_get_state_file(const MachineMutationReport *report,
    const MachineStateFile **out_state_file) {
    if (!report || !out_state_file) {
        return 0;
    }
    *out_state_file = &report->file.state_file;
    return 1;
}

int machine_mutation_report_get_state_report(const MachineMutationReport *report,
    const MachineStateReport **out_state_report) {
    if (!report || !out_state_report) {
        return 0;
    }
    *out_state_report = &report->state_report;
    return 1;
}

int machine_mutation_report_get_source_elf_artifact_summary_artifact(
    const MachineMutationReport *report,
    const MachineElfArtifactSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file
                         .launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

#define MACHINE_MUTATION_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineMutationReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_MUTATION_DEFINE_REPORT_ARTIFACT_GETTER(machine_mutation_report_get_header_summary_artifact,
    header_summary, MachineMutationHeaderSummary)
MACHINE_MUTATION_DEFINE_REPORT_ARTIFACT_GETTER(machine_mutation_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineMutationTargetPolicySummary)
MACHINE_MUTATION_DEFINE_REPORT_ARTIFACT_GETTER(machine_mutation_report_get_mutation_summary_artifact,
    mutation_summary, MachineMutationSummary)

int machine_mutation_report_overview_artifact_get_state_report(
    const MachineMutationReportOverviewArtifact *artifact,
    const MachineStateReport **out_state_report) {
    if (!artifact || !out_state_report) {
        return 0;
    }
    *out_state_report = artifact->state_report;
    return 1;
}

int machine_mutation_report_overview_artifact_get_mutation_summary_artifact(
    const MachineMutationReportOverviewArtifact *artifact,
    const MachineMutationSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->mutation_summary;
    return 1;
}

int machine_mutation_dump_report(const MachineMutationReport *report,
    char **out_text,
    MachineMutationError *error) {
    MachineMutationStringBuilder builder;
    const MachineMutationSummary *mutation_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_mutation_report_get_mutation_summary_artifact(report, &mutation_summary) ||
        !mutation_summary) {
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-129: invalid report dump contract");
        return 0;
    }

    if (!machine_mutation_append_format(
            &builder,
            "machine_mutation profile=%s elf_origin=%s elf_semantics=%s state=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_state_resolution_kind_name(report->header_summary.state_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_mutation_append_format(
            &builder,
            "mutation: resolution=%s effect=%s transition=%s action=%s payload-kind=%s tag-class=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu has-state=%s status=%s pc=%s",
            machine_mutation_resolution_kind_name(mutation_summary->resolution_kind),
            machine_mutation_effect_kind_name(mutation_summary->effect_kind),
            machine_transition_resolution_kind_name(mutation_summary->transition_resolution_kind),
            machine_interp_action_kind_name(mutation_summary->action_kind),
            machine_payload_decode_kind_name(mutation_summary->payload_kind),
            machine_decode_tag_class_name(mutation_summary->tag_class),
            (unsigned int)mutation_summary->raw_byte,
            (unsigned int)mutation_summary->tag_value,
            mutation_summary->is_known ? "yes" : "no",
            mutation_summary->tag_name ? mutation_summary->tag_name : "-",
            mutation_summary->instruction_byte_count,
            mutation_summary->has_state ? "yes" : "no",
            mutation_summary->has_state ? machine_step_status_name(mutation_summary->state_status) : "-",
            mutation_summary->has_state ? "" : "-")) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-130: out of memory building report dump");
        return 0;
    }
    if (mutation_summary->has_state &&
        !machine_mutation_append_format(&builder, "0x%zx", mutation_summary->program_counter)) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-131: out of memory appending report pc");
        return 0;
    }
    if (!machine_mutation_append_format(
            &builder,
            " current-segment=%s",
            mutation_summary->has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-132: out of memory appending report current segment");
        return 0;
    }
    if (mutation_summary->has_current_fetch &&
        !machine_mutation_append_format(&builder, "%zu", mutation_summary->current_segment_index)) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-133: out of memory appending report current segment value");
        return 0;
    }
    if (!machine_mutation_append_format(
            &builder,
            " current-byte=%s",
            mutation_summary->has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-134: out of memory appending report current byte");
        return 0;
    }
    if (mutation_summary->has_current_fetch &&
        !machine_mutation_append_format(&builder, "0x%02x", (unsigned int)mutation_summary->current_byte)) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-135: out of memory appending report current byte value");
        return 0;
    }
    if (!machine_mutation_append_targets(&builder, mutation_summary) ||
        !machine_mutation_append_return_immediate(&builder, mutation_summary) ||
        !machine_mutation_append_format(&builder, "\nreport_overview:\n") ||
        !machine_mutation_append_format(
            &builder,
            "  origin: state=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_state_resolution_kind_name(report->header_summary.state_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer) ||
        !machine_mutation_append_format(
            &builder,
            "  elf_source: target=%s origin=%s semantics=%s\n",
            machine_elf_target_profile_name(
                report->file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics)) ||
        !machine_mutation_append_format(
            &builder,
            "  policy: profile=%s control-only=%s register=%s slot=%s call=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.classifies_control_only ? "yes" : "no",
            report->target_policy_summary.defers_register_result_mutation ? "yes" : "no",
            report->target_policy_summary.defers_slot_mutation ? "yes" : "no",
            report->target_policy_summary.defers_call_effect ? "yes" : "no") ||
        !machine_mutation_append_format(
            &builder,
            "  mutation: resolution=%s effect=%s has-state=%s status=%s pc=%s",
            machine_mutation_resolution_kind_name(mutation_summary->resolution_kind),
            machine_mutation_effect_kind_name(mutation_summary->effect_kind),
            mutation_summary->has_state ? "yes" : "no",
            mutation_summary->has_state ? machine_step_status_name(mutation_summary->state_status) : "-",
            mutation_summary->has_state ? "" : "-")) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-136: out of memory building report overview");
        return 0;
    }
    if (mutation_summary->has_state &&
        !machine_mutation_append_format(&builder, "0x%zx", mutation_summary->program_counter)) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-137: out of memory appending report overview pc");
        return 0;
    }
    if (!machine_mutation_append_targets(&builder, mutation_summary) ||
        !machine_mutation_append_return_immediate(&builder, mutation_summary) ||
        !machine_mutation_append_format(&builder, "\n")) {
        free(builder.data);
        machine_mutation_set_error(error, 0, 0, "MACHINE-MUTATION-138: out of memory terminating report overview");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_mutation_build_report_dump_from_file(const MachineMutationFile *source,
    char **out_text,
    MachineMutationError *error) {
    MachineMutationReport report;
    int ok;

    machine_mutation_report_init(&report);
    ok = machine_mutation_build_report_from_file(source, &report, error) &&
        machine_mutation_dump_report(&report, out_text, error);
    machine_mutation_report_free(&report);
    return ok;
}

#define MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineMutationError *error) { \
    MachineMutationReport report; \
    int ok; \
    machine_mutation_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_mutation_dump_report(&report, out_text, error); \
    machine_mutation_report_free(&report); \
    return ok; \
}

MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_state_file,
    MachineStateFile, machine_mutation_build_report_from_machine_state_file)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_state_report,
    MachineStateReport, machine_mutation_build_report_from_machine_state_report)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_transition_file,
    MachineTransitionFile, machine_mutation_build_report_from_machine_transition_file)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_transition_report,
    MachineTransitionReport, machine_mutation_build_report_from_machine_transition_report)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_interp_file,
    MachineInterpFile, machine_mutation_build_report_from_machine_interp_file)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_interp_report,
    MachineInterpReport, machine_mutation_build_report_from_machine_interp_report)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_mutation_build_report_from_machine_payload_decode_file)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_mutation_build_report_from_machine_payload_decode_report)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_decode_file,
    MachineDecodeFile, machine_mutation_build_report_from_machine_decode_file)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_decode_report,
    MachineDecodeReport, machine_mutation_build_report_from_machine_decode_report)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_mutation_build_report_from_machine_step_file)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_mutation_build_report_from_machine_step_report)
MACHINE_MUTATION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_mutation_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_mutation_build_report_from_machine_ir_report)

int machine_mutation_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineMutationError *error) {
    MachineMutationReport report_file;
    int ok;

    machine_mutation_report_init(&report_file);
    ok = machine_mutation_build_report_from_machine_ir_report_with_profile(report, profile, &report_file, error) &&
        machine_mutation_dump_report(&report_file, out_text, error);
    machine_mutation_report_free(&report_file);
    return ok;
}
