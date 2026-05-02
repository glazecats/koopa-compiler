#include "machine/interp.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineInterpStringBuilder;

static void machine_interp_set_error(MachineInterpError *error, int line, int column, const char *fmt, ...);
static int machine_interp_append_format(MachineInterpStringBuilder *builder, const char *fmt, ...);
static void machine_interp_reset_targets(MachineInterpFile *interp_file);
static int machine_interp_decode_target_pair(unsigned char encoded_pair,
    size_t *out_primary_target,
    size_t *out_secondary_target);
static int machine_interp_append_targets(MachineInterpStringBuilder *builder,
    const MachineInterpSummary *interp_summary);
static int machine_interp_append_return_immediate(MachineInterpStringBuilder *builder,
    const MachineInterpSummary *interp_summary);

static void machine_interp_set_error(MachineInterpError *error, int line, int column, const char *fmt, ...) {
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

static int machine_interp_append_format(MachineInterpStringBuilder *builder, const char *fmt, ...) {
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

static void machine_interp_reset_targets(MachineInterpFile *interp_file) {
    if (!interp_file) {
        return;
    }
    interp_file->has_primary_target_block = 0;
    interp_file->primary_target_block_index = 0u;
    interp_file->has_secondary_target_block = 0;
    interp_file->secondary_target_block_index = 0u;
    interp_file->has_return_immediate = 0;
    interp_file->return_immediate = 0u;
}

static int machine_interp_decode_target_pair(unsigned char encoded_pair,
    size_t *out_primary_target,
    size_t *out_secondary_target) {
    if (!out_primary_target || !out_secondary_target) {
        return 0;
    }
    *out_primary_target = (size_t)((encoded_pair >> 4) & 0x0Fu);
    *out_secondary_target = (size_t)(encoded_pair & 0x0Fu);
    return 1;
}

static int machine_interp_append_targets(MachineInterpStringBuilder *builder,
    const MachineInterpSummary *interp_summary) {
    if (!builder || !interp_summary) {
        return 0;
    }
    if (!interp_summary->has_primary_target_block) {
        return machine_interp_append_format(builder, " targets=[]");
    }
    if (!interp_summary->has_secondary_target_block) {
        return machine_interp_append_format(
            builder,
            " targets=[%zu]",
            interp_summary->primary_target_block_index);
    }
    return machine_interp_append_format(
        builder,
        " targets=[%zu,%zu]",
        interp_summary->primary_target_block_index,
        interp_summary->secondary_target_block_index);
}

static int machine_interp_append_return_immediate(MachineInterpStringBuilder *builder,
    const MachineInterpSummary *interp_summary) {
    if (!builder || !interp_summary) {
        return 0;
    }
    if (!interp_summary->has_return_immediate) {
        return machine_interp_append_format(builder, " return-imm=-");
    }
    return machine_interp_append_format(
        builder,
        " return-imm=%zu",
        interp_summary->return_immediate);
}

void machine_interp_file_init(MachineInterpFile *interp_file) {
    if (!interp_file) {
        return;
    }
    machine_payload_decode_file_init(&interp_file->payload_decode_file);
    interp_file->action_kind = MACHINE_INTERP_ACTION_NONE;
    interp_file->next_status = MACHINE_STEP_STATUS_READY;
    interp_file->has_next_program_counter = 0;
    interp_file->next_program_counter = 0u;
    machine_interp_reset_targets(interp_file);
}

void machine_interp_file_free(MachineInterpFile *interp_file) {
    if (!interp_file) {
        return;
    }
    machine_payload_decode_file_free(&interp_file->payload_decode_file);
    machine_interp_file_init(interp_file);
}

void machine_interp_report_init(MachineInterpReport *report) {
    if (!report) {
        return;
    }
    machine_interp_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    memset(&report->runtime_launch_summary, 0, sizeof(report->runtime_launch_summary));
    memset(&report->initial_stack_summary, 0, sizeof(report->initial_stack_summary));
    memset(&report->runtime_memory_summary, 0, sizeof(report->runtime_memory_summary));
    memset(&report->current_segment_summary, 0, sizeof(report->current_segment_summary));
    memset(&report->fetch_summary, 0, sizeof(report->fetch_summary));
    memset(&report->decode_tag_summary, 0, sizeof(report->decode_tag_summary));
    memset(&report->payload_summary, 0, sizeof(report->payload_summary));
    memset(&report->interp_summary, 0, sizeof(report->interp_summary));
}

void machine_interp_report_free(MachineInterpReport *report) {
    if (!report) {
        return;
    }
    machine_interp_file_free(&report->file);
    machine_interp_report_init(report);
}

const char *machine_interp_action_kind_name(MachineInterpActionKind action_kind) {
    switch (action_kind) {
        case MACHINE_INTERP_ACTION_NONE:
            return "none";
        case MACHINE_INTERP_ACTION_ADVANCE:
            return "advance";
        case MACHINE_INTERP_ACTION_HALT:
            return "halt";
        case MACHINE_INTERP_ACTION_CONTROL_TRANSFER:
            return "control-transfer";
        case MACHINE_INTERP_ACTION_UNSUPPORTED:
            return "unsupported";
    }
    return "unknown";
}

int machine_interp_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineInterpTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->max_control_target_count = 2u;
    out_summary->resolves_linear_next_program_counter = 1;
    out_summary->resolves_control_targets_as_block_indices = 1;
    return 1;
}

int machine_interp_file_get_target_policy_summary(const MachineInterpFile *interp_file,
    MachineInterpTargetPolicySummary *out_summary) {
    if (!interp_file || !out_summary) {
        return 0;
    }
    return machine_interp_get_target_policy_summary(
        interp_file->payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_interp_verify_file(const MachineInterpFile *interp_file,
    MachineInterpError *error) {
    MachineInterpHeaderSummary header_summary;
    MachinePayloadDecodeSummary payload_summary;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&payload_summary, 0, sizeof(payload_summary));
    if (!interp_file) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-100: invalid interp file");
        return 0;
    }
    if (!machine_payload_decode_verify_file(&interp_file->payload_decode_file, NULL) ||
        !machine_interp_file_get_header_summary(interp_file, &header_summary) ||
        !machine_interp_file_get_payload_summary(interp_file, &payload_summary)) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-101: invalid payload-decode input");
        return 0;
    }
    if (interp_file->action_kind == MACHINE_INTERP_ACTION_ADVANCE) {
        if (interp_file->next_status != MACHINE_STEP_STATUS_READY ||
            !interp_file->has_next_program_counter ||
            interp_file->next_program_counter < header_summary.program_counter + payload_summary.total_byte_count) {
            machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-102: invalid advance result");
            return 0;
        }
    }
    if (interp_file->action_kind == MACHINE_INTERP_ACTION_HALT) {
        if (interp_file->next_status != MACHINE_STEP_STATUS_HALTED ||
            interp_file->has_next_program_counter ||
            interp_file->has_primary_target_block ||
            interp_file->has_secondary_target_block) {
            machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-103: invalid halt result");
            return 0;
        }
    }
    if (interp_file->action_kind == MACHINE_INTERP_ACTION_CONTROL_TRANSFER) {
        if (interp_file->next_status != MACHINE_STEP_STATUS_READY ||
            interp_file->has_next_program_counter ||
            !interp_file->has_primary_target_block) {
            machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-104: invalid control-transfer result");
            return 0;
        }
    }
    if ((interp_file->action_kind == MACHINE_INTERP_ACTION_NONE ||
            interp_file->action_kind == MACHINE_INTERP_ACTION_UNSUPPORTED) &&
        interp_file->has_next_program_counter) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-105: unresolved result must not expose next pc");
        return 0;
    }
    return 1;
}

