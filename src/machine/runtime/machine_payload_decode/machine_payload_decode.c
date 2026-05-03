#include "machine/payload_decode.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachinePayloadDecodeStringBuilder;

static void machine_payload_decode_set_error(MachinePayloadDecodeError *error, int line, int column, const char *fmt, ...);
static int machine_payload_decode_append_format(MachinePayloadDecodeStringBuilder *builder, const char *fmt, ...);

static void machine_payload_decode_set_error(MachinePayloadDecodeError *error, int line, int column, const char *fmt, ...) {
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

static int machine_payload_decode_append_format(MachinePayloadDecodeStringBuilder *builder, const char *fmt, ...) {
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

void machine_payload_decode_file_init(MachinePayloadDecodeFile *decode_file) {
    if (!decode_file) {
        return;
    }
    machine_decode_file_init(&decode_file->decode_file);
    decode_file->kind = MACHINE_PAYLOAD_DECODE_KIND_NONE;
    decode_file->is_known = 0;
    decode_file->total_byte_count = 1u;
    decode_file->payload_byte_count = 0u;
    memset(decode_file->payload_bytes, 0, sizeof(decode_file->payload_bytes));
    decode_file->immediate_value = 0u;
}

void machine_payload_decode_file_free(MachinePayloadDecodeFile *decode_file) {
    if (!decode_file) {
        return;
    }
    machine_decode_file_free(&decode_file->decode_file);
    machine_payload_decode_file_init(decode_file);
}

void machine_payload_decode_report_init(MachinePayloadDecodeReport *report) {
    if (!report) {
        return;
    }
    machine_payload_decode_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    memset(&report->runtime_launch_summary, 0, sizeof(report->runtime_launch_summary));
    memset(&report->initial_stack_summary, 0, sizeof(report->initial_stack_summary));
    memset(&report->runtime_memory_summary, 0, sizeof(report->runtime_memory_summary));
    memset(&report->current_segment_summary, 0, sizeof(report->current_segment_summary));
    memset(&report->fetch_summary, 0, sizeof(report->fetch_summary));
    memset(&report->decode_tag_summary, 0, sizeof(report->decode_tag_summary));
    memset(&report->payload_summary, 0, sizeof(report->payload_summary));
}

void machine_payload_decode_report_free(MachinePayloadDecodeReport *report) {
    if (!report) {
        return;
    }
    machine_payload_decode_file_free(&report->file);
    machine_payload_decode_report_init(report);
}

const char *machine_payload_decode_kind_name(MachinePayloadDecodeKind kind) {
    switch (kind) {
        case MACHINE_PAYLOAD_DECODE_KIND_NONE:
            return "none";
        case MACHINE_PAYLOAD_DECODE_KIND_OP:
            return "op";
        case MACHINE_PAYLOAD_DECODE_KIND_TERMINATOR:
            return "terminator";
    }
    return "unknown";
}

int machine_payload_decode_get_target_policy_summary(MachineElfTargetProfile profile,
    MachinePayloadDecodeTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->max_payload_byte_count = 3u;
    return 1;
}

int machine_payload_decode_file_get_target_policy_summary(const MachinePayloadDecodeFile *decode_file,
    MachinePayloadDecodeTargetPolicySummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    return machine_payload_decode_get_target_policy_summary(
        decode_file->decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_payload_decode_verify_file(const MachinePayloadDecodeFile *decode_file,
    MachinePayloadDecodeError *error) {
    MachineDecodeTagSummary tag_summary;
    MachineStepFetchSummary fetch_summary;

    memset(&tag_summary, 0, sizeof(tag_summary));
    memset(&fetch_summary, 0, sizeof(fetch_summary));
    if (!decode_file) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-100: invalid payload decode file");
        return 0;
    }
    if (!machine_decode_verify_file(&decode_file->decode_file, NULL)) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-101: invalid decode input");
        return 0;
    }
    if (!machine_decode_file_get_tag_summary(&decode_file->decode_file, &tag_summary) ||
        !machine_decode_file_get_fetch_summary(&decode_file->decode_file, &fetch_summary)) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-102: failed deriving decode facts");
        return 0;
    }
    if (decode_file->kind == MACHINE_PAYLOAD_DECODE_KIND_NONE &&
        (decode_file->payload_byte_count != 0u || decode_file->is_known)) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-103: empty payload decode must not expose payload bytes");
        return 0;
    }
    if (decode_file->payload_byte_count > 3u ||
        decode_file->total_byte_count != decode_file->payload_byte_count + 1u) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-104: invalid payload byte counts");
        return 0;
    }
    if (decode_file->decode_file.raw_byte != fetch_summary.byte_value ||
        decode_file->decode_file.raw_byte != tag_summary.raw_byte) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-105: payload decode must match current fetch byte");
        return 0;
    }
    return 1;
}

