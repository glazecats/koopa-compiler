#include "machine/transition.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineTransitionStringBuilder;

static void machine_transition_set_error(MachineTransitionError *error, int line, int column, const char *fmt, ...);
static int machine_transition_append_format(MachineTransitionStringBuilder *builder, const char *fmt, ...);
static int machine_transition_append_targets(MachineTransitionStringBuilder *builder,
    const MachineTransitionSummary *transition_summary);
static int machine_transition_append_return_immediate(MachineTransitionStringBuilder *builder,
    const MachineTransitionSummary *transition_summary);

static void machine_transition_set_error(MachineTransitionError *error, int line, int column, const char *fmt, ...) {
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

static int machine_transition_append_format(MachineTransitionStringBuilder *builder, const char *fmt, ...) {
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

static int machine_transition_append_targets(MachineTransitionStringBuilder *builder,
    const MachineTransitionSummary *transition_summary) {
    if (!builder || !transition_summary) {
        return 0;
    }
    if (!transition_summary->has_primary_target_block) {
        return machine_transition_append_format(builder, " targets=[]");
    }
    if (!transition_summary->has_secondary_target_block) {
        return machine_transition_append_format(
            builder,
            " targets=[%zu]",
            transition_summary->primary_target_block_index);
    }
    return machine_transition_append_format(
        builder,
        " targets=[%zu,%zu]",
        transition_summary->primary_target_block_index,
        transition_summary->secondary_target_block_index);
}

static int machine_transition_append_return_immediate(MachineTransitionStringBuilder *builder,
    const MachineTransitionSummary *transition_summary) {
    if (!builder || !transition_summary) {
        return 0;
    }
    if (!transition_summary->has_return_immediate) {
        return machine_transition_append_format(builder, " return-imm=-");
    }
    return machine_transition_append_format(
        builder,
        " return-imm=%zu",
        transition_summary->return_immediate);
}

void machine_transition_file_init(MachineTransitionFile *transition_file) {
    if (!transition_file) {
        return;
    }
    machine_interp_file_init(&transition_file->interp_file);
    transition_file->resolution_kind = MACHINE_TRANSITION_RESOLUTION_NONE;
    transition_file->next_status = MACHINE_STEP_STATUS_READY;
    transition_file->has_next_fetch = 0;
    transition_file->next_program_counter = 0u;
    transition_file->next_current_segment_index = 0u;
    transition_file->next_current_byte = 0u;
    transition_file->next_current_byte_memory_offset = 0u;
}

void machine_transition_file_free(MachineTransitionFile *transition_file) {
    if (!transition_file) {
        return;
    }
    machine_interp_file_free(&transition_file->interp_file);
    machine_transition_file_init(transition_file);
}

void machine_transition_report_init(MachineTransitionReport *report) {
    if (!report) {
        return;
    }
    machine_transition_file_init(&report->file);
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
    memset(&report->transition_summary, 0, sizeof(report->transition_summary));
    memset(&report->next_fetch_summary, 0, sizeof(report->next_fetch_summary));
}

void machine_transition_report_free(MachineTransitionReport *report) {
    if (!report) {
        return;
    }
    machine_transition_file_free(&report->file);
    machine_transition_report_init(report);
}

const char *machine_transition_resolution_kind_name(MachineTransitionResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_TRANSITION_RESOLUTION_NONE:
            return "none";
        case MACHINE_TRANSITION_RESOLUTION_NEXT_FETCH:
            return "next-fetch";
        case MACHINE_TRANSITION_RESOLUTION_HALT:
            return "halt";
        case MACHINE_TRANSITION_RESOLUTION_DEFERRED_CONTROL_TRANSFER:
            return "deferred-control-transfer";
        case MACHINE_TRANSITION_RESOLUTION_UNSUPPORTED:
            return "unsupported";
    }
    return "unknown";
}

int machine_transition_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineTransitionTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->resolves_linear_next_fetch = 1;
    out_summary->resolves_halt_transition = 1;
    out_summary->defers_control_transfer_targets = 1;
    return 1;
}

