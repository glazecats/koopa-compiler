#include "machine/decode.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineDecodeStringBuilder;

static void machine_decode_set_error(MachineDecodeError *error, int line, int column, const char *fmt, ...);
static int machine_decode_append_format(MachineDecodeStringBuilder *builder, const char *fmt, ...);
static const char *machine_decode_op_tag_name(unsigned char tag_value);
static const char *machine_decode_terminator_tag_name(unsigned char tag_value);

static void machine_decode_set_error(MachineDecodeError *error, int line, int column, const char *fmt, ...) {
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

static int machine_decode_append_format(MachineDecodeStringBuilder *builder, const char *fmt, ...) {
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

static const char *machine_decode_op_tag_name(unsigned char tag_value) {
    switch ((MachineSelectOpKind)tag_value) {
        case MACHINE_SELECT_OP_COPY:
            return "copy";
        case MACHINE_SELECT_OP_MATERIALIZE_IMM:
            return "materialize-imm";
        case MACHINE_SELECT_OP_ALU:
            return "alu";
        case MACHINE_SELECT_OP_ALU_IMM:
            return "alu-imm";
        case MACHINE_SELECT_OP_CMP:
            return "cmp";
        case MACHINE_SELECT_OP_CMP_IMM:
            return "cmp-imm";
        case MACHINE_SELECT_OP_CALL:
            return "call";
        case MACHINE_SELECT_OP_CALL_IMM:
            return "call-imm";
        case MACHINE_SELECT_OP_CALL_SPILL:
            return "call-spill";
        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
            return "call-imm-spill";
        case MACHINE_SELECT_OP_CALL_VOID:
            return "call-void";
        case MACHINE_SELECT_OP_CALL_VOID_IMM:
            return "call-void-imm";
        case MACHINE_SELECT_OP_ADDR_LOCAL:
            return "addr-local";
        case MACHINE_SELECT_OP_ADDR_GLOBAL:
            return "addr-global";
        case MACHINE_SELECT_OP_LOAD_LOCAL:
            return "load-local";
        case MACHINE_SELECT_OP_STORE_LOCAL:
            return "store-local";
        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
            return "store-local-imm";
        case MACHINE_SELECT_OP_LOAD_GLOBAL:
            return "load-global";
        case MACHINE_SELECT_OP_STORE_GLOBAL:
            return "store-global";
        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
            return "store-global-imm";
        case MACHINE_SELECT_OP_LOAD_INDIRECT:
            return "load-indirect";
        case MACHINE_SELECT_OP_STORE_INDIRECT:
            return "store-indirect";
    }
    return NULL;
}

static const char *machine_decode_terminator_tag_name(unsigned char tag_value) {
    switch ((MachineLayoutTerminatorKind)tag_value) {
        case MACHINE_LAYOUT_TERM_RETURN:
            return "return";
        case MACHINE_LAYOUT_TERM_RETURN_IMM:
            return "return-imm";
        case MACHINE_LAYOUT_TERM_RETURN_SPILL:
            return "return-spill";
        case MACHINE_LAYOUT_TERM_FALLTHROUGH:
            return "fallthrough";
        case MACHINE_LAYOUT_TERM_JUMP:
            return "jump";
        case MACHINE_LAYOUT_TERM_BRANCH:
            return "branch";
        case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
            return "branch-fallthrough";
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            return "compare-branch";
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
            return "compare-branch-imm";
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            return "compare-branch-fallthrough";
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
            return "compare-branch-imm-fallthrough";
    }
    return NULL;
}

void machine_decode_file_init(MachineDecodeFile *decode_file) {
    if (!decode_file) {
        return;
    }
    machine_step_file_init(&decode_file->step_file);
    decode_file->tag_class = MACHINE_DECODE_TAG_NONE;
    decode_file->raw_byte = 0u;
    decode_file->tag_value = 0u;
    decode_file->is_known = 0;
}

void machine_decode_file_free(MachineDecodeFile *decode_file) {
    if (!decode_file) {
        return;
    }
    machine_step_file_free(&decode_file->step_file);
    machine_decode_file_init(decode_file);
}

void machine_decode_report_init(MachineDecodeReport *report) {
    if (!report) {
        return;
    }
    machine_decode_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    memset(&report->runtime_launch_summary, 0, sizeof(report->runtime_launch_summary));
    memset(&report->initial_stack_summary, 0, sizeof(report->initial_stack_summary));
    memset(&report->runtime_memory_summary, 0, sizeof(report->runtime_memory_summary));
    memset(&report->current_segment_summary, 0, sizeof(report->current_segment_summary));
    memset(&report->fetch_summary, 0, sizeof(report->fetch_summary));
    memset(&report->tag_summary, 0, sizeof(report->tag_summary));
}

void machine_decode_report_free(MachineDecodeReport *report) {
    if (!report) {
        return;
    }
    machine_decode_file_free(&report->file);
    machine_decode_report_init(report);
}

const char *machine_decode_tag_class_name(MachineDecodeTagClass tag_class) {
    switch (tag_class) {
        case MACHINE_DECODE_TAG_NONE:
            return "none";
        case MACHINE_DECODE_TAG_OP:
            return "op";
        case MACHINE_DECODE_TAG_TERMINATOR:
            return "terminator";
    }
    return "unknown";
}

int machine_decode_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineDecodeTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->opcode_tag_base = 0x10u;
    out_summary->terminator_tag_base = 0x80u;
    return 1;
}

int machine_decode_file_get_target_policy_summary(const MachineDecodeFile *decode_file,
    MachineDecodeTargetPolicySummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    return machine_decode_get_target_policy_summary(
        decode_file->step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_decode_verify_file(const MachineDecodeFile *decode_file,
    MachineDecodeError *error) {
    MachineStepFetchSummary fetch_summary;
    MachineDecodeTagSummary tag_summary;

    memset(&fetch_summary, 0, sizeof(fetch_summary));
    memset(&tag_summary, 0, sizeof(tag_summary));
    if (!decode_file) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-100: invalid decode file");
        return 0;
    }
    if (!machine_step_verify_file(&decode_file->step_file, NULL)) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-101: invalid step input");
        return 0;
    }
    if (!machine_step_file_get_fetch_summary(&decode_file->step_file, &fetch_summary) ||
        !machine_decode_file_get_tag_summary(decode_file, &tag_summary)) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-102: failed deriving decode facts");
        return 0;
    }
    if (decode_file->raw_byte != fetch_summary.byte_value) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-103: decode raw byte must match current fetch byte");
        return 0;
    }
    if (tag_summary.tag_class != decode_file->tag_class ||
        tag_summary.raw_byte != decode_file->raw_byte ||
        tag_summary.tag_value != decode_file->tag_value ||
        tag_summary.is_known != decode_file->is_known) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-104: decode tag summary mismatch");
        return 0;
    }
    return 1;
}