int machine_payload_decode_clone_file(const MachinePayloadDecodeFile *source,
    MachinePayloadDecodeFile *out_clone,
    MachinePayloadDecodeError *error) {
    if (!source || !out_clone) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-106: invalid payload decode clone contract");
        return 0;
    }
    if (!machine_payload_decode_verify_file(source, error)) {
        return 0;
    }

    machine_payload_decode_file_free(out_clone);
    if (!machine_decode_clone_file(&source->decode_file, &out_clone->decode_file, NULL)) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-107: failed cloning decode input");
        machine_payload_decode_file_free(out_clone);
        return 0;
    }
    out_clone->kind = source->kind;
    out_clone->is_known = source->is_known;
    out_clone->total_byte_count = source->total_byte_count;
    out_clone->payload_byte_count = source->payload_byte_count;
    memcpy(out_clone->payload_bytes, source->payload_bytes, sizeof(out_clone->payload_bytes));
    out_clone->immediate_value = source->immediate_value;
    return 1;
}

int machine_payload_decode_file_get_summary(const MachinePayloadDecodeFile *decode_file,
    size_t *out_mapped_byte_count) {
    if (!decode_file) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count =
            decode_file->decode_file.step_file.launch_file.runtime_file.total_mapped_byte_count;
    }
    return 1;
}

int machine_payload_decode_file_get_header_summary(const MachinePayloadDecodeFile *decode_file,
    MachinePayloadDecodeHeaderSummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    out_summary->target_profile =
        decode_file->decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile;
    out_summary->step_status = decode_file->decode_file.step_file.status;
    out_summary->program_counter = decode_file->decode_file.step_file.program_counter;
    out_summary->stack_pointer = decode_file->decode_file.step_file.stack_pointer;
    out_summary->current_segment_index = decode_file->decode_file.step_file.current_segment_index;
    out_summary->mapped_byte_count =
        decode_file->decode_file.step_file.launch_file.runtime_file.total_mapped_byte_count;
    return 1;
}

int machine_payload_decode_file_get_runtime_launch_summary(const MachinePayloadDecodeFile *decode_file,
    MachineRuntimeLaunchSummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    return machine_decode_file_get_runtime_launch_summary(&decode_file->decode_file, out_summary);
}

int machine_payload_decode_file_get_initial_stack_summary(const MachinePayloadDecodeFile *decode_file,
    MachineRuntimeInitialStackSummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    return machine_decode_file_get_initial_stack_summary(&decode_file->decode_file, out_summary);
}

int machine_payload_decode_file_get_runtime_memory_summary(const MachinePayloadDecodeFile *decode_file,
    MachineRuntimeMemorySummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    return machine_decode_file_get_runtime_memory_summary(&decode_file->decode_file, out_summary);
}

int machine_payload_decode_file_get_current_segment_summary(const MachinePayloadDecodeFile *decode_file,
    MachineRuntimeSegmentSummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    return machine_decode_file_get_current_segment_summary(&decode_file->decode_file, NULL, out_summary);
}

int machine_payload_decode_file_get_fetch_summary(const MachinePayloadDecodeFile *decode_file,
    MachineStepFetchSummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    return machine_decode_file_get_fetch_summary(&decode_file->decode_file, out_summary);
}