int machine_transition_file_get_target_policy_summary(const MachineTransitionFile *transition_file,
    MachineTransitionTargetPolicySummary *out_summary) {
    if (!transition_file || !out_summary) {
        return 0;
    }
    return machine_transition_get_target_policy_summary(
        transition_file->interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_transition_verify_file(const MachineTransitionFile *transition_file,
    MachineTransitionError *error) {
    MachineInterpSummary interp_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    const MachineRuntimeSegment *segment = NULL;
    unsigned char next_byte = 0u;

    memset(&interp_summary, 0, sizeof(interp_summary));
    memset(&runtime_memory_summary, 0, sizeof(runtime_memory_summary));
    if (!transition_file) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-100: invalid transition file");
        return 0;
    }
    if (!machine_interp_verify_file(&transition_file->interp_file, NULL) ||
        !machine_transition_file_get_interp_summary(transition_file, &interp_summary)) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-101: invalid interp input");
        return 0;
    }
    switch (transition_file->resolution_kind) {
        case MACHINE_TRANSITION_RESOLUTION_NEXT_FETCH:
            if (interp_summary.action_kind != MACHINE_INTERP_ACTION_ADVANCE ||
                transition_file->next_status != MACHINE_STEP_STATUS_READY ||
                !transition_file->has_next_fetch ||
                !interp_summary.has_next_program_counter ||
                transition_file->next_program_counter != interp_summary.next_program_counter ||
                !machine_runtime_file_get_segment(
                    &transition_file->interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file,
                    transition_file->next_current_segment_index,
                    &segment) ||
                !segment || !segment->executable ||
                !machine_transition_file_get_runtime_memory_summary(transition_file, &runtime_memory_summary) ||
                transition_file->next_program_counter < runtime_memory_summary.base_virtual_address ||
                transition_file->next_current_byte_memory_offset !=
                    transition_file->next_program_counter - runtime_memory_summary.base_virtual_address ||
                !machine_runtime_file_get_memory_byte_at_virtual_address(
                    &transition_file->interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file,
                    transition_file->next_program_counter,
                    &next_byte) ||
                next_byte != transition_file->next_current_byte) {
                machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-102: invalid next-fetch transition");
                return 0;
            }
            if (!machine_transition_file_get_next_fetch_summary(transition_file, NULL)) {
                machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-103: failed deriving next fetch summary");
                return 0;
            }
            break;
        case MACHINE_TRANSITION_RESOLUTION_HALT:
            if (interp_summary.action_kind != MACHINE_INTERP_ACTION_HALT ||
                transition_file->next_status != MACHINE_STEP_STATUS_HALTED ||
                transition_file->has_next_fetch) {
                machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-104: invalid halt transition");
                return 0;
            }
            break;
        case MACHINE_TRANSITION_RESOLUTION_DEFERRED_CONTROL_TRANSFER:
            if (interp_summary.action_kind != MACHINE_INTERP_ACTION_CONTROL_TRANSFER ||
                transition_file->next_status != MACHINE_STEP_STATUS_READY ||
                transition_file->has_next_fetch ||
                !interp_summary.has_primary_target_block) {
                machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-105: invalid deferred control transition");
                return 0;
            }
            break;
        case MACHINE_TRANSITION_RESOLUTION_UNSUPPORTED:
            if ((interp_summary.action_kind != MACHINE_INTERP_ACTION_UNSUPPORTED &&
                    interp_summary.action_kind != MACHINE_INTERP_ACTION_NONE) ||
                transition_file->has_next_fetch) {
                machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-106: invalid unsupported transition");
                return 0;
            }
            break;
        case MACHINE_TRANSITION_RESOLUTION_NONE:
        default:
            machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-107: invalid transition resolution kind");
            return 0;
    }
    return 1;
}

int machine_transition_clone_file(const MachineTransitionFile *source,
    MachineTransitionFile *out_clone,
    MachineTransitionError *error) {
    if (!source || !out_clone) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-108: invalid clone contract");
        return 0;
    }
    if (!machine_transition_verify_file(source, error)) {
        return 0;
    }

    machine_transition_file_free(out_clone);
    if (!machine_interp_clone_file(&source->interp_file, &out_clone->interp_file, NULL)) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-109: failed cloning interp input");
        machine_transition_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->next_status = source->next_status;
    out_clone->has_next_fetch = source->has_next_fetch;
    out_clone->next_program_counter = source->next_program_counter;
    out_clone->next_current_segment_index = source->next_current_segment_index;
    out_clone->next_current_byte = source->next_current_byte;
    out_clone->next_current_byte_memory_offset = source->next_current_byte_memory_offset;
    return 1;
}

int machine_transition_file_get_summary(const MachineTransitionFile *transition_file,
    size_t *out_mapped_byte_count) {
    if (!transition_file) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count =
            transition_file->interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.total_mapped_byte_count;
    }
    return 1;
}

int machine_transition_file_get_header_summary(const MachineTransitionFile *transition_file,
    MachineTransitionHeaderSummary *out_summary) {
    if (!transition_file || !out_summary) {
        return 0;
    }
    out_summary->target_profile =
        transition_file->interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile;
    out_summary->step_status =
        transition_file->interp_file.payload_decode_file.decode_file.step_file.status;
    out_summary->program_counter =
        transition_file->interp_file.payload_decode_file.decode_file.step_file.program_counter;
    out_summary->stack_pointer =
        transition_file->interp_file.payload_decode_file.decode_file.step_file.stack_pointer;
    out_summary->current_segment_index =
        transition_file->interp_file.payload_decode_file.decode_file.step_file.current_segment_index;
    out_summary->mapped_byte_count =
        transition_file->interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.total_mapped_byte_count;
    return 1;
}

int machine_transition_file_get_runtime_launch_summary(const MachineTransitionFile *transition_file,
    MachineRuntimeLaunchSummary *out_summary) {
    if (!transition_file || !out_summary) {
        return 0;
    }
    return machine_interp_file_get_runtime_launch_summary(&transition_file->interp_file, out_summary);
}

int machine_transition_file_get_initial_stack_summary(const MachineTransitionFile *transition_file,
    MachineRuntimeInitialStackSummary *out_summary) {
    if (!transition_file || !out_summary) {
        return 0;
    }
    return machine_interp_file_get_initial_stack_summary(&transition_file->interp_file, out_summary);
}

int machine_transition_file_get_runtime_memory_summary(const MachineTransitionFile *transition_file,
    MachineRuntimeMemorySummary *out_summary) {
    if (!transition_file || !out_summary) {
        return 0;
    }
    return machine_interp_file_get_runtime_memory_summary(&transition_file->interp_file, out_summary);
}