int machine_interp_clone_file(const MachineInterpFile *source,
    MachineInterpFile *out_clone,
    MachineInterpError *error) {
    if (!source || !out_clone) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-106: invalid clone contract");
        return 0;
    }
    if (!machine_interp_verify_file(source, error)) {
        return 0;
    }

    machine_interp_file_free(out_clone);
    if (!machine_payload_decode_clone_file(
            &source->payload_decode_file, &out_clone->payload_decode_file, NULL)) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-107: failed cloning payload decode input");
        machine_interp_file_free(out_clone);
        return 0;
    }
    out_clone->action_kind = source->action_kind;
    out_clone->next_status = source->next_status;
    out_clone->has_next_program_counter = source->has_next_program_counter;
    out_clone->next_program_counter = source->next_program_counter;
    out_clone->has_primary_target_block = source->has_primary_target_block;
    out_clone->primary_target_block_index = source->primary_target_block_index;
    out_clone->has_secondary_target_block = source->has_secondary_target_block;
    out_clone->secondary_target_block_index = source->secondary_target_block_index;
    out_clone->has_return_immediate = source->has_return_immediate;
    out_clone->return_immediate = source->return_immediate;
    return 1;
}

int machine_interp_file_get_summary(const MachineInterpFile *interp_file,
    size_t *out_mapped_byte_count) {
    if (!interp_file) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count =
            interp_file->payload_decode_file.decode_file.step_file.launch_file.runtime_file.total_mapped_byte_count;
    }
    return 1;
}

int machine_interp_file_get_header_summary(const MachineInterpFile *interp_file,
    MachineInterpHeaderSummary *out_summary) {
    if (!interp_file || !out_summary) {
        return 0;
    }
    out_summary->target_profile =
        interp_file->payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile;
    out_summary->step_status = interp_file->payload_decode_file.decode_file.step_file.status;
    out_summary->program_counter = interp_file->payload_decode_file.decode_file.step_file.program_counter;
    out_summary->stack_pointer = interp_file->payload_decode_file.decode_file.step_file.stack_pointer;
    out_summary->current_segment_index = interp_file->payload_decode_file.decode_file.step_file.current_segment_index;
    out_summary->mapped_byte_count =
        interp_file->payload_decode_file.decode_file.step_file.launch_file.runtime_file.total_mapped_byte_count;
    return 1;
}