int machine_payload_decode_file_get_decode_tag_summary(const MachinePayloadDecodeFile *decode_file,
    MachineDecodeTagSummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    return machine_decode_file_get_tag_summary(&decode_file->decode_file, out_summary);
}

int machine_payload_decode_file_get_payload_summary(const MachinePayloadDecodeFile *decode_file,
    MachinePayloadDecodeSummary *out_summary) {
    MachineDecodeTagSummary decode_tag_summary;

    memset(&decode_tag_summary, 0, sizeof(decode_tag_summary));
    if (!decode_file || !out_summary ||
        !machine_decode_file_get_tag_summary(&decode_file->decode_file, &decode_tag_summary)) {
        return 0;
    }
    out_summary->kind = decode_file->kind;
    out_summary->raw_byte = decode_file->decode_file.raw_byte;
    out_summary->tag_value = decode_file->decode_file.tag_value;
    out_summary->is_known = decode_file->is_known;
    out_summary->tag_name = decode_tag_summary.tag_name;
    out_summary->total_byte_count = decode_file->total_byte_count;
    out_summary->payload_byte_count = decode_file->payload_byte_count;
    memcpy(out_summary->payload_bytes, decode_file->payload_bytes, sizeof(out_summary->payload_bytes));
    out_summary->immediate_value = decode_file->immediate_value;
    return 1;
}

int machine_payload_decode_build_from_machine_decode_file(const MachineDecodeFile *decode_file,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error) {
    MachineStepFetchSummary fetch_summary;
    MachineDecodeTagSummary decode_tag_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    unsigned char payload_bytes[3] = {0u, 0u, 0u};
    size_t payload_byte_count = 0u;
    size_t total_byte_count = 1u;
    size_t immediate_value = 0u;
    int is_known = 0;
    MachinePayloadDecodeKind kind = MACHINE_PAYLOAD_DECODE_KIND_NONE;
    size_t index;

    memset(&fetch_summary, 0, sizeof(fetch_summary));
    memset(&decode_tag_summary, 0, sizeof(decode_tag_summary));
    memset(&runtime_memory_summary, 0, sizeof(runtime_memory_summary));
    if (!decode_file || !out_payload_decode_file) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-108: invalid payload decode build contract");
        return 0;
    }
    if (!machine_decode_verify_file(decode_file, NULL) ||
        !machine_decode_file_get_fetch_summary(decode_file, &fetch_summary) ||
        !machine_decode_file_get_tag_summary(decode_file, &decode_tag_summary) ||
        !machine_decode_file_get_runtime_memory_summary(decode_file, &runtime_memory_summary)) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-109: invalid decode input");
        return 0;
    }

    if (decode_tag_summary.tag_class == MACHINE_DECODE_TAG_OP) {
        kind = MACHINE_PAYLOAD_DECODE_KIND_OP;
        switch ((MachineSelectOpKind)decode_tag_summary.tag_value) {
            case MACHINE_SELECT_OP_MATERIALIZE_IMM:
            case MACHINE_SELECT_OP_ALU_IMM:
            case MACHINE_SELECT_OP_CMP_IMM:
            case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
            case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                payload_byte_count = 1u;
                is_known = 1;
                break;
            case MACHINE_SELECT_OP_CALL:
            case MACHINE_SELECT_OP_CALL_IMM:
            case MACHINE_SELECT_OP_CALL_SPILL:
            case MACHINE_SELECT_OP_CALL_IMM_SPILL:
            case MACHINE_SELECT_OP_CALL_VOID:
            case MACHINE_SELECT_OP_CALL_VOID_IMM:
                payload_byte_count = 1u;
                is_known = 1;
                break;
            default:
                payload_byte_count = 0u;
                is_known = decode_tag_summary.is_known;
                break;
        }
    } else if (decode_tag_summary.tag_class == MACHINE_DECODE_TAG_TERMINATOR) {
        kind = MACHINE_PAYLOAD_DECODE_KIND_TERMINATOR;
        switch ((MachineLayoutTerminatorKind)decode_tag_summary.tag_value) {
            case MACHINE_LAYOUT_TERM_RETURN_IMM:
            case MACHINE_LAYOUT_TERM_RETURN_SPILL:
            case MACHINE_LAYOUT_TERM_JUMP:
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                payload_byte_count = 1u;
                is_known = 1;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH:
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                payload_byte_count = 2u;
                is_known = 1;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                payload_byte_count = 3u;
                is_known = 1;
                break;
            default:
                payload_byte_count = 0u;
                is_known = decode_tag_summary.is_known;
                break;
        }
    }

    total_byte_count = 1u + payload_byte_count;
    if (fetch_summary.byte_memory_offset + total_byte_count > runtime_memory_summary.span_byte_count) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-110: payload decode would read past mapped memory");
        return 0;
    }

    for (index = 0u; index < payload_byte_count; ++index) {
        if (!machine_runtime_file_get_memory_byte_at_offset(
                &decode_file->step_file.launch_file.runtime_file,
                fetch_summary.byte_memory_offset + 1u + index,
                &payload_bytes[index])) {
            machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-111: failed reading payload byte");
            return 0;
        }
    }
    if (payload_byte_count > 0u) {
        immediate_value = payload_bytes[0];
    }

    machine_payload_decode_file_free(out_payload_decode_file);
    if (!machine_decode_clone_file(decode_file, &out_payload_decode_file->decode_file, NULL)) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-112: failed cloning decode input");
        machine_payload_decode_file_free(out_payload_decode_file);
        return 0;
    }
    out_payload_decode_file->kind = kind;
    out_payload_decode_file->is_known = is_known;
    out_payload_decode_file->total_byte_count = total_byte_count;
    out_payload_decode_file->payload_byte_count = payload_byte_count;
    memcpy(out_payload_decode_file->payload_bytes, payload_bytes, sizeof(payload_bytes));
    out_payload_decode_file->immediate_value = immediate_value;

    if (!machine_payload_decode_verify_file(out_payload_decode_file, error)) {
        machine_payload_decode_file_free(out_payload_decode_file);
        return 0;
    }
    return 1;
}