int machine_transition_file_get_current_segment_summary(const MachineTransitionFile *transition_file,
    MachineRuntimeSegmentSummary *out_summary) {
    if (!transition_file || !out_summary) {
        return 0;
    }
    return machine_interp_file_get_current_segment_summary(&transition_file->interp_file, out_summary);
}

int machine_transition_file_get_fetch_summary(const MachineTransitionFile *transition_file,
    MachineStepFetchSummary *out_summary) {
    if (!transition_file || !out_summary) {
        return 0;
    }
    return machine_interp_file_get_fetch_summary(&transition_file->interp_file, out_summary);
}

int machine_transition_file_get_decode_tag_summary(const MachineTransitionFile *transition_file,
    MachineDecodeTagSummary *out_summary) {
    if (!transition_file || !out_summary) {
        return 0;
    }
    return machine_interp_file_get_decode_tag_summary(&transition_file->interp_file, out_summary);
}

int machine_transition_file_get_payload_summary(const MachineTransitionFile *transition_file,
    MachinePayloadDecodeSummary *out_summary) {
    if (!transition_file || !out_summary) {
        return 0;
    }
    return machine_interp_file_get_payload_summary(&transition_file->interp_file, out_summary);
}

int machine_transition_file_get_interp_summary(const MachineTransitionFile *transition_file,
    MachineInterpSummary *out_summary) {
    if (!transition_file || !out_summary) {
        return 0;
    }
    return machine_interp_file_get_interp_summary(&transition_file->interp_file, out_summary);
}

int machine_transition_file_get_transition_summary(const MachineTransitionFile *transition_file,
    MachineTransitionSummary *out_summary) {
    MachineInterpSummary interp_summary;

    memset(&interp_summary, 0, sizeof(interp_summary));
    if (!transition_file || !out_summary ||
        !machine_transition_file_get_interp_summary(transition_file, &interp_summary)) {
        return 0;
    }
    out_summary->resolution_kind = transition_file->resolution_kind;
    out_summary->action_kind = interp_summary.action_kind;
    out_summary->raw_byte = interp_summary.raw_byte;
    out_summary->tag_value = interp_summary.tag_value;
    out_summary->is_known = interp_summary.is_known;
    out_summary->tag_name = interp_summary.tag_name;
    out_summary->instruction_byte_count = interp_summary.instruction_byte_count;
    out_summary->next_status = transition_file->next_status;
    out_summary->has_next_fetch = transition_file->has_next_fetch;
    out_summary->next_program_counter = transition_file->next_program_counter;
    out_summary->next_current_segment_index = transition_file->next_current_segment_index;
    out_summary->next_current_byte = transition_file->next_current_byte;
    out_summary->next_current_byte_memory_offset = transition_file->next_current_byte_memory_offset;
    out_summary->has_primary_target_block = interp_summary.has_primary_target_block;
    out_summary->primary_target_block_index = interp_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = interp_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = interp_summary.secondary_target_block_index;
    out_summary->has_return_immediate = interp_summary.has_return_immediate;
    out_summary->return_immediate = interp_summary.return_immediate;
    return 1;
}

int machine_transition_file_get_next_fetch_summary(const MachineTransitionFile *transition_file,
    MachineTransitionNextFetchSummary *out_summary) {
    MachineRuntimeSegmentSummary segment_summary;
    const MachineRuntimeSegment *segment = NULL;

    memset(&segment_summary, 0, sizeof(segment_summary));
    if (!transition_file || !transition_file->has_next_fetch) {
        return 0;
    }
    if (!machine_runtime_file_get_segment(
            &transition_file->interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file,
            transition_file->next_current_segment_index,
            &segment) ||
        !segment ||
        !machine_runtime_segment_get_summary(segment, &segment_summary)) {
        return 0;
    }
    if (!out_summary) {
        return 1;
    }
    out_summary->byte_virtual_address = transition_file->next_program_counter;
    out_summary->byte_memory_offset = transition_file->next_current_byte_memory_offset;
    out_summary->segment_index = transition_file->next_current_segment_index;
    out_summary->segment_name = segment_summary.name;
    out_summary->byte_value = transition_file->next_current_byte;
    return 1;
}