int machine_decode_clone_file(const MachineDecodeFile *source,
    MachineDecodeFile *out_clone,
    MachineDecodeError *error) {
    if (!source || !out_clone) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-105: invalid decode clone contract");
        return 0;
    }
    if (!machine_decode_verify_file(source, error)) {
        return 0;
    }

    machine_decode_file_free(out_clone);
    if (!machine_step_clone_file(&source->step_file, &out_clone->step_file, NULL)) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-106: failed cloning step input");
        machine_decode_file_free(out_clone);
        return 0;
    }
    out_clone->tag_class = source->tag_class;
    out_clone->raw_byte = source->raw_byte;
    out_clone->tag_value = source->tag_value;
    out_clone->is_known = source->is_known;
    return 1;
}

int machine_decode_file_get_summary(const MachineDecodeFile *decode_file,
    size_t *out_mapped_byte_count) {
    if (!decode_file) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = decode_file->step_file.launch_file.runtime_file.total_mapped_byte_count;
    }
    return 1;
}

int machine_decode_file_get_header_summary(const MachineDecodeFile *decode_file,
    MachineDecodeHeaderSummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    out_summary->target_profile =
        decode_file->step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile;
    out_summary->step_status = decode_file->step_file.status;
    out_summary->program_counter = decode_file->step_file.program_counter;
    out_summary->stack_pointer = decode_file->step_file.stack_pointer;
    out_summary->current_segment_index = decode_file->step_file.current_segment_index;
    out_summary->mapped_byte_count =
        decode_file->step_file.launch_file.runtime_file.total_mapped_byte_count;
    return 1;
}