int machine_payload_decode_build_from_machine_decode_report(const MachineDecodeReport *report,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error) {
    const MachineDecodeFile *decode_file = NULL;

    if (!report) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-113: invalid build-from-decode-report contract");
        return 0;
    }
    if (!machine_decode_report_get_file(report, &decode_file) || !decode_file) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-114: failed recovering decode file from report");
        return 0;
    }
    return machine_payload_decode_build_from_machine_decode_file(decode_file, out_payload_decode_file, error);
}

int machine_payload_decode_build_from_machine_step_file(const MachineStepFile *step_file,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_step_file(step_file, &decode_file, NULL) &&
        machine_payload_decode_build_from_machine_decode_file(&decode_file, out_payload_decode_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-115: failed building payload decode from step file");
    }
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_payload_decode_build_from_machine_step_report(const MachineStepReport *report,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_step_report(report, &decode_file, NULL) &&
        machine_payload_decode_build_from_machine_decode_file(&decode_file, out_payload_decode_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-116: failed building payload decode from step report");
    }
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_payload_decode_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_ir_report(report, &decode_file, NULL) &&
        machine_payload_decode_build_from_machine_decode_file(&decode_file, out_payload_decode_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-117: failed building payload decode from machine-ir report");
    }
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_payload_decode_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_ir_report_with_profile(report, profile, &decode_file, NULL) &&
        machine_payload_decode_build_from_machine_decode_file(&decode_file, out_payload_decode_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-118: failed building profiled payload decode from machine-ir report");
    }
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_payload_decode_build_report_from_file(const MachinePayloadDecodeFile *source,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error) {
    if (!source || !out_report) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-119: invalid payload decode report build contract");
        return 0;
    }
    if (!machine_payload_decode_verify_file(source, error)) {
        return 0;
    }

    machine_payload_decode_report_free(out_report);
    if (!machine_payload_decode_clone_file(source, &out_report->file, error)) {
        machine_payload_decode_report_free(out_report);
        return 0;
    }
    if (!machine_payload_decode_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_payload_decode_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_payload_decode_file_get_runtime_launch_summary(&out_report->file, &out_report->runtime_launch_summary) ||
        !machine_payload_decode_file_get_initial_stack_summary(&out_report->file, &out_report->initial_stack_summary) ||
        !machine_payload_decode_file_get_runtime_memory_summary(&out_report->file, &out_report->runtime_memory_summary) ||
        !machine_payload_decode_file_get_current_segment_summary(&out_report->file, &out_report->current_segment_summary) ||
        !machine_payload_decode_file_get_fetch_summary(&out_report->file, &out_report->fetch_summary) ||
        !machine_payload_decode_file_get_decode_tag_summary(&out_report->file, &out_report->decode_tag_summary) ||
        !machine_payload_decode_file_get_payload_summary(&out_report->file, &out_report->payload_summary)) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-120: failed summarizing payload decode report");
        machine_payload_decode_report_free(out_report);
        return 0;
    }
    return 1;
}