int machine_transition_build_from_machine_interp_file(const MachineInterpFile *interp_file,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error) {
    MachineInterpSummary interp_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    const MachineRuntimeSegment *segment = NULL;
    size_t segment_index = 0u;
    unsigned char next_byte = 0u;

    memset(&interp_summary, 0, sizeof(interp_summary));
    memset(&runtime_memory_summary, 0, sizeof(runtime_memory_summary));
    if (!interp_file || !out_transition_file) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-110: invalid build contract");
        return 0;
    }
    if (!machine_interp_verify_file(interp_file, NULL) ||
        !machine_interp_file_get_interp_summary(interp_file, &interp_summary) ||
        !machine_interp_file_get_runtime_memory_summary(interp_file, &runtime_memory_summary)) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-111: invalid interp input");
        return 0;
    }

    machine_transition_file_free(out_transition_file);
    if (!machine_interp_clone_file(interp_file, &out_transition_file->interp_file, NULL)) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-112: failed cloning interp input");
        machine_transition_file_free(out_transition_file);
        return 0;
    }
    out_transition_file->next_status = interp_summary.next_status;

    switch (interp_summary.action_kind) {
        case MACHINE_INTERP_ACTION_ADVANCE:
            if (!interp_summary.has_next_program_counter ||
                !machine_runtime_file_find_segment_covering_virtual_address(
                    &interp_file->payload_decode_file.decode_file.step_file.launch_file.runtime_file,
                    interp_summary.next_program_counter,
                    &segment_index,
                    &segment) ||
                !segment || !segment->executable ||
                !machine_runtime_file_get_memory_byte_at_virtual_address(
                    &interp_file->payload_decode_file.decode_file.step_file.launch_file.runtime_file,
                    interp_summary.next_program_counter,
                    &next_byte)) {
                machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-113: failed deriving next fetch");
                machine_transition_file_free(out_transition_file);
                return 0;
            }
            out_transition_file->resolution_kind = MACHINE_TRANSITION_RESOLUTION_NEXT_FETCH;
            out_transition_file->next_status = MACHINE_STEP_STATUS_READY;
            out_transition_file->has_next_fetch = 1;
            out_transition_file->next_program_counter = interp_summary.next_program_counter;
            out_transition_file->next_current_segment_index = segment_index;
            out_transition_file->next_current_byte = next_byte;
            out_transition_file->next_current_byte_memory_offset =
                interp_summary.next_program_counter - runtime_memory_summary.base_virtual_address;
            break;
        case MACHINE_INTERP_ACTION_HALT:
            out_transition_file->resolution_kind = MACHINE_TRANSITION_RESOLUTION_HALT;
            out_transition_file->next_status = MACHINE_STEP_STATUS_HALTED;
            out_transition_file->has_next_fetch = 0;
            break;
        case MACHINE_INTERP_ACTION_CONTROL_TRANSFER:
            out_transition_file->resolution_kind = MACHINE_TRANSITION_RESOLUTION_DEFERRED_CONTROL_TRANSFER;
            out_transition_file->next_status = MACHINE_STEP_STATUS_READY;
            out_transition_file->has_next_fetch = 0;
            break;
        case MACHINE_INTERP_ACTION_UNSUPPORTED:
        case MACHINE_INTERP_ACTION_NONE:
        default:
            out_transition_file->resolution_kind = MACHINE_TRANSITION_RESOLUTION_UNSUPPORTED;
            out_transition_file->next_status = interp_summary.next_status;
            out_transition_file->has_next_fetch = 0;
            break;
    }

    if (!machine_transition_verify_file(out_transition_file, error)) {
        machine_transition_file_free(out_transition_file);
        return 0;
    }
    return 1;
}

int machine_transition_build_from_machine_interp_report(const MachineInterpReport *report,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error) {
    const MachineInterpFile *interp_file = NULL;

    if (!report) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-114: invalid interp-report build contract");
        return 0;
    }
    if (!machine_interp_report_get_file(report, &interp_file) || !interp_file) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-115: failed recovering interp file from report");
        return 0;
    }
    return machine_transition_build_from_machine_interp_file(interp_file, out_transition_file, error);
}

#define MACHINE_TRANSITION_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineTransitionFile *out_transition_file, MachineTransitionError *error) { \
    MachineInterpFile interp_file; \
    int ok; \
    machine_interp_file_init(&interp_file); \
    ok = builder(source, &interp_file, NULL) && \
        machine_transition_build_from_machine_interp_file(&interp_file, out_transition_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-116: failed building transition wrapper"); \
    } \
    machine_interp_file_free(&interp_file); \
    return ok; \
}

MACHINE_TRANSITION_DEFINE_BUILD_FROM_WRAPPER(machine_transition_build_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_interp_build_from_machine_payload_decode_file)
MACHINE_TRANSITION_DEFINE_BUILD_FROM_WRAPPER(machine_transition_build_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_interp_build_from_machine_payload_decode_report)
MACHINE_TRANSITION_DEFINE_BUILD_FROM_WRAPPER(machine_transition_build_from_machine_decode_file,
    MachineDecodeFile, machine_interp_build_from_machine_decode_file)
MACHINE_TRANSITION_DEFINE_BUILD_FROM_WRAPPER(machine_transition_build_from_machine_decode_report,
    MachineDecodeReport, machine_interp_build_from_machine_decode_report)
MACHINE_TRANSITION_DEFINE_BUILD_FROM_WRAPPER(machine_transition_build_from_machine_step_file,
    MachineStepFile, machine_interp_build_from_machine_step_file)
MACHINE_TRANSITION_DEFINE_BUILD_FROM_WRAPPER(machine_transition_build_from_machine_step_report,
    MachineStepReport, machine_interp_build_from_machine_step_report)
MACHINE_TRANSITION_DEFINE_BUILD_FROM_WRAPPER(machine_transition_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_interp_build_from_machine_ir_report)

int machine_transition_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error) {
    MachineInterpFile interp_file;
    int ok;

    machine_interp_file_init(&interp_file);
    ok = machine_interp_build_from_machine_ir_report_with_profile(report, profile, &interp_file, NULL) &&
        machine_transition_build_from_machine_interp_file(&interp_file, out_transition_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-117: failed building profiled transition wrapper");
    }
    machine_interp_file_free(&interp_file);
    return ok;
}