int machine_decode_file_get_runtime_launch_summary(const MachineDecodeFile *decode_file,
    MachineRuntimeLaunchSummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    return machine_step_file_get_runtime_launch_summary(&decode_file->step_file, out_summary);
}

int machine_decode_file_get_initial_stack_summary(const MachineDecodeFile *decode_file,
    MachineRuntimeInitialStackSummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    return machine_step_file_get_initial_stack_summary(&decode_file->step_file, out_summary);
}

int machine_decode_file_get_runtime_memory_summary(const MachineDecodeFile *decode_file,
    MachineRuntimeMemorySummary *out_summary) {
    if (!decode_file || !out_summary) {
        return 0;
    }
    return machine_step_file_get_runtime_memory_summary(&decode_file->step_file, out_summary);
}

int machine_decode_file_get_current_segment_summary(const MachineDecodeFile *decode_file,
    size_t *out_segment_index,
    MachineRuntimeSegmentSummary *out_summary) {
    if (!decode_file) {
        return 0;
    }
    return machine_step_file_get_current_segment_summary(&decode_file->step_file, out_segment_index, out_summary);
}

int machine_decode_file_get_fetch_summary(const MachineDecodeFile *decode_file,
    MachineStepFetchSummary *out_summary) {
    if (!decode_file) {
        return 0;
    }
    return machine_step_file_get_fetch_summary(&decode_file->step_file, out_summary);
}

int machine_decode_file_get_tag_summary(const MachineDecodeFile *decode_file,
    MachineDecodeTagSummary *out_summary) {
    const char *tag_name = NULL;

    if (!decode_file || !out_summary) {
        return 0;
    }
    switch (decode_file->tag_class) {
        case MACHINE_DECODE_TAG_OP:
            tag_name = machine_decode_op_tag_name(decode_file->tag_value);
            break;
        case MACHINE_DECODE_TAG_TERMINATOR:
            tag_name = machine_decode_terminator_tag_name(decode_file->tag_value);
            break;
        case MACHINE_DECODE_TAG_NONE:
        default:
            tag_name = NULL;
            break;
    }
    out_summary->tag_class = decode_file->tag_class;
    out_summary->raw_byte = decode_file->raw_byte;
    out_summary->tag_value = decode_file->tag_value;
    out_summary->is_known = decode_file->is_known;
    out_summary->tag_name = tag_name;
    return 1;
}

int machine_decode_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error) {
    MachineStepFetchSummary fetch_summary;
    unsigned char raw_byte;
    MachineDecodeTagClass tag_class = MACHINE_DECODE_TAG_NONE;
    unsigned char tag_value = 0u;
    int is_known = 0;

    memset(&fetch_summary, 0, sizeof(fetch_summary));
    if (!step_file || !out_decode_file) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-107: invalid decode build contract");
        return 0;
    }
    if (!machine_step_verify_file(step_file, NULL) ||
        !machine_step_file_get_fetch_summary(step_file, &fetch_summary)) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-108: invalid step input");
        return 0;
    }

    raw_byte = fetch_summary.byte_value;
    if (raw_byte >= 0x10u && raw_byte < (unsigned char)(0x10u + MACHINE_SELECT_OP_STORE_GLOBAL_IMM + 1u)) {
        tag_class = MACHINE_DECODE_TAG_OP;
        tag_value = (unsigned char)(raw_byte - 0x10u);
        is_known = machine_decode_op_tag_name(tag_value) != NULL;
    } else if (raw_byte >= 0x80u &&
        raw_byte < (unsigned char)(0x80u + MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH + 1u)) {
        tag_class = MACHINE_DECODE_TAG_TERMINATOR;
        tag_value = (unsigned char)(raw_byte - 0x80u);
        is_known = machine_decode_terminator_tag_name(tag_value) != NULL;
    }

    machine_decode_file_free(out_decode_file);
    if (!machine_step_clone_file(step_file, &out_decode_file->step_file, NULL)) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-109: failed cloning step input");
        machine_decode_file_free(out_decode_file);
        return 0;
    }
    out_decode_file->tag_class = tag_class;
    out_decode_file->raw_byte = raw_byte;
    out_decode_file->tag_value = tag_value;
    out_decode_file->is_known = is_known;

    if (!machine_decode_verify_file(out_decode_file, error)) {
        machine_decode_file_free(out_decode_file);
        return 0;
    }
    return 1;
}