int machine_payload_decode_build_report_from_machine_decode_file(const MachineDecodeFile *source,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_decode_file(source, &payload_decode_file, error) &&
        machine_payload_decode_build_report_from_file(&payload_decode_file, out_report, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_build_report_from_machine_decode_report(const MachineDecodeReport *report,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_decode_report(report, &payload_decode_file, error) &&
        machine_payload_decode_build_report_from_file(&payload_decode_file, out_report, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_build_report_from_machine_step_file(const MachineStepFile *source,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_step_file(source, &payload_decode_file, error) &&
        machine_payload_decode_build_report_from_file(&payload_decode_file, out_report, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_build_report_from_machine_step_report(const MachineStepReport *report,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_step_report(report, &payload_decode_file, error) &&
        machine_payload_decode_build_report_from_file(&payload_decode_file, out_report, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_ir_report(report, &payload_decode_file, error) &&
        machine_payload_decode_build_report_from_file(&payload_decode_file, out_report, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_ir_report_with_profile(report, profile, &payload_decode_file, error) &&
        machine_payload_decode_build_report_from_file(&payload_decode_file, out_report, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_report_refresh(MachinePayloadDecodeReport *report,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeReport refreshed_report;
    int ok;

    if (!report) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-121: invalid payload decode report refresh contract");
        return 0;
    }
    machine_payload_decode_report_init(&refreshed_report);
    ok = machine_payload_decode_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_payload_decode_report_free(report);
        *report = refreshed_report;
    } else {
        machine_payload_decode_report_free(&refreshed_report);
    }
    return ok;
}

int machine_payload_decode_dump_file(const MachinePayloadDecodeFile *decode_file,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeStringBuilder builder;
    MachinePayloadDecodeHeaderSummary header_summary;
    MachinePayloadDecodeSummary payload_summary;
    size_t payload_index;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&payload_summary, 0, sizeof(payload_summary));
    if (!decode_file || !out_text ||
        !machine_payload_decode_verify_file(decode_file, error) ||
        !machine_payload_decode_file_get_header_summary(decode_file, &header_summary) ||
        !machine_payload_decode_file_get_payload_summary(decode_file, &payload_summary)) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-122: invalid payload decode dump contract");
        return 0;
    }

    if (!machine_payload_decode_append_format(
            &builder,
            "machine_payload_decode profile=%s elf_origin=%s elf_semantics=%s status=%s pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_elf_target_profile_name(
                decode_file->decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                decode_file->decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_step_status_name(header_summary.step_status),
            header_summary.program_counter,
            header_summary.stack_pointer,
            header_summary.current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_payload_decode_append_format(
            &builder,
            "payload: kind=%s raw=0x%02x value=0x%02x known=%s name=%s total-bytes=%zu payload-bytes=%zu imm=%zu",
            machine_payload_decode_kind_name(payload_summary.kind),
            (unsigned int)payload_summary.raw_byte,
            (unsigned int)payload_summary.tag_value,
            payload_summary.is_known ? "yes" : "no",
            payload_summary.tag_name ? payload_summary.tag_name : "-",
            payload_summary.total_byte_count,
            payload_summary.payload_byte_count,
            payload_summary.immediate_value)) {
        free(builder.data);
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-123: out of memory building payload decode dump");
        return 0;
    }

    for (payload_index = 0u; payload_index < payload_summary.payload_byte_count; ++payload_index) {
        if (!machine_payload_decode_append_format(
                &builder,
                "%s0x%02x",
                payload_index == 0u ? " bytes=[" : ",",
                (unsigned int)payload_summary.payload_bytes[payload_index])) {
            free(builder.data);
            machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-124: out of memory appending payload bytes");
            return 0;
        }
    }
    if (!machine_payload_decode_append_format(
            &builder,
            "%s\n",
            payload_summary.payload_byte_count > 0u ? "]" : " bytes=[]")) {
        free(builder.data);
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-125: out of memory terminating payload dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_payload_decode_build_dump_from_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachinePayloadDecodeError *error) {
    return machine_payload_decode_dump_file(source, out_text, error);
}

int machine_payload_decode_build_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_decode_file(source, &payload_decode_file, error) &&
        machine_payload_decode_dump_file(&payload_decode_file, out_text, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_build_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_decode_report(report, &payload_decode_file, error) &&
        machine_payload_decode_dump_file(&payload_decode_file, out_text, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_step_file(source, &payload_decode_file, error) &&
        machine_payload_decode_dump_file(&payload_decode_file, out_text, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_step_report(report, &payload_decode_file, error) &&
        machine_payload_decode_dump_file(&payload_decode_file, out_text, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_ir_report(report, &payload_decode_file, error) &&
        machine_payload_decode_dump_file(&payload_decode_file, out_text, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeFile payload_decode_file;
    int ok;

    machine_payload_decode_file_init(&payload_decode_file);
    ok = machine_payload_decode_build_from_machine_ir_report_with_profile(report, profile, &payload_decode_file, error) &&
        machine_payload_decode_dump_file(&payload_decode_file, out_text, error);
    machine_payload_decode_file_free(&payload_decode_file);
    return ok;
}

int machine_payload_decode_report_get_summary(const MachinePayloadDecodeReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_payload_decode_file_get_source_elf_artifact_summary(const MachinePayloadDecodeFile *decode_file,
    MachineElfArtifactSummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    *out_summary =
        decode_file->decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file
            .source_elf_artifact_summary;
    return 1;
}

int machine_payload_decode_report_get_overview_artifact(const MachinePayloadDecodeReport *report,
    MachinePayloadDecodeReportOverviewArtifact *out_artifact) {
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
    return 1;
}

int machine_payload_decode_report_get_file(const MachinePayloadDecodeReport *report,
    const MachinePayloadDecodeFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_payload_decode_report_get_decode_file(const MachinePayloadDecodeReport *report,
    const MachineDecodeFile **out_decode_file) {
    if (!report || !out_decode_file) {
        return 0;
    }
    *out_decode_file = &report->file.decode_file;
    return 1;
}

int machine_payload_decode_report_get_source_elf_artifact_summary_artifact(
    const MachinePayloadDecodeReport *report,
    const MachineElfArtifactSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file
                         .source_elf_artifact_summary;
    return 1;
}

int machine_payload_decode_report_get_header_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachinePayloadDecodeHeaderSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->header_summary;
    return 1;
}

int machine_payload_decode_report_get_target_policy_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachinePayloadDecodeTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_payload_decode_report_get_runtime_launch_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineRuntimeLaunchSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->runtime_launch_summary;
    return 1;
}

int machine_payload_decode_report_get_initial_stack_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineRuntimeInitialStackSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->initial_stack_summary;
    return 1;
}

int machine_payload_decode_report_get_runtime_memory_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineRuntimeMemorySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->runtime_memory_summary;
    return 1;
}

int machine_payload_decode_report_get_current_segment_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineRuntimeSegmentSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->current_segment_summary;
    return 1;
}

int machine_payload_decode_report_get_fetch_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineStepFetchSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->fetch_summary;
    return 1;
}

int machine_payload_decode_report_get_decode_tag_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineDecodeTagSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->decode_tag_summary;
    return 1;
}

int machine_payload_decode_report_get_payload_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachinePayloadDecodeSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->payload_summary;
    return 1;
}

int machine_payload_decode_report_overview_artifact_get_runtime_launch_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->runtime_launch_summary;
    return 1;
}

int machine_payload_decode_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->initial_stack_summary;
    return 1;
}

int machine_payload_decode_report_overview_artifact_get_runtime_memory_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeMemorySummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->runtime_memory_summary;
    return 1;
}

int machine_payload_decode_report_overview_artifact_get_current_segment_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeSegmentSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->current_segment_summary;
    return 1;
}

int machine_payload_decode_report_overview_artifact_get_fetch_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineStepFetchSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->fetch_summary;
    return 1;
}

int machine_payload_decode_report_overview_artifact_get_decode_tag_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineDecodeTagSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->decode_tag_summary;
    return 1;
}