int machine_transition_build_report_from_file(const MachineTransitionFile *source,
    MachineTransitionReport *out_report,
    MachineTransitionError *error) {
    if (!source || !out_report) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-118: invalid report build contract");
        return 0;
    }
    if (!machine_transition_verify_file(source, error)) {
        return 0;
    }

    machine_transition_report_free(out_report);
    if (!machine_transition_clone_file(source, &out_report->file, error)) {
        machine_transition_report_free(out_report);
        return 0;
    }
    if (!machine_transition_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_transition_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_transition_file_get_runtime_launch_summary(&out_report->file, &out_report->runtime_launch_summary) ||
        !machine_transition_file_get_initial_stack_summary(&out_report->file, &out_report->initial_stack_summary) ||
        !machine_transition_file_get_runtime_memory_summary(&out_report->file, &out_report->runtime_memory_summary) ||
        !machine_transition_file_get_current_segment_summary(&out_report->file, &out_report->current_segment_summary) ||
        !machine_transition_file_get_fetch_summary(&out_report->file, &out_report->fetch_summary) ||
        !machine_transition_file_get_decode_tag_summary(&out_report->file, &out_report->decode_tag_summary) ||
        !machine_transition_file_get_payload_summary(&out_report->file, &out_report->payload_summary) ||
        !machine_transition_file_get_interp_summary(&out_report->file, &out_report->interp_summary) ||
        !machine_transition_file_get_transition_summary(&out_report->file, &out_report->transition_summary)) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-119: failed summarizing transition report");
        machine_transition_report_free(out_report);
        return 0;
    }
    if (out_report->file.has_next_fetch &&
        !machine_transition_file_get_next_fetch_summary(&out_report->file, &out_report->next_fetch_summary)) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-120: failed summarizing next fetch");
        machine_transition_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_TRANSITION_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineTransitionReport *out_report, MachineTransitionError *error) { \
    MachineTransitionFile transition_file; \
    int ok; \
    machine_transition_file_init(&transition_file); \
    ok = builder(source, &transition_file, error) && \
        machine_transition_build_report_from_file(&transition_file, out_report, error); \
    machine_transition_file_free(&transition_file); \
    return ok; \
}

MACHINE_TRANSITION_DEFINE_REPORT_FROM_WRAPPER(machine_transition_build_report_from_machine_interp_file,
    MachineInterpFile, machine_transition_build_from_machine_interp_file)
MACHINE_TRANSITION_DEFINE_REPORT_FROM_WRAPPER(machine_transition_build_report_from_machine_interp_report,
    MachineInterpReport, machine_transition_build_from_machine_interp_report)
MACHINE_TRANSITION_DEFINE_REPORT_FROM_WRAPPER(machine_transition_build_report_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_transition_build_from_machine_payload_decode_file)
MACHINE_TRANSITION_DEFINE_REPORT_FROM_WRAPPER(machine_transition_build_report_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_transition_build_from_machine_payload_decode_report)
MACHINE_TRANSITION_DEFINE_REPORT_FROM_WRAPPER(machine_transition_build_report_from_machine_decode_file,
    MachineDecodeFile, machine_transition_build_from_machine_decode_file)
MACHINE_TRANSITION_DEFINE_REPORT_FROM_WRAPPER(machine_transition_build_report_from_machine_decode_report,
    MachineDecodeReport, machine_transition_build_from_machine_decode_report)
MACHINE_TRANSITION_DEFINE_REPORT_FROM_WRAPPER(machine_transition_build_report_from_machine_step_file,
    MachineStepFile, machine_transition_build_from_machine_step_file)
MACHINE_TRANSITION_DEFINE_REPORT_FROM_WRAPPER(machine_transition_build_report_from_machine_step_report,
    MachineStepReport, machine_transition_build_from_machine_step_report)
MACHINE_TRANSITION_DEFINE_REPORT_FROM_WRAPPER(machine_transition_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_transition_build_from_machine_ir_report)

int machine_transition_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTransitionReport *out_report,
    MachineTransitionError *error) {
    MachineTransitionFile transition_file;
    int ok;

    machine_transition_file_init(&transition_file);
    ok = machine_transition_build_from_machine_ir_report_with_profile(report, profile, &transition_file, error) &&
        machine_transition_build_report_from_file(&transition_file, out_report, error);
    machine_transition_file_free(&transition_file);
    return ok;
}

int machine_transition_report_refresh(MachineTransitionReport *report,
    MachineTransitionError *error) {
    MachineTransitionReport refreshed_report;
    int ok;

    if (!report) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-121: invalid report refresh contract");
        return 0;
    }
    machine_transition_report_init(&refreshed_report);
    ok = machine_transition_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_transition_report_free(report);
        *report = refreshed_report;
    } else {
        machine_transition_report_free(&refreshed_report);
    }
    return ok;
}