int machine_interp_file_get_runtime_launch_summary(const MachineInterpFile *interp_file,
    MachineRuntimeLaunchSummary *out_summary) {
    if (!interp_file || !out_summary) {
        return 0;
    }
    return machine_payload_decode_file_get_runtime_launch_summary(&interp_file->payload_decode_file, out_summary);
}

int machine_interp_file_get_initial_stack_summary(const MachineInterpFile *interp_file,
    MachineRuntimeInitialStackSummary *out_summary) {
    if (!interp_file || !out_summary) {
        return 0;
    }
    return machine_payload_decode_file_get_initial_stack_summary(&interp_file->payload_decode_file, out_summary);
}

int machine_interp_file_get_runtime_memory_summary(const MachineInterpFile *interp_file,
    MachineRuntimeMemorySummary *out_summary) {
    if (!interp_file || !out_summary) {
        return 0;
    }
    return machine_payload_decode_file_get_runtime_memory_summary(&interp_file->payload_decode_file, out_summary);
}

int machine_interp_file_get_current_segment_summary(const MachineInterpFile *interp_file,
    MachineRuntimeSegmentSummary *out_summary) {
    if (!interp_file || !out_summary) {
        return 0;
    }
    return machine_payload_decode_file_get_current_segment_summary(&interp_file->payload_decode_file, out_summary);
}

int machine_interp_file_get_fetch_summary(const MachineInterpFile *interp_file,
    MachineStepFetchSummary *out_summary) {
    if (!interp_file || !out_summary) {
        return 0;
    }
    return machine_payload_decode_file_get_fetch_summary(&interp_file->payload_decode_file, out_summary);
}

int machine_interp_file_get_decode_tag_summary(const MachineInterpFile *interp_file,
    MachineDecodeTagSummary *out_summary) {
    if (!interp_file || !out_summary) {
        return 0;
    }
    return machine_payload_decode_file_get_decode_tag_summary(&interp_file->payload_decode_file, out_summary);
}

int machine_interp_file_get_payload_summary(const MachineInterpFile *interp_file,
    MachinePayloadDecodeSummary *out_summary) {
    if (!interp_file || !out_summary) {
        return 0;
    }
    return machine_payload_decode_file_get_payload_summary(&interp_file->payload_decode_file, out_summary);
}

int machine_interp_file_get_interp_summary(const MachineInterpFile *interp_file,
    MachineInterpSummary *out_summary) {
    MachinePayloadDecodeSummary payload_summary;

    memset(&payload_summary, 0, sizeof(payload_summary));
    if (!interp_file || !out_summary ||
        !machine_interp_file_get_payload_summary(interp_file, &payload_summary)) {
        return 0;
    }
    out_summary->action_kind = interp_file->action_kind;
    out_summary->raw_byte = payload_summary.raw_byte;
    out_summary->tag_value = payload_summary.tag_value;
    out_summary->is_known = payload_summary.is_known;
    out_summary->tag_name = payload_summary.tag_name;
    out_summary->instruction_byte_count = payload_summary.total_byte_count;
    out_summary->next_status = interp_file->next_status;
    out_summary->has_next_program_counter = interp_file->has_next_program_counter;
    out_summary->next_program_counter = interp_file->next_program_counter;
    out_summary->has_primary_target_block = interp_file->has_primary_target_block;
    out_summary->primary_target_block_index = interp_file->primary_target_block_index;
    out_summary->has_secondary_target_block = interp_file->has_secondary_target_block;
    out_summary->secondary_target_block_index = interp_file->secondary_target_block_index;
    out_summary->has_return_immediate = interp_file->has_return_immediate;
    out_summary->return_immediate = interp_file->return_immediate;
    return 1;
}