int machine_payload_decode_report_overview_artifact_get_payload_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachinePayloadDecodeSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->payload_summary;
    return 1;
}

int machine_payload_decode_dump_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeStringBuilder builder;
    size_t payload_index;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text) {
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-126: invalid payload decode report dump contract");
        return 0;
    }

    if (!machine_payload_decode_append_format(
            &builder,
            "machine_payload_decode profile=%s elf_origin=%s elf_semantics=%s status=%s pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_step_status_name(report->header_summary.step_status),
            report->header_summary.program_counter,
            report->header_summary.stack_pointer,
            report->header_summary.current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_payload_decode_append_format(
            &builder,
            "payload: kind=%s raw=0x%02x value=0x%02x known=%s name=%s total-bytes=%zu payload-bytes=%zu imm=%zu",
            machine_payload_decode_kind_name(report->payload_summary.kind),
            (unsigned int)report->payload_summary.raw_byte,
            (unsigned int)report->payload_summary.tag_value,
            report->payload_summary.is_known ? "yes" : "no",
            report->payload_summary.tag_name ? report->payload_summary.tag_name : "-",
            report->payload_summary.total_byte_count,
            report->payload_summary.payload_byte_count,
            report->payload_summary.immediate_value)) {
        free(builder.data);
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-127: out of memory building payload decode report dump");
        return 0;
    }

    for (payload_index = 0u; payload_index < report->payload_summary.payload_byte_count; ++payload_index) {
        if (!machine_payload_decode_append_format(
                &builder,
                "%s0x%02x",
                payload_index == 0u ? " bytes=[" : ",",
                (unsigned int)report->payload_summary.payload_bytes[payload_index])) {
            free(builder.data);
            machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-128: out of memory appending payload bytes");
            return 0;
        }
    }
    if (!machine_payload_decode_append_format(
            &builder,
            "%s\n",
            report->payload_summary.payload_byte_count > 0u ? "]" : " bytes=[]") ||
        !machine_payload_decode_append_format(&builder, "report_overview:\n") ||
        !machine_payload_decode_append_format(
            &builder,
            "  status=%s current-segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_step_status_name(report->header_summary.step_status),
            report->header_summary.current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.program_counter,
            report->header_summary.stack_pointer) ||
        !machine_payload_decode_append_format(
            &builder,
            "  elf_source: target=%s origin=%s semantics=%s\n",
            machine_elf_target_profile_name(
                report->file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics)) ||
        !machine_payload_decode_append_format(
            &builder,
            "  policy: profile=%s max-payload=%zu\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.max_payload_byte_count) ||
        !machine_payload_decode_append_format(
            &builder,
            "  payload: kind=%s raw=0x%02x value=0x%02x known=%s name=%s total-bytes=%zu payload-bytes=%zu imm=%zu",
            machine_payload_decode_kind_name(report->payload_summary.kind),
            (unsigned int)report->payload_summary.raw_byte,
            (unsigned int)report->payload_summary.tag_value,
            report->payload_summary.is_known ? "yes" : "no",
            report->payload_summary.tag_name ? report->payload_summary.tag_name : "-",
            report->payload_summary.total_byte_count,
            report->payload_summary.payload_byte_count,
            report->payload_summary.immediate_value)) {
        free(builder.data);
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-129: out of memory building payload decode report overview");
        return 0;
    }

    for (payload_index = 0u; payload_index < report->payload_summary.payload_byte_count; ++payload_index) {
        if (!machine_payload_decode_append_format(
                &builder,
                "%s0x%02x",
                payload_index == 0u ? " bytes=[" : ",",
                (unsigned int)report->payload_summary.payload_bytes[payload_index])) {
            free(builder.data);
            machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-130: out of memory appending report payload bytes");
            return 0;
        }
    }
    if (!machine_payload_decode_append_format(
            &builder,
            "%s\n",
            report->payload_summary.payload_byte_count > 0u ? "]" : " bytes=[]")) {
        free(builder.data);
        machine_payload_decode_set_error(error, 0, 0, "MACHINE-PAYLOAD-DECODE-131: out of memory terminating payload decode report");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_payload_decode_build_report_dump_from_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeReport report;
    int ok;

    machine_payload_decode_report_init(&report);
    ok = machine_payload_decode_build_report_from_file(source, &report, error) &&
        machine_payload_decode_dump_report(&report, out_text, error);
    machine_payload_decode_report_free(&report);
    return ok;
}

int machine_payload_decode_build_report_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeReport report;
    int ok;

    machine_payload_decode_report_init(&report);
    ok = machine_payload_decode_build_report_from_machine_decode_file(source, &report, error) &&
        machine_payload_decode_dump_report(&report, out_text, error);
    machine_payload_decode_report_free(&report);
    return ok;
}

int machine_payload_decode_build_report_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeReport payload_decode_report;
    int ok;

    machine_payload_decode_report_init(&payload_decode_report);
    ok = machine_payload_decode_build_report_from_machine_decode_report(report, &payload_decode_report, error) &&
        machine_payload_decode_dump_report(&payload_decode_report, out_text, error);
    machine_payload_decode_report_free(&payload_decode_report);
    return ok;
}