int machine_decode_build_from_machine_step_report(const MachineStepReport *report,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error) {
    const MachineStepFile *step_file = NULL;

    if (!report) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-110: invalid decode build-from-step-report contract");
        return 0;
    }
    if (!machine_step_report_get_file(report, &step_file) || !step_file) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-111: failed recovering step file from report");
        return 0;
    }
    return machine_decode_build_from_machine_step_file(step_file, out_decode_file, error);
}

int machine_decode_build_from_machine_launch_file(const MachineLaunchFile *launch_file,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_launch_file(launch_file, &step_file, NULL) &&
        machine_decode_build_from_machine_step_file(&step_file, out_decode_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-112: failed building decode from launch file");
    }
    machine_step_file_free(&step_file);
    return ok;
}

int machine_decode_build_from_machine_launch_report(const MachineLaunchReport *report,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_launch_report(report, &step_file, NULL) &&
        machine_decode_build_from_machine_step_file(&step_file, out_decode_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-113: failed building decode from launch report");
    }
    machine_step_file_free(&step_file);
    return ok;
}

int machine_decode_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_ir_report(report, &step_file, NULL) &&
        machine_decode_build_from_machine_step_file(&step_file, out_decode_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-114: failed building decode from machine-ir report");
    }
    machine_step_file_free(&step_file);
    return ok;
}

int machine_decode_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error) {
    MachineStepFile step_file;
    int ok;

    machine_step_file_init(&step_file);
    ok = machine_step_build_from_machine_ir_report_with_profile(report, profile, &step_file, NULL) &&
        machine_decode_build_from_machine_step_file(&step_file, out_decode_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-115: failed building profiled decode from machine-ir report");
    }
    machine_step_file_free(&step_file);
    return ok;
}

int machine_decode_build_report_from_file(const MachineDecodeFile *source,
    MachineDecodeReport *out_report,
    MachineDecodeError *error) {
    if (!source || !out_report) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-116: invalid decode report build contract");
        return 0;
    }
    if (!machine_decode_verify_file(source, error)) {
        return 0;
    }

    machine_decode_report_free(out_report);
    if (!machine_decode_clone_file(source, &out_report->file, error)) {
        machine_decode_report_free(out_report);
        return 0;
    }
    if (!machine_decode_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_decode_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_decode_file_get_runtime_launch_summary(&out_report->file, &out_report->runtime_launch_summary) ||
        !machine_decode_file_get_initial_stack_summary(&out_report->file, &out_report->initial_stack_summary) ||
        !machine_decode_file_get_runtime_memory_summary(&out_report->file, &out_report->runtime_memory_summary) ||
        !machine_decode_file_get_current_segment_summary(&out_report->file, NULL, &out_report->current_segment_summary) ||
        !machine_decode_file_get_fetch_summary(&out_report->file, &out_report->fetch_summary) ||
        !machine_decode_file_get_tag_summary(&out_report->file, &out_report->tag_summary)) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-117: failed summarizing decode report");
        machine_decode_report_free(out_report);
        return 0;
    }
    return 1;
}