int machine_interp_build_from_machine_payload_decode_file(const MachinePayloadDecodeFile *payload_decode_file,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error) {
    MachinePayloadDecodeHeaderSummary header_summary;
    MachinePayloadDecodeSummary payload_summary;
    MachineDecodeTagSummary decode_tag_summary;
    size_t next_program_counter;
    size_t primary_target;
    size_t secondary_target;

    memset(&header_summary, 0, sizeof(header_summary));
    memset(&payload_summary, 0, sizeof(payload_summary));
    memset(&decode_tag_summary, 0, sizeof(decode_tag_summary));
    next_program_counter = 0u;
    primary_target = 0u;
    secondary_target = 0u;
    if (!payload_decode_file || !out_interp_file) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-108: invalid build contract");
        return 0;
    }
    if (!machine_payload_decode_verify_file(payload_decode_file, NULL) ||
        !machine_payload_decode_file_get_header_summary(payload_decode_file, &header_summary) ||
        !machine_payload_decode_file_get_payload_summary(payload_decode_file, &payload_summary) ||
        !machine_payload_decode_file_get_decode_tag_summary(payload_decode_file, &decode_tag_summary)) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-109: invalid payload decode input");
        return 0;
    }

    machine_interp_file_free(out_interp_file);
    if (!machine_payload_decode_clone_file(
            payload_decode_file, &out_interp_file->payload_decode_file, NULL)) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-110: failed cloning payload decode input");
        machine_interp_file_free(out_interp_file);
        return 0;
    }

    out_interp_file->action_kind = MACHINE_INTERP_ACTION_UNSUPPORTED;
    out_interp_file->next_status = header_summary.step_status;
    out_interp_file->has_next_program_counter = 0;
    out_interp_file->next_program_counter = 0u;
    machine_interp_reset_targets(out_interp_file);

    if (header_summary.step_status != MACHINE_STEP_STATUS_READY) {
        if (!machine_interp_verify_file(out_interp_file, error)) {
            machine_interp_file_free(out_interp_file);
            return 0;
        }
        return 1;
    }

    if (payload_summary.kind == MACHINE_PAYLOAD_DECODE_KIND_OP) {
        if (header_summary.program_counter > (size_t)-1 - payload_summary.total_byte_count) {
            machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-111: pc advance overflow");
            machine_interp_file_free(out_interp_file);
            return 0;
        }
        next_program_counter = header_summary.program_counter + payload_summary.total_byte_count;
        out_interp_file->action_kind = MACHINE_INTERP_ACTION_ADVANCE;
        out_interp_file->next_status = MACHINE_STEP_STATUS_READY;
        out_interp_file->has_next_program_counter = 1;
        out_interp_file->next_program_counter = next_program_counter;
    } else if (payload_summary.kind == MACHINE_PAYLOAD_DECODE_KIND_TERMINATOR) {
        switch ((MachineLayoutTerminatorKind)decode_tag_summary.tag_value) {
            case MACHINE_LAYOUT_TERM_RETURN:
                out_interp_file->action_kind = MACHINE_INTERP_ACTION_HALT;
                out_interp_file->next_status = MACHINE_STEP_STATUS_HALTED;
                break;
            case MACHINE_LAYOUT_TERM_RETURN_IMM:
                out_interp_file->action_kind = MACHINE_INTERP_ACTION_HALT;
                out_interp_file->next_status = MACHINE_STEP_STATUS_HALTED;
                if (payload_summary.payload_byte_count == 1u &&
                    (payload_summary.payload_bytes[0] & 0xF0u) == 0x10u) {
                    out_interp_file->has_return_immediate = 1;
                    out_interp_file->return_immediate = (size_t)(payload_summary.payload_bytes[0] & 0x0Fu);
                }
                break;
            case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                out_interp_file->action_kind = MACHINE_INTERP_ACTION_HALT;
                out_interp_file->next_status = MACHINE_STEP_STATUS_HALTED;
                break;
            case MACHINE_LAYOUT_TERM_JUMP:
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                if (payload_summary.payload_byte_count == 1u) {
                    out_interp_file->action_kind = MACHINE_INTERP_ACTION_CONTROL_TRANSFER;
                    out_interp_file->next_status = MACHINE_STEP_STATUS_READY;
                    out_interp_file->has_primary_target_block = 1;
                    out_interp_file->primary_target_block_index = (size_t)payload_summary.payload_bytes[0];
                }
                break;
            case MACHINE_LAYOUT_TERM_BRANCH:
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                if (payload_summary.payload_byte_count == 2u &&
                    machine_interp_decode_target_pair(
                        payload_summary.payload_bytes[1], &primary_target, &secondary_target)) {
                    out_interp_file->action_kind = MACHINE_INTERP_ACTION_CONTROL_TRANSFER;
                    out_interp_file->next_status = MACHINE_STEP_STATUS_READY;
                    out_interp_file->has_primary_target_block = 1;
                    out_interp_file->primary_target_block_index = primary_target;
                    out_interp_file->has_secondary_target_block = 1;
                    out_interp_file->secondary_target_block_index = secondary_target;
                }
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                if (payload_summary.payload_byte_count == 3u &&
                    machine_interp_decode_target_pair(
                        payload_summary.payload_bytes[2], &primary_target, &secondary_target)) {
                    out_interp_file->action_kind = MACHINE_INTERP_ACTION_CONTROL_TRANSFER;
                    out_interp_file->next_status = MACHINE_STEP_STATUS_READY;
                    out_interp_file->has_primary_target_block = 1;
                    out_interp_file->primary_target_block_index = primary_target;
                    out_interp_file->has_secondary_target_block = 1;
                    out_interp_file->secondary_target_block_index = secondary_target;
                }
                break;
            default:
                break;
        }
    }

    if (!machine_interp_verify_file(out_interp_file, error)) {
        machine_interp_file_free(out_interp_file);
        return 0;
    }
    return 1;
}

int machine_interp_build_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error) {
    const MachinePayloadDecodeFile *payload_decode_file = NULL;

    if (!report) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-112: invalid payload-report build contract");
        return 0;
    }
    if (!machine_payload_decode_report_get_file(report, &payload_decode_file) || !payload_decode_file) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-113: failed recovering payload decode file from report");
        return 0;
    }
    return machine_interp_build_from_machine_payload_decode_file(payload_decode_file, out_interp_file, error);
}