int machine_payload_decode_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeReport report;
    int ok;

    machine_payload_decode_report_init(&report);
    ok = machine_payload_decode_build_report_from_machine_step_file(source, &report, error) &&
        machine_payload_decode_dump_report(&report, out_text, error);
    machine_payload_decode_report_free(&report);
    return ok;
}

int machine_payload_decode_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeReport payload_decode_report;
    int ok;

    machine_payload_decode_report_init(&payload_decode_report);
    ok = machine_payload_decode_build_report_from_machine_step_report(report, &payload_decode_report, error) &&
        machine_payload_decode_dump_report(&payload_decode_report, out_text, error);
    machine_payload_decode_report_free(&payload_decode_report);
    return ok;
}

int machine_payload_decode_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeReport payload_decode_report;
    int ok;

    machine_payload_decode_report_init(&payload_decode_report);
    ok = machine_payload_decode_build_report_from_machine_ir_report(report, &payload_decode_report, error) &&
        machine_payload_decode_dump_report(&payload_decode_report, out_text, error);
    machine_payload_decode_report_free(&payload_decode_report);
    return ok;
}

int machine_payload_decode_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachinePayloadDecodeError *error) {
    MachinePayloadDecodeReport payload_decode_report;
    int ok;

    machine_payload_decode_report_init(&payload_decode_report);
    ok = machine_payload_decode_build_report_from_machine_ir_report_with_profile(
            report, profile, &payload_decode_report, error) &&
        machine_payload_decode_dump_report(&payload_decode_report, out_text, error);
    machine_payload_decode_report_free(&payload_decode_report);
    return ok;
}