int machine_transition_dump_file(const MachineTransitionFile *transition_file,
    char **out_text,
    MachineTransitionError *error) {
    MachineTransitionStringBuilder builder;
    MachineTransitionHeaderSummary header_summary;
    MachineTransitionSummary transition_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&transition_summary, 0, sizeof(transition_summary));
    if (!transition_file || !out_text ||
        !machine_transition_verify_file(transition_file, error) ||
        !machine_transition_file_get_header_summary(transition_file, &header_summary) ||
        !machine_transition_file_get_transition_summary(transition_file, &transition_summary)) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-122: invalid dump contract");
        return 0;
    }

    if (!machine_transition_append_format(
            &builder,
            "machine_transition profile=%s status=%s pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_step_status_name(header_summary.step_status),
            header_summary.program_counter,
            header_summary.stack_pointer,
            header_summary.current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_transition_append_format(
            &builder,
            "transition: resolution=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu next-status=%s",
            machine_transition_resolution_kind_name(transition_summary.resolution_kind),
            machine_interp_action_kind_name(transition_summary.action_kind),
            (unsigned int)transition_summary.raw_byte,
            (unsigned int)transition_summary.tag_value,
            transition_summary.is_known ? "yes" : "no",
            transition_summary.tag_name ? transition_summary.tag_name : "-",
            transition_summary.instruction_byte_count,
            machine_step_status_name(transition_summary.next_status))) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-123: out of memory building dump");
        return 0;
    }
    if (!machine_transition_append_format(
            &builder,
            " next-pc=%s",
            transition_summary.has_next_fetch ? "" : "-")) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-124: out of memory appending next pc");
        return 0;
    }
    if (transition_summary.has_next_fetch &&
        !machine_transition_append_format(&builder, "0x%zx", transition_summary.next_program_counter)) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-125: out of memory appending next pc value");
        return 0;
    }
    if (!machine_transition_append_format(
            &builder,
            " next-segment=%s",
            transition_summary.has_next_fetch ? "" : "-")) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-126: out of memory appending next segment");
        return 0;
    }
    if (transition_summary.has_next_fetch &&
        !machine_transition_append_format(&builder, "%zu", transition_summary.next_current_segment_index)) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-127: out of memory appending next segment value");
        return 0;
    }
    if (!machine_transition_append_format(
            &builder,
            " next-byte=%s",
            transition_summary.has_next_fetch ? "" : "-")) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-128: out of memory appending next byte");
        return 0;
    }
    if (transition_summary.has_next_fetch &&
        !machine_transition_append_format(&builder, "0x%02x", (unsigned int)transition_summary.next_current_byte)) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-129: out of memory appending next byte value");
        return 0;
    }
    if (!machine_transition_append_targets(&builder, &transition_summary) ||
        !machine_transition_append_return_immediate(&builder, &transition_summary) ||
        !machine_transition_append_format(&builder, "\n")) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-130: out of memory terminating dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_transition_build_dump_from_file(const MachineTransitionFile *source,
    char **out_text,
    MachineTransitionError *error) {
    return machine_transition_dump_file(source, out_text, error);
}

#define MACHINE_TRANSITION_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineTransitionError *error) { \
    MachineTransitionFile transition_file; \
    int ok; \
    machine_transition_file_init(&transition_file); \
    ok = builder(source, &transition_file, error) && \
        machine_transition_dump_file(&transition_file, out_text, error); \
    machine_transition_file_free(&transition_file); \
    return ok; \
}

MACHINE_TRANSITION_DEFINE_DUMP_FROM_WRAPPER(machine_transition_build_dump_from_machine_interp_file,
    MachineInterpFile, machine_transition_build_from_machine_interp_file)
MACHINE_TRANSITION_DEFINE_DUMP_FROM_WRAPPER(machine_transition_build_dump_from_machine_interp_report,
    MachineInterpReport, machine_transition_build_from_machine_interp_report)
MACHINE_TRANSITION_DEFINE_DUMP_FROM_WRAPPER(machine_transition_build_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_transition_build_from_machine_payload_decode_file)
MACHINE_TRANSITION_DEFINE_DUMP_FROM_WRAPPER(machine_transition_build_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_transition_build_from_machine_payload_decode_report)
MACHINE_TRANSITION_DEFINE_DUMP_FROM_WRAPPER(machine_transition_build_dump_from_machine_decode_file,
    MachineDecodeFile, machine_transition_build_from_machine_decode_file)
MACHINE_TRANSITION_DEFINE_DUMP_FROM_WRAPPER(machine_transition_build_dump_from_machine_decode_report,
    MachineDecodeReport, machine_transition_build_from_machine_decode_report)
MACHINE_TRANSITION_DEFINE_DUMP_FROM_WRAPPER(machine_transition_build_dump_from_machine_step_file,
    MachineStepFile, machine_transition_build_from_machine_step_file)
MACHINE_TRANSITION_DEFINE_DUMP_FROM_WRAPPER(machine_transition_build_dump_from_machine_step_report,
    MachineStepReport, machine_transition_build_from_machine_step_report)
MACHINE_TRANSITION_DEFINE_DUMP_FROM_WRAPPER(machine_transition_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_transition_build_from_machine_ir_report)

int machine_transition_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTransitionError *error) {
    MachineTransitionFile transition_file;
    int ok;

    machine_transition_file_init(&transition_file);
    ok = machine_transition_build_from_machine_ir_report_with_profile(report, profile, &transition_file, error) &&
        machine_transition_dump_file(&transition_file, out_text, error);
    machine_transition_file_free(&transition_file);
    return ok;
}