int machine_interp_build_from_machine_decode_file(const MachineDecodeFile *decode_file,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_decode_file(decode_file, &payload_decode_file, NULL) &&
        machine_interp_build_from_machine_payload_decode_file(&payload_decode_file, out_interp_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-114: failed building from decode file");
    }
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_interp_build_from_machine_decode_report(const MachineDecodeReport *report,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_decode_report(report, &payload_decode_file, NULL) &&
        machine_interp_build_from_machine_payload_decode_file(&payload_decode_file, out_interp_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-115: failed building from decode report");
    }
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_interp_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_step_file(step_file, &payload_decode_file, NULL) &&
        machine_interp_build_from_machine_payload_decode_file(&payload_decode_file, out_interp_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-116: failed building from step file");
    }
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_interp_build_from_machine_step_report(const MachineStepReport *report,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_step_report(report, &payload_decode_file, NULL) &&
        machine_interp_build_from_machine_payload_decode_file(&payload_decode_file, out_interp_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-117: failed building from step report");
    }
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_interp_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_ir_report(report, &payload_decode_file, NULL) &&
        machine_interp_build_from_machine_payload_decode_file(&payload_decode_file, out_interp_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-118: failed building from machine-ir report");
    }
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_interp_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_ir_report_with_profile(
            report, profile, &payload_decode_file, NULL) &&
        machine_interp_build_from_machine_payload_decode_file(&payload_decode_file, out_interp_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-119: failed building profiled machine-ir interp");
    }
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_interp_build_report_from_file(const MachineInterpFile *source,
    MachineInterpReport *out_report,
    MachineInterpError *error) {
    if (!source || !out_report) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-120: invalid report build contract");
        return 0;
    }
    if (!machine_interp_verify_file(source, error)) {
        return 0;
    }

    machine_interp_report_free(out_report);
    if (!machine_interp_clone_file(source, &out_report->file, error)) {
        machine_interp_report_free(out_report);
        return 0;
    }
    if (!machine_interp_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_interp_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_interp_file_get_runtime_launch_summary(&out_report->file, &out_report->runtime_launch_summary) ||
        !machine_interp_file_get_initial_stack_summary(&out_report->file, &out_report->initial_stack_summary) ||
        !machine_interp_file_get_runtime_memory_summary(&out_report->file, &out_report->runtime_memory_summary) ||
        !machine_interp_file_get_current_segment_summary(&out_report->file, &out_report->current_segment_summary) ||
        !machine_interp_file_get_fetch_summary(&out_report->file, &out_report->fetch_summary) ||
        !machine_interp_file_get_decode_tag_summary(&out_report->file, &out_report->decode_tag_summary) ||
        !machine_interp_file_get_payload_summary(&out_report->file, &out_report->payload_summary) ||
        !machine_interp_file_get_interp_summary(&out_report->file, &out_report->interp_summary)) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-121: failed summarizing interp report");
        machine_interp_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_INTERP_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineInterpReport *out_report, MachineInterpError *error) { \
    MachineInterpFile interp_file; \
    int ok; \
    machine_interp_file_init(&interp_file); \
    ok = builder(source, &interp_file, error) && \
        machine_interp_build_report_from_file(&interp_file, out_report, error); \
    machine_interp_file_free(&interp_file); \
    return ok; \
}

MACHINE_INTERP_DEFINE_REPORT_FROM_WRAPPER(machine_interp_build_report_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_interp_build_from_machine_payload_decode_file)
MACHINE_INTERP_DEFINE_REPORT_FROM_WRAPPER(machine_interp_build_report_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_interp_build_from_machine_payload_decode_report)
MACHINE_INTERP_DEFINE_REPORT_FROM_WRAPPER(machine_interp_build_report_from_machine_decode_file,
    MachineDecodeFile, machine_interp_build_from_machine_decode_file)
MACHINE_INTERP_DEFINE_REPORT_FROM_WRAPPER(machine_interp_build_report_from_machine_decode_report,
    MachineDecodeReport, machine_interp_build_from_machine_decode_report)
MACHINE_INTERP_DEFINE_REPORT_FROM_WRAPPER(machine_interp_build_report_from_machine_step_file,
    MachineStepFile, machine_interp_build_from_machine_step_file)
MACHINE_INTERP_DEFINE_REPORT_FROM_WRAPPER(machine_interp_build_report_from_machine_step_report,
    MachineStepReport, machine_interp_build_from_machine_step_report)
MACHINE_INTERP_DEFINE_REPORT_FROM_WRAPPER(machine_interp_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_interp_build_from_machine_ir_report)

int machine_interp_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineInterpReport *out_report,
    MachineInterpError *error) {
    MachineInterpFile interp_file;
    int ok;

    machine_interp_file_init(&interp_file);
    ok = machine_interp_build_from_machine_ir_report_with_profile(report, profile, &interp_file, error) &&
        machine_interp_build_report_from_file(&interp_file, out_report, error);
    machine_interp_file_free(&interp_file);
    return ok;
}

int machine_interp_report_refresh(MachineInterpReport *report,
    MachineInterpError *error) {
    MachineInterpReport refreshed_report;
    int ok;

    if (!report) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-122: invalid report refresh contract");
        return 0;
    }
    machine_interp_report_init(&refreshed_report);
    ok = machine_interp_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_interp_report_free(report);
        *report = refreshed_report;
    } else {
        machine_interp_report_free(&refreshed_report);
    }
    return ok;
}