int machine_decode_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineDecodeReport *out_report,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_step_file(source, &decode_file, error) &&
        machine_decode_build_report_from_file(&decode_file, out_report, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineDecodeReport *out_report,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_step_report(report, &decode_file, error) &&
        machine_decode_build_report_from_file(&decode_file, out_report, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_build_report_from_machine_launch_file(const MachineLaunchFile *source,
    MachineDecodeReport *out_report,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_launch_file(source, &decode_file, error) &&
        machine_decode_build_report_from_file(&decode_file, out_report, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_build_report_from_machine_launch_report(const MachineLaunchReport *report,
    MachineDecodeReport *out_report,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_launch_report(report, &decode_file, error) &&
        machine_decode_build_report_from_file(&decode_file, out_report, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineDecodeReport *out_report,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_ir_report(report, &decode_file, error) &&
        machine_decode_build_report_from_file(&decode_file, out_report, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineDecodeReport *out_report,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_ir_report_with_profile(report, profile, &decode_file, error) &&
        machine_decode_build_report_from_file(&decode_file, out_report, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_report_refresh(MachineDecodeReport *report,
    MachineDecodeError *error) {
    MachineDecodeReport refreshed_report;
    int ok;

    if (!report) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-118: invalid decode report refresh contract");
        return 0;
    }
    machine_decode_report_init(&refreshed_report);
    ok = machine_decode_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_decode_report_free(report);
        *report = refreshed_report;
    } else {
        machine_decode_report_free(&refreshed_report);
    }
    return ok;
}

int machine_decode_dump_file(const MachineDecodeFile *decode_file,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeStringBuilder builder;
    MachineDecodeHeaderSummary header_summary;
    MachineDecodeTagSummary tag_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&tag_summary, 0, sizeof(tag_summary));
    if (!decode_file || !out_text ||
        !machine_decode_verify_file(decode_file, error) ||
        !machine_decode_file_get_header_summary(decode_file, &header_summary) ||
        !machine_decode_file_get_tag_summary(decode_file, &tag_summary)) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-119: invalid decode dump contract");
        return 0;
    }

    if (!machine_decode_append_format(
            &builder,
            "machine_decode profile=%s status=%s pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_step_status_name(header_summary.step_status),
            header_summary.program_counter,
            header_summary.stack_pointer,
            header_summary.current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_decode_append_format(
            &builder,
            "tag: class=%s raw=0x%02x value=0x%02x known=%s name=%s\n",
            machine_decode_tag_class_name(tag_summary.tag_class),
            (unsigned int)tag_summary.raw_byte,
            (unsigned int)tag_summary.tag_value,
            tag_summary.is_known ? "yes" : "no",
            tag_summary.tag_name ? tag_summary.tag_name : "-")) {
        free(builder.data);
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-120: out of memory building decode dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_decode_build_dump_from_file(const MachineDecodeFile *source,
    char **out_text,
    MachineDecodeError *error) {
    return machine_decode_dump_file(source, out_text, error);
}

int machine_decode_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_step_file(source, &decode_file, error) &&
        machine_decode_dump_file(&decode_file, out_text, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_step_report(report, &decode_file, error) &&
        machine_decode_dump_file(&decode_file, out_text, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_build_dump_from_machine_launch_file(const MachineLaunchFile *source,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_launch_file(source, &decode_file, error) &&
        machine_decode_dump_file(&decode_file, out_text, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_build_dump_from_machine_launch_report(const MachineLaunchReport *report,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_launch_report(report, &decode_file, error) &&
        machine_decode_dump_file(&decode_file, out_text, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_ir_report(report, &decode_file, error) &&
        machine_decode_dump_file(&decode_file, out_text, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeFile decode_file;
    int ok;

    machine_decode_file_init(&decode_file);
    ok = machine_decode_build_from_machine_ir_report_with_profile(report, profile, &decode_file, error) &&
        machine_decode_dump_file(&decode_file, out_text, error);
    machine_decode_file_free(&decode_file);
    return ok;
}

int machine_decode_report_get_summary(const MachineDecodeReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_decode_report_get_overview_artifact(const MachineDecodeReport *report,
    MachineDecodeReportOverviewArtifact *out_artifact) {
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
    out_artifact->tag_summary = &report->tag_summary;
    return 1;
}

int machine_decode_report_get_file(const MachineDecodeReport *report,
    const MachineDecodeFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_decode_report_get_step_file(const MachineDecodeReport *report,
    const MachineStepFile **out_step_file) {
    if (!report || !out_step_file) {
        return 0;
    }
    *out_step_file = &report->file.step_file;
    return 1;
}

int machine_decode_report_get_header_summary_artifact(const MachineDecodeReport *report,
    const MachineDecodeHeaderSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->header_summary;
    return 1;
}

int machine_decode_report_get_target_policy_summary_artifact(const MachineDecodeReport *report,
    const MachineDecodeTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_decode_report_get_runtime_launch_summary_artifact(const MachineDecodeReport *report,
    const MachineRuntimeLaunchSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->runtime_launch_summary;
    return 1;
}

int machine_decode_report_get_initial_stack_summary_artifact(const MachineDecodeReport *report,
    const MachineRuntimeInitialStackSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->initial_stack_summary;
    return 1;
}

int machine_decode_report_get_runtime_memory_summary_artifact(const MachineDecodeReport *report,
    const MachineRuntimeMemorySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->runtime_memory_summary;
    return 1;
}

int machine_decode_report_get_current_segment_summary_artifact(const MachineDecodeReport *report,
    const MachineRuntimeSegmentSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->current_segment_summary;
    return 1;
}

int machine_decode_report_get_fetch_summary_artifact(const MachineDecodeReport *report,
    const MachineStepFetchSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->fetch_summary;
    return 1;
}

int machine_decode_report_get_tag_summary_artifact(const MachineDecodeReport *report,
    const MachineDecodeTagSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->tag_summary;
    return 1;
}

int machine_decode_report_overview_artifact_get_runtime_launch_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->runtime_launch_summary;
    return 1;
}

int machine_decode_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->initial_stack_summary;
    return 1;
}

int machine_decode_report_overview_artifact_get_runtime_memory_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeMemorySummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->runtime_memory_summary;
    return 1;
}

int machine_decode_report_overview_artifact_get_current_segment_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeSegmentSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->current_segment_summary;
    return 1;
}

int machine_decode_report_overview_artifact_get_fetch_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineStepFetchSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->fetch_summary;
    return 1;
}

int machine_decode_report_overview_artifact_get_tag_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineDecodeTagSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->tag_summary;
    return 1;
}

int machine_decode_dump_report(const MachineDecodeReport *report,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeStringBuilder builder;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text) {
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-121: invalid decode report dump contract");
        return 0;
    }

    if (!machine_decode_append_format(
            &builder,
            "machine_decode profile=%s status=%s pc=0x%zx sp=0x%zx current_segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_step_status_name(report->header_summary.step_status),
            report->header_summary.program_counter,
            report->header_summary.stack_pointer,
            report->header_summary.current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_decode_append_format(
            &builder,
            "tag: class=%s raw=0x%02x value=0x%02x known=%s name=%s\n",
            machine_decode_tag_class_name(report->tag_summary.tag_class),
            (unsigned int)report->tag_summary.raw_byte,
            (unsigned int)report->tag_summary.tag_value,
            report->tag_summary.is_known ? "yes" : "no",
            report->tag_summary.tag_name ? report->tag_summary.tag_name : "-") ||
        !machine_decode_append_format(&builder, "report_overview:\n") ||
        !machine_decode_append_format(
            &builder,
            "  status=%s current-segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_step_status_name(report->header_summary.step_status),
            report->header_summary.current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.program_counter,
            report->header_summary.stack_pointer) ||
        !machine_decode_append_format(
            &builder,
            "  policy: profile=%s op-base=0x%zx term-base=0x%zx\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.opcode_tag_base,
            report->target_policy_summary.terminator_tag_base) ||
        !machine_decode_append_format(
            &builder,
            "  runtime-launch: pc=0x%zx sp=0x%zx stack-segment=%zu\n",
            report->runtime_launch_summary.entry_virtual_address,
            report->runtime_launch_summary.initial_stack_pointer,
            report->runtime_launch_summary.stack_segment_index) ||
        !machine_decode_append_format(
            &builder,
            "  fetch: vaddr=0x%zx mem-offset=0x%zx byte=0x%02x segment=%zu %s\n",
            report->fetch_summary.byte_virtual_address,
            report->fetch_summary.byte_memory_offset,
            (unsigned int)report->fetch_summary.byte_value,
            report->fetch_summary.segment_index,
            report->fetch_summary.segment_name ? report->fetch_summary.segment_name : "") ||
        !machine_decode_append_format(
            &builder,
            "  tag: class=%s raw=0x%02x value=0x%02x known=%s name=%s\n",
            machine_decode_tag_class_name(report->tag_summary.tag_class),
            (unsigned int)report->tag_summary.raw_byte,
            (unsigned int)report->tag_summary.tag_value,
            report->tag_summary.is_known ? "yes" : "no",
            report->tag_summary.tag_name ? report->tag_summary.tag_name : "-")) {
        free(builder.data);
        machine_decode_set_error(error, 0, 0, "MACHINE-DECODE-122: out of memory building decode report dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_decode_build_report_dump_from_file(const MachineDecodeFile *source,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeReport report;
    int ok;

    machine_decode_report_init(&report);
    ok = machine_decode_build_report_from_file(source, &report, error) &&
        machine_decode_dump_report(&report, out_text, error);
    machine_decode_report_free(&report);
    return ok;
}

int machine_decode_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeReport report;
    int ok;

    machine_decode_report_init(&report);
    ok = machine_decode_build_report_from_machine_step_file(source, &report, error) &&
        machine_decode_dump_report(&report, out_text, error);
    machine_decode_report_free(&report);
    return ok;
}

int machine_decode_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeReport decode_report;
    int ok;

    machine_decode_report_init(&decode_report);
    ok = machine_decode_build_report_from_machine_step_report(report, &decode_report, error) &&
        machine_decode_dump_report(&decode_report, out_text, error);
    machine_decode_report_free(&decode_report);
    return ok;
}

int machine_decode_build_report_dump_from_machine_launch_file(const MachineLaunchFile *source,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeReport report;
    int ok;

    machine_decode_report_init(&report);
    ok = machine_decode_build_report_from_machine_launch_file(source, &report, error) &&
        machine_decode_dump_report(&report, out_text, error);
    machine_decode_report_free(&report);
    return ok;
}

int machine_decode_build_report_dump_from_machine_launch_report(const MachineLaunchReport *report,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeReport decode_report;
    int ok;

    machine_decode_report_init(&decode_report);
    ok = machine_decode_build_report_from_machine_launch_report(report, &decode_report, error) &&
        machine_decode_dump_report(&decode_report, out_text, error);
    machine_decode_report_free(&decode_report);
    return ok;
}

int machine_decode_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeReport decode_report;
    int ok;

    machine_decode_report_init(&decode_report);
    ok = machine_decode_build_report_from_machine_ir_report(report, &decode_report, error) &&
        machine_decode_dump_report(&decode_report, out_text, error);
    machine_decode_report_free(&decode_report);
    return ok;
}

int machine_decode_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineDecodeError *error) {
    MachineDecodeReport decode_report;
    int ok;

    machine_decode_report_init(&decode_report);
    ok = machine_decode_build_report_from_machine_ir_report_with_profile(report, profile, &decode_report, error) &&
        machine_decode_dump_report(&decode_report, out_text, error);
    machine_decode_report_free(&decode_report);
    return ok;
}