int machine_transition_report_get_summary(const MachineTransitionReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_transition_report_get_overview_artifact(const MachineTransitionReport *report,
    MachineTransitionReportOverviewArtifact *out_artifact) {
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
    out_artifact->transition_summary = &report->transition_summary;
    out_artifact->next_fetch_summary = report->file.has_next_fetch ? &report->next_fetch_summary : NULL;
    return 1;
}

int machine_transition_report_get_file(const MachineTransitionReport *report,
    const MachineTransitionFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_transition_report_get_interp_file(const MachineTransitionReport *report,
    const MachineInterpFile **out_interp_file) {
    if (!report || !out_interp_file) {
        return 0;
    }
    *out_interp_file = &report->file.interp_file;
    return 1;
}

#define MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineTransitionReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(machine_transition_report_get_header_summary_artifact,
    header_summary, MachineTransitionHeaderSummary)
MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(machine_transition_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineTransitionTargetPolicySummary)
MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(machine_transition_report_get_runtime_launch_summary_artifact,
    runtime_launch_summary, MachineRuntimeLaunchSummary)
MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(machine_transition_report_get_initial_stack_summary_artifact,
    initial_stack_summary, MachineRuntimeInitialStackSummary)
MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(machine_transition_report_get_runtime_memory_summary_artifact,
    runtime_memory_summary, MachineRuntimeMemorySummary)
MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(machine_transition_report_get_current_segment_summary_artifact,
    current_segment_summary, MachineRuntimeSegmentSummary)
MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(machine_transition_report_get_fetch_summary_artifact,
    fetch_summary, MachineStepFetchSummary)
MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(machine_transition_report_get_decode_tag_summary_artifact,
    decode_tag_summary, MachineDecodeTagSummary)
MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(machine_transition_report_get_payload_summary_artifact,
    payload_summary, MachinePayloadDecodeSummary)
MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(machine_transition_report_get_interp_summary_artifact,
    interp_summary, MachineInterpSummary)
MACHINE_TRANSITION_DEFINE_REPORT_ARTIFACT_GETTER(machine_transition_report_get_transition_summary_artifact,
    transition_summary, MachineTransitionSummary)

int machine_transition_report_get_next_fetch_summary_artifact(const MachineTransitionReport *report,
    const MachineTransitionNextFetchSummary **out_summary) {
    if (!report || !out_summary || !report->file.has_next_fetch) {
        return 0;
    }
    *out_summary = &report->next_fetch_summary;
    return 1;
}

#define MACHINE_TRANSITION_DEFINE_OVERVIEW_GETTER(name, field, type) \
int name(const MachineTransitionReportOverviewArtifact *artifact, const type **out_summary) { \
    if (!artifact || !out_summary) { \
        return 0; \
    } \
    *out_summary = artifact->field; \
    return 1; \
}

MACHINE_TRANSITION_DEFINE_OVERVIEW_GETTER(machine_transition_report_overview_artifact_get_runtime_launch_summary_artifact,
    runtime_launch_summary, MachineRuntimeLaunchSummary)
MACHINE_TRANSITION_DEFINE_OVERVIEW_GETTER(machine_transition_report_overview_artifact_get_initial_stack_summary_artifact,
    initial_stack_summary, MachineRuntimeInitialStackSummary)
MACHINE_TRANSITION_DEFINE_OVERVIEW_GETTER(machine_transition_report_overview_artifact_get_runtime_memory_summary_artifact,
    runtime_memory_summary, MachineRuntimeMemorySummary)
MACHINE_TRANSITION_DEFINE_OVERVIEW_GETTER(machine_transition_report_overview_artifact_get_current_segment_summary_artifact,
    current_segment_summary, MachineRuntimeSegmentSummary)
MACHINE_TRANSITION_DEFINE_OVERVIEW_GETTER(machine_transition_report_overview_artifact_get_fetch_summary_artifact,
    fetch_summary, MachineStepFetchSummary)
MACHINE_TRANSITION_DEFINE_OVERVIEW_GETTER(machine_transition_report_overview_artifact_get_decode_tag_summary_artifact,
    decode_tag_summary, MachineDecodeTagSummary)
MACHINE_TRANSITION_DEFINE_OVERVIEW_GETTER(machine_transition_report_overview_artifact_get_payload_summary_artifact,
    payload_summary, MachinePayloadDecodeSummary)
MACHINE_TRANSITION_DEFINE_OVERVIEW_GETTER(machine_transition_report_overview_artifact_get_interp_summary_artifact,
    interp_summary, MachineInterpSummary)
MACHINE_TRANSITION_DEFINE_OVERVIEW_GETTER(machine_transition_report_overview_artifact_get_transition_summary_artifact,
    transition_summary, MachineTransitionSummary)

int machine_transition_report_overview_artifact_get_next_fetch_summary_artifact(
    const MachineTransitionReportOverviewArtifact *artifact,
    const MachineTransitionNextFetchSummary **out_summary) {
    if (!artifact || !out_summary || !artifact->next_fetch_summary) {
        return 0;
    }
    *out_summary = artifact->next_fetch_summary;
    return 1;
}

int machine_transition_dump_report(const MachineTransitionReport *report,
    char **out_text,
    MachineTransitionError *error) {
    MachineTransitionStringBuilder builder;
    const MachineTransitionSummary *transition_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_transition_report_get_transition_summary_artifact(report, &transition_summary) ||
        !transition_summary) {
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-131: invalid report dump contract");
        return 0;
    }

    if (!machine_transition_append_format(
            &builder,
            "machine_transition profile=%s status=%s pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_step_status_name(report->header_summary.step_status),
            report->header_summary.program_counter,
            report->header_summary.stack_pointer,
            report->header_summary.current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_transition_append_format(
            &builder,
            "transition: resolution=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu next-status=%s",
            machine_transition_resolution_kind_name(transition_summary->resolution_kind),
            machine_interp_action_kind_name(transition_summary->action_kind),
            (unsigned int)transition_summary->raw_byte,
            (unsigned int)transition_summary->tag_value,
            transition_summary->is_known ? "yes" : "no",
            transition_summary->tag_name ? transition_summary->tag_name : "-",
            transition_summary->instruction_byte_count,
            machine_step_status_name(transition_summary->next_status))) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-132: out of memory building report dump");
        return 0;
    }
    if (!machine_transition_append_format(
            &builder,
            " next-pc=%s",
            transition_summary->has_next_fetch ? "" : "-")) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-133: out of memory appending report next pc");
        return 0;
    }
    if (transition_summary->has_next_fetch &&
        !machine_transition_append_format(&builder, "0x%zx", transition_summary->next_program_counter)) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-134: out of memory appending report next pc value");
        return 0;
    }
    if (!machine_transition_append_format(
            &builder,
            " next-segment=%s",
            transition_summary->has_next_fetch ? "" : "-")) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-135: out of memory appending report next segment");
        return 0;
    }
    if (transition_summary->has_next_fetch &&
        !machine_transition_append_format(&builder, "%zu", transition_summary->next_current_segment_index)) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-136: out of memory appending report next segment value");
        return 0;
    }
    if (!machine_transition_append_format(
            &builder,
            " next-byte=%s",
            transition_summary->has_next_fetch ? "" : "-")) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-137: out of memory appending report next byte");
        return 0;
    }
    if (transition_summary->has_next_fetch &&
        !machine_transition_append_format(&builder, "0x%02x", (unsigned int)transition_summary->next_current_byte)) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-138: out of memory appending report next byte value");
        return 0;
    }
    if (!machine_transition_append_targets(&builder, transition_summary) ||
        !machine_transition_append_return_immediate(&builder, transition_summary) ||
        !machine_transition_append_format(&builder, "\nreport_overview:\n") ||
        !machine_transition_append_format(
            &builder,
            "  status=%s current-segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_step_status_name(report->header_summary.step_status),
            report->header_summary.current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.program_counter,
            report->header_summary.stack_pointer) ||
        !machine_transition_append_format(
            &builder,
            "  policy: profile=%s linear-next-fetch=%s halt=%s deferred-control=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.resolves_linear_next_fetch ? "yes" : "no",
            report->target_policy_summary.resolves_halt_transition ? "yes" : "no",
            report->target_policy_summary.defers_control_transfer_targets ? "yes" : "no") ||
        !machine_transition_append_format(
            &builder,
            "  transition: resolution=%s next-status=%s next-pc=%s",
            machine_transition_resolution_kind_name(transition_summary->resolution_kind),
            machine_step_status_name(transition_summary->next_status),
            transition_summary->has_next_fetch ? "" : "-")) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-139: out of memory building report overview");
        return 0;
    }
    if (transition_summary->has_next_fetch &&
        !machine_transition_append_format(&builder, "0x%zx", transition_summary->next_program_counter)) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-140: out of memory appending report overview next pc");
        return 0;
    }
    if (!machine_transition_append_targets(&builder, transition_summary) ||
        !machine_transition_append_return_immediate(&builder, transition_summary) ||
        !machine_transition_append_format(&builder, "\n")) {
        free(builder.data);
        machine_transition_set_error(error, 0, 0, "MACHINE-TRANSITION-141: out of memory terminating report overview");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_transition_build_report_dump_from_file(const MachineTransitionFile *source,
    char **out_text,
    MachineTransitionError *error) {
    MachineTransitionReport report;
    int ok;

    machine_transition_report_init(&report);
    ok = machine_transition_build_report_from_file(source, &report, error) &&
        machine_transition_dump_report(&report, out_text, error);
    machine_transition_report_free(&report);
    return ok;
}

#define MACHINE_TRANSITION_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineTransitionError *error) { \
    MachineTransitionReport report; \
    int ok; \
    machine_transition_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_transition_dump_report(&report, out_text, error); \
    machine_transition_report_free(&report); \
    return ok; \
}