int machine_interp_dump_file(const MachineInterpFile *interp_file,
    char **out_text,
    MachineInterpError *error) {
    MachineInterpStringBuilder builder;
    MachineInterpHeaderSummary header_summary;
    MachineInterpSummary interp_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&interp_summary, 0, sizeof(interp_summary));
    if (!interp_file || !out_text ||
        !machine_interp_verify_file(interp_file, error) ||
        !machine_interp_file_get_header_summary(interp_file, &header_summary) ||
        !machine_interp_file_get_interp_summary(interp_file, &interp_summary)) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-123: invalid dump contract");
        return 0;
    }

    if (!machine_interp_append_format(
            &builder,
            "machine_interp profile=%s status=%s pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_step_status_name(header_summary.step_status),
            header_summary.program_counter,
            header_summary.stack_pointer,
            header_summary.current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_interp_append_format(
            &builder,
            "interp: action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu next-status=%s",
            machine_interp_action_kind_name(interp_summary.action_kind),
            (unsigned int)interp_summary.raw_byte,
            (unsigned int)interp_summary.tag_value,
            interp_summary.is_known ? "yes" : "no",
            interp_summary.tag_name ? interp_summary.tag_name : "-",
            interp_summary.instruction_byte_count,
            machine_step_status_name(interp_summary.next_status))) {
        free(builder.data);
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-124: out of memory building dump");
        return 0;
    }
    if (!machine_interp_append_format(
            &builder,
            " next-pc=%s",
            interp_summary.has_next_program_counter ? "" : "-")) {
        free(builder.data);
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-125: out of memory appending next-pc");
        return 0;
    }
    if (interp_summary.has_next_program_counter &&
        !machine_interp_append_format(&builder, "0x%zx", interp_summary.next_program_counter)) {
        free(builder.data);
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-126: out of memory appending next pc value");
        return 0;
    }
    if (!machine_interp_append_targets(&builder, &interp_summary) ||
        !machine_interp_append_return_immediate(&builder, &interp_summary)) {
        free(builder.data);
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-127: out of memory appending targets");
        return 0;
    }
    if (!machine_interp_append_format(&builder, "\n")) {
        free(builder.data);
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-129: out of memory terminating dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

#define MACHINE_INTERP_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineInterpError *error) { \
    MachineInterpFile interp_file; \
    int ok; \
    machine_interp_file_init(&interp_file); \
    ok = builder(source, &interp_file, error) && \
        machine_interp_dump_file(&interp_file, out_text, error); \
    machine_interp_file_free(&interp_file); \
    return ok; \
}

int machine_interp_build_dump_from_file(const MachineInterpFile *source,
    char **out_text,
    MachineInterpError *error) {
    return machine_interp_dump_file(source, out_text, error);
}

MACHINE_INTERP_DEFINE_DUMP_FROM_WRAPPER(machine_interp_build_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_interp_build_from_machine_payload_decode_file)
MACHINE_INTERP_DEFINE_DUMP_FROM_WRAPPER(machine_interp_build_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_interp_build_from_machine_payload_decode_report)
MACHINE_INTERP_DEFINE_DUMP_FROM_WRAPPER(machine_interp_build_dump_from_machine_decode_file,
    MachineDecodeFile, machine_interp_build_from_machine_decode_file)
MACHINE_INTERP_DEFINE_DUMP_FROM_WRAPPER(machine_interp_build_dump_from_machine_decode_report,
    MachineDecodeReport, machine_interp_build_from_machine_decode_report)
MACHINE_INTERP_DEFINE_DUMP_FROM_WRAPPER(machine_interp_build_dump_from_machine_step_file,
    MachineStepFile, machine_interp_build_from_machine_step_file)
MACHINE_INTERP_DEFINE_DUMP_FROM_WRAPPER(machine_interp_build_dump_from_machine_step_report,
    MachineStepReport, machine_interp_build_from_machine_step_report)
MACHINE_INTERP_DEFINE_DUMP_FROM_WRAPPER(machine_interp_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_interp_build_from_machine_ir_report)

int machine_interp_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineInterpError *error) {
    MachineInterpFile interp_file;
    int ok;

    machine_interp_file_init(&interp_file);
    ok = machine_interp_build_from_machine_ir_report_with_profile(report, profile, &interp_file, error) &&
        machine_interp_dump_file(&interp_file, out_text, error);
    machine_interp_file_free(&interp_file);
    return ok;
}