MACHINE_TRANSITION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_transition_build_report_dump_from_machine_interp_file,
    MachineInterpFile, machine_transition_build_report_from_machine_interp_file)
MACHINE_TRANSITION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_transition_build_report_dump_from_machine_interp_report,
    MachineInterpReport, machine_transition_build_report_from_machine_interp_report)
MACHINE_TRANSITION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_transition_build_report_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_transition_build_report_from_machine_payload_decode_file)
MACHINE_TRANSITION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_transition_build_report_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_transition_build_report_from_machine_payload_decode_report)
MACHINE_TRANSITION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_transition_build_report_dump_from_machine_decode_file,
    MachineDecodeFile, machine_transition_build_report_from_machine_decode_file)
MACHINE_TRANSITION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_transition_build_report_dump_from_machine_decode_report,
    MachineDecodeReport, machine_transition_build_report_from_machine_decode_report)
MACHINE_TRANSITION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_transition_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_transition_build_report_from_machine_step_file)
MACHINE_TRANSITION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_transition_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_transition_build_report_from_machine_step_report)
MACHINE_TRANSITION_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_transition_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_transition_build_report_from_machine_ir_report)

int machine_transition_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTransitionError *error) {
    MachineTransitionReport report_file;
    int ok;

    machine_transition_report_init(&report_file);
    ok = machine_transition_build_report_from_machine_ir_report_with_profile(report, profile, &report_file, error) &&
        machine_transition_dump_report(&report_file, out_text, error);
    machine_transition_report_free(&report_file);
    return ok;
}