int machine_interp_report_get_summary(const MachineInterpReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_interp_report_get_overview_artifact(const MachineInterpReport *report,
    MachineInterpReportOverviewArtifact *out_artifact) {
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
    out_artifact->decode_tag_summary = &report->decode_tag_summary;
    out_artifact->payload_summary = &report->payload_summary;
    out_artifact->interp_summary = &report->interp_summary;
    return 1;
}

int machine_interp_report_get_file(const MachineInterpReport *report,
    const MachineInterpFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_interp_report_get_payload_decode_file(const MachineInterpReport *report,
    const MachinePayloadDecodeFile **out_payload_decode_file) {
    if (!report || !out_payload_decode_file) {
        return 0;
    }
    *out_payload_decode_file = &report->file.payload_decode_file;
    return 1;
}

#define MACHINE_INTERP_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineInterpReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_INTERP_DEFINE_REPORT_ARTIFACT_GETTER(machine_interp_report_get_header_summary_artifact,
    header_summary, MachineInterpHeaderSummary)
MACHINE_INTERP_DEFINE_REPORT_ARTIFACT_GETTER(machine_interp_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineInterpTargetPolicySummary)
MACHINE_INTERP_DEFINE_REPORT_ARTIFACT_GETTER(machine_interp_report_get_runtime_launch_summary_artifact,
    runtime_launch_summary, MachineRuntimeLaunchSummary)
MACHINE_INTERP_DEFINE_REPORT_ARTIFACT_GETTER(machine_interp_report_get_initial_stack_summary_artifact,
    initial_stack_summary, MachineRuntimeInitialStackSummary)
MACHINE_INTERP_DEFINE_REPORT_ARTIFACT_GETTER(machine_interp_report_get_runtime_memory_summary_artifact,
    runtime_memory_summary, MachineRuntimeMemorySummary)
MACHINE_INTERP_DEFINE_REPORT_ARTIFACT_GETTER(machine_interp_report_get_current_segment_summary_artifact,
    current_segment_summary, MachineRuntimeSegmentSummary)
MACHINE_INTERP_DEFINE_REPORT_ARTIFACT_GETTER(machine_interp_report_get_fetch_summary_artifact,
    fetch_summary, MachineStepFetchSummary)
MACHINE_INTERP_DEFINE_REPORT_ARTIFACT_GETTER(machine_interp_report_get_decode_tag_summary_artifact,
    decode_tag_summary, MachineDecodeTagSummary)
MACHINE_INTERP_DEFINE_REPORT_ARTIFACT_GETTER(machine_interp_report_get_payload_summary_artifact,
    payload_summary, MachinePayloadDecodeSummary)
MACHINE_INTERP_DEFINE_REPORT_ARTIFACT_GETTER(machine_interp_report_get_interp_summary_artifact,
    interp_summary, MachineInterpSummary)

#define MACHINE_INTERP_DEFINE_OVERVIEW_GETTER(name, field, type) \
int name(const MachineInterpReportOverviewArtifact *artifact, const type **out_summary) { \
    if (!artifact || !out_summary) { \
        return 0; \
    } \
    *out_summary = artifact->field; \
    return 1; \
}

MACHINE_INTERP_DEFINE_OVERVIEW_GETTER(machine_interp_report_overview_artifact_get_runtime_launch_summary_artifact,
    runtime_launch_summary, MachineRuntimeLaunchSummary)
MACHINE_INTERP_DEFINE_OVERVIEW_GETTER(machine_interp_report_overview_artifact_get_initial_stack_summary_artifact,
    initial_stack_summary, MachineRuntimeInitialStackSummary)
MACHINE_INTERP_DEFINE_OVERVIEW_GETTER(machine_interp_report_overview_artifact_get_runtime_memory_summary_artifact,
    runtime_memory_summary, MachineRuntimeMemorySummary)
MACHINE_INTERP_DEFINE_OVERVIEW_GETTER(machine_interp_report_overview_artifact_get_current_segment_summary_artifact,
    current_segment_summary, MachineRuntimeSegmentSummary)
MACHINE_INTERP_DEFINE_OVERVIEW_GETTER(machine_interp_report_overview_artifact_get_fetch_summary_artifact,
    fetch_summary, MachineStepFetchSummary)
MACHINE_INTERP_DEFINE_OVERVIEW_GETTER(machine_interp_report_overview_artifact_get_decode_tag_summary_artifact,
    decode_tag_summary, MachineDecodeTagSummary)
MACHINE_INTERP_DEFINE_OVERVIEW_GETTER(machine_interp_report_overview_artifact_get_payload_summary_artifact,
    payload_summary, MachinePayloadDecodeSummary)
MACHINE_INTERP_DEFINE_OVERVIEW_GETTER(machine_interp_report_overview_artifact_get_interp_summary_artifact,
    interp_summary, MachineInterpSummary)

int machine_interp_dump_report(const MachineInterpReport *report,
    char **out_text,
    MachineInterpError *error) {
    MachineInterpStringBuilder builder;
    const MachineInterpSummary *interp_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_interp_report_get_interp_summary_artifact(report, &interp_summary) ||
        !interp_summary) {
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-130: invalid report dump contract");
        return 0;
    }
    if (!machine_interp_append_format(
            &builder,
            "machine_interp profile=%s status=%s pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_step_status_name(report->header_summary.step_status),
            report->header_summary.program_counter,
            report->header_summary.stack_pointer,
            report->header_summary.current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_interp_append_format(
            &builder,
            "interp: action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu next-status=%s next-pc=%s",
            machine_interp_action_kind_name(interp_summary->action_kind),
            (unsigned int)interp_summary->raw_byte,
            (unsigned int)interp_summary->tag_value,
            interp_summary->is_known ? "yes" : "no",
            interp_summary->tag_name ? interp_summary->tag_name : "-",
            interp_summary->instruction_byte_count,
            machine_step_status_name(interp_summary->next_status),
            interp_summary->has_next_program_counter ? "" : "-")) {
        free(builder.data);
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-131: out of memory building report dump");
        return 0;
    }
    if (interp_summary->has_next_program_counter &&
        !machine_interp_append_format(&builder, "0x%zx", interp_summary->next_program_counter)) {
        free(builder.data);
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-132: out of memory appending report next pc");
        return 0;
    }
    if (!machine_interp_append_targets(&builder, interp_summary) ||
        !machine_interp_append_return_immediate(&builder, interp_summary)) {
        free(builder.data);
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-133: out of memory appending report targets");
        return 0;
    }
    if (!machine_interp_append_format(&builder, "\nreport_overview:\n") ||
        !machine_interp_append_format(
            &builder,
            "  status=%s current-segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_step_status_name(report->header_summary.step_status),
            report->header_summary.current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.program_counter,
            report->header_summary.stack_pointer) ||
        !machine_interp_append_format(
            &builder,
            "  policy: profile=%s targets=%zu linear-next-pc=%s block-targets=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.max_control_target_count,
            report->target_policy_summary.resolves_linear_next_program_counter ? "yes" : "no",
            report->target_policy_summary.resolves_control_targets_as_block_indices ? "yes" : "no") ||
        !machine_interp_append_format(
            &builder,
            "  interp: action=%s next-status=%s next-pc=%s",
            machine_interp_action_kind_name(interp_summary->action_kind),
            machine_step_status_name(interp_summary->next_status),
            interp_summary->has_next_program_counter ? "" : "-")) {
        free(builder.data);
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-135: out of memory building report overview");
        return 0;
    }
    if (interp_summary->has_next_program_counter &&
        !machine_interp_append_format(&builder, "0x%zx", interp_summary->next_program_counter)) {
        free(builder.data);
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-136: out of memory appending report overview next pc");
        return 0;
    }
    if (!machine_interp_append_targets(&builder, interp_summary) ||
        !machine_interp_append_return_immediate(&builder, interp_summary) ||
        !machine_interp_append_format(&builder, "\n")) {
        free(builder.data);
        machine_interp_set_error(error, 0, 0, "MACHINE-INTERP-137: out of memory terminating report overview");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_interp_build_report_dump_from_file(const MachineInterpFile *source,
    char **out_text,
    MachineInterpError *error) {
    MachineInterpReport report;
    int ok;

    machine_interp_report_init(&report);
    ok = machine_interp_build_report_from_file(source, &report, error) &&
        machine_interp_dump_report(&report, out_text, error);
    machine_interp_report_free(&report);
    return ok;
}

#define MACHINE_INTERP_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineInterpError *error) { \
    MachineInterpReport report; \
    int ok; \
    machine_interp_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_interp_dump_report(&report, out_text, error); \
    machine_interp_report_free(&report); \
    return ok; \
}

MACHINE_INTERP_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_interp_build_report_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_interp_build_report_from_machine_payload_decode_file)
MACHINE_INTERP_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_interp_build_report_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_interp_build_report_from_machine_payload_decode_report)
MACHINE_INTERP_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_interp_build_report_dump_from_machine_decode_file,
    MachineDecodeFile, machine_interp_build_report_from_machine_decode_file)
MACHINE_INTERP_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_interp_build_report_dump_from_machine_decode_report,
    MachineDecodeReport, machine_interp_build_report_from_machine_decode_report)
MACHINE_INTERP_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_interp_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_interp_build_report_from_machine_step_file)
MACHINE_INTERP_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_interp_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_interp_build_report_from_machine_step_report)
MACHINE_INTERP_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_interp_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_interp_build_report_from_machine_ir_report)

int machine_interp_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineInterpError *error) {
    MachineInterpReport interp_report;
    int ok;

    machine_interp_report_init(&interp_report);
    ok = machine_interp_build_report_from_machine_ir_report_with_profile(
            report, profile, &interp_report, error) &&
        machine_interp_dump_report(&interp_report, out_text, error);
    machine_interp_report_free(&interp_report);
    return ok;
}
