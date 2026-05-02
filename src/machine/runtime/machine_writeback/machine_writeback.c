#include "machine/writeback.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineWritebackStringBuilder;

static void machine_writeback_set_error(MachineWritebackError *error, int line, int column, const char *fmt, ...);
static int machine_writeback_append_format(MachineWritebackStringBuilder *builder, const char *fmt, ...);
static int machine_writeback_append_targets(MachineWritebackStringBuilder *builder,
    const MachineWritebackSummary *writeback_summary);
static int machine_writeback_append_return_immediate(MachineWritebackStringBuilder *builder,
    const MachineWritebackSummary *writeback_summary);

static void machine_writeback_set_error(MachineWritebackError *error, int line, int column, const char *fmt, ...) {
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

static int machine_writeback_append_format(MachineWritebackStringBuilder *builder, const char *fmt, ...) {
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

static int machine_writeback_append_targets(MachineWritebackStringBuilder *builder,
    const MachineWritebackSummary *writeback_summary) {
    if (!builder || !writeback_summary) {
        return 0;
    }
    if (!writeback_summary->has_primary_target_block) {
        return machine_writeback_append_format(builder, " targets=[]");
    }
    if (!writeback_summary->has_secondary_target_block) {
        return machine_writeback_append_format(
            builder,
            " targets=[%zu]",
            writeback_summary->primary_target_block_index);
    }
    return machine_writeback_append_format(
        builder,
        " targets=[%zu,%zu]",
        writeback_summary->primary_target_block_index,
        writeback_summary->secondary_target_block_index);
}

static int machine_writeback_append_return_immediate(MachineWritebackStringBuilder *builder,
    const MachineWritebackSummary *writeback_summary) {
    if (!builder || !writeback_summary) {
        return 0;
    }
    if (!writeback_summary->has_return_immediate) {
        return machine_writeback_append_format(builder, " return-imm=-");
    }
    return machine_writeback_append_format(
        builder,
        " return-imm=%zu",
        writeback_summary->return_immediate);
}

void machine_writeback_file_init(MachineWritebackFile *writeback_file) {
    if (!writeback_file) {
        return;
    }
    machine_mutation_file_init(&writeback_file->mutation_file);
    writeback_file->resolution_kind = MACHINE_WRITEBACK_RESOLUTION_NONE;
    writeback_file->commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_NONE;
}

void machine_writeback_file_free(MachineWritebackFile *writeback_file) {
    if (!writeback_file) {
        return;
    }
    machine_mutation_file_free(&writeback_file->mutation_file);
    machine_writeback_file_init(writeback_file);
}

void machine_writeback_report_init(MachineWritebackReport *report) {
    if (!report) {
        return;
    }
    machine_writeback_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_mutation_report_init(&report->mutation_report);
    memset(&report->writeback_summary, 0, sizeof(report->writeback_summary));
}

void machine_writeback_report_free(MachineWritebackReport *report) {
    if (!report) {
        return;
    }
    machine_mutation_report_free(&report->mutation_report);
    machine_writeback_file_free(&report->file);
    machine_writeback_report_init(report);
}

const char *machine_writeback_resolution_kind_name(MachineWritebackResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_WRITEBACK_RESOLUTION_NONE:
            return "none";
        case MACHINE_WRITEBACK_RESOLUTION_COMMITTED_NO_OP:
            return "committed-no-op";
        case MACHINE_WRITEBACK_RESOLUTION_DEFERRED_REGISTER_WRITEBACK:
            return "deferred-register-writeback";
        case MACHINE_WRITEBACK_RESOLUTION_DEFERRED_LOCAL_WRITEBACK:
            return "deferred-local-writeback";
        case MACHINE_WRITEBACK_RESOLUTION_DEFERRED_GLOBAL_WRITEBACK:
            return "deferred-global-writeback";
        case MACHINE_WRITEBACK_RESOLUTION_DEFERRED_CALL_WRITEBACK:
            return "deferred-call-writeback";
        case MACHINE_WRITEBACK_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_WRITEBACK_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_writeback_commit_kind_name(MachineWritebackCommitKind commit_kind) {
    switch (commit_kind) {
        case MACHINE_WRITEBACK_COMMIT_KIND_NONE:
            return "none";
        case MACHINE_WRITEBACK_COMMIT_KIND_NO_OP:
            return "no-op";
        case MACHINE_WRITEBACK_COMMIT_KIND_REGISTER:
            return "register";
        case MACHINE_WRITEBACK_COMMIT_KIND_LOCAL_SLOT:
            return "local-slot";
        case MACHINE_WRITEBACK_COMMIT_KIND_GLOBAL_SLOT:
            return "global-slot";
        case MACHINE_WRITEBACK_COMMIT_KIND_CALL:
            return "call";
    }
    return "unknown";
}

int machine_writeback_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineWritebackTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->commits_no_op_halt = 1;
    out_summary->defers_register_writeback = 1;
    out_summary->defers_slot_writeback = 1;
    out_summary->defers_call_writeback = 1;
    return 1;
}

int machine_writeback_file_get_target_policy_summary(const MachineWritebackFile *writeback_file,
    MachineWritebackTargetPolicySummary *out_summary) {
    if (!writeback_file || !out_summary) {
        return 0;
    }
    return machine_writeback_get_target_policy_summary(
        writeback_file->mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_writeback_file_get_summary(const MachineWritebackFile *writeback_file,
    size_t *out_mapped_byte_count) {
    if (!writeback_file) {
        return 0;
    }
    return machine_mutation_file_get_summary(&writeback_file->mutation_file, out_mapped_byte_count);
}

int machine_writeback_file_get_header_summary(const MachineWritebackFile *writeback_file,
    MachineWritebackHeaderSummary *out_summary) {
    MachineMutationHeaderSummary mutation_header_summary;

    memset(&mutation_header_summary, 0, sizeof(mutation_header_summary));
    if (!writeback_file || !out_summary ||
        !machine_mutation_file_get_header_summary(&writeback_file->mutation_file, &mutation_header_summary)) {
        return 0;
    }
    out_summary->target_profile = mutation_header_summary.target_profile;
    out_summary->mutation_resolution_kind = writeback_file->mutation_file.resolution_kind;
    out_summary->origin_step_status = mutation_header_summary.origin_step_status;
    out_summary->origin_program_counter = mutation_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = mutation_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = mutation_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = mutation_header_summary.mapped_byte_count;
    return 1;
}

int machine_writeback_file_get_mutation_summary(const MachineWritebackFile *writeback_file,
    MachineMutationSummary *out_summary) {
    if (!writeback_file || !out_summary) {
        return 0;
    }
    return machine_mutation_file_get_mutation_summary(&writeback_file->mutation_file, out_summary);
}

int machine_writeback_file_get_writeback_summary(const MachineWritebackFile *writeback_file,
    MachineWritebackSummary *out_summary) {
    MachineMutationSummary mutation_summary;

    memset(&mutation_summary, 0, sizeof(mutation_summary));
    if (!writeback_file || !out_summary ||
        !machine_writeback_file_get_mutation_summary(writeback_file, &mutation_summary)) {
        return 0;
    }
    out_summary->resolution_kind = writeback_file->resolution_kind;
    out_summary->commit_kind = writeback_file->commit_kind;
    out_summary->mutation_resolution_kind = mutation_summary.resolution_kind;
    out_summary->mutation_effect_kind = mutation_summary.effect_kind;
    out_summary->state_resolution_kind = mutation_summary.state_resolution_kind;
    out_summary->transition_resolution_kind = mutation_summary.transition_resolution_kind;
    out_summary->action_kind = mutation_summary.action_kind;
    out_summary->payload_kind = mutation_summary.payload_kind;
    out_summary->tag_class = mutation_summary.tag_class;
    out_summary->raw_byte = mutation_summary.raw_byte;
    out_summary->tag_value = mutation_summary.tag_value;
    out_summary->is_known = mutation_summary.is_known;
    out_summary->tag_name = mutation_summary.tag_name;
    out_summary->instruction_byte_count = mutation_summary.instruction_byte_count;
    out_summary->has_state = mutation_summary.has_state;
    out_summary->state_status = mutation_summary.state_status;
    out_summary->program_counter = mutation_summary.program_counter;
    out_summary->stack_pointer = mutation_summary.stack_pointer;
    out_summary->has_current_fetch = mutation_summary.has_current_fetch;
    out_summary->current_segment_index = mutation_summary.current_segment_index;
    out_summary->current_byte = mutation_summary.current_byte;
    out_summary->current_byte_memory_offset = mutation_summary.current_byte_memory_offset;
    out_summary->has_primary_target_block = mutation_summary.has_primary_target_block;
    out_summary->primary_target_block_index = mutation_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = mutation_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = mutation_summary.secondary_target_block_index;
    out_summary->has_return_immediate = mutation_summary.has_return_immediate;
    out_summary->return_immediate = mutation_summary.return_immediate;
    return 1;
}

int machine_writeback_verify_file(const MachineWritebackFile *writeback_file,
    MachineWritebackError *error) {
    MachineMutationSummary mutation_summary;
    MachineWritebackResolutionKind expected_resolution_kind;
    MachineWritebackCommitKind expected_commit_kind;

    memset(&mutation_summary, 0, sizeof(mutation_summary));
    expected_resolution_kind = MACHINE_WRITEBACK_RESOLUTION_NONE;
    expected_commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_NONE;
    if (!writeback_file) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-100: invalid writeback file");
        return 0;
    }
    if (!machine_mutation_verify_file(&writeback_file->mutation_file, NULL) ||
        !machine_writeback_file_get_mutation_summary(writeback_file, &mutation_summary)) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-101: invalid mutation input");
        return 0;
    }

    switch (writeback_file->mutation_file.resolution_kind) {
        case MACHINE_MUTATION_RESOLUTION_NO_MUTATION:
            expected_resolution_kind = MACHINE_WRITEBACK_RESOLUTION_COMMITTED_NO_OP;
            expected_commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_NO_OP;
            break;
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_REGISTER_RESULT:
            expected_resolution_kind = MACHINE_WRITEBACK_RESOLUTION_DEFERRED_REGISTER_WRITEBACK;
            expected_commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_REGISTER;
            break;
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_LOCAL_SLOT:
            expected_resolution_kind = MACHINE_WRITEBACK_RESOLUTION_DEFERRED_LOCAL_WRITEBACK;
            expected_commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_LOCAL_SLOT;
            break;
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_GLOBAL_SLOT:
            expected_resolution_kind = MACHINE_WRITEBACK_RESOLUTION_DEFERRED_GLOBAL_WRITEBACK;
            expected_commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_GLOBAL_SLOT;
            break;
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_CALL_EFFECT:
            expected_resolution_kind = MACHINE_WRITEBACK_RESOLUTION_DEFERRED_CALL_WRITEBACK;
            expected_commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_CALL;
            break;
        case MACHINE_MUTATION_RESOLUTION_BLOCKED_ON_CONTROL:
            expected_resolution_kind = MACHINE_WRITEBACK_RESOLUTION_BLOCKED_ON_CONTROL;
            expected_commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_NONE;
            break;
        case MACHINE_MUTATION_RESOLUTION_BLOCKED_UNSUPPORTED:
            expected_resolution_kind = MACHINE_WRITEBACK_RESOLUTION_BLOCKED_UNSUPPORTED;
            expected_commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_NONE;
            break;
        case MACHINE_MUTATION_RESOLUTION_NONE:
        default:
            machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-102: invalid mutation resolution kind");
            return 0;
    }

    if (writeback_file->resolution_kind != expected_resolution_kind ||
        writeback_file->commit_kind != expected_commit_kind) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-103: writeback classification mismatch");
        return 0;
    }
    return 1;
}

int machine_writeback_clone_file(const MachineWritebackFile *source,
    MachineWritebackFile *out_clone,
    MachineWritebackError *error) {
    if (!source || !out_clone) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-104: invalid clone contract");
        return 0;
    }
    if (!machine_writeback_verify_file(source, error)) {
        return 0;
    }

    machine_writeback_file_free(out_clone);
    if (!machine_mutation_clone_file(&source->mutation_file, &out_clone->mutation_file, NULL)) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-105: failed cloning mutation input");
        machine_writeback_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->commit_kind = source->commit_kind;
    return 1;
}

int machine_writeback_build_from_machine_mutation_file(const MachineMutationFile *mutation_file,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error) {
    MachineWritebackResolutionKind resolution_kind;
    MachineWritebackCommitKind commit_kind;

    resolution_kind = MACHINE_WRITEBACK_RESOLUTION_NONE;
    commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_NONE;
    if (!mutation_file || !out_writeback_file) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-106: invalid build contract");
        return 0;
    }
    if (!machine_mutation_verify_file(mutation_file, NULL)) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-107: invalid mutation input");
        return 0;
    }

    switch (mutation_file->resolution_kind) {
        case MACHINE_MUTATION_RESOLUTION_NO_MUTATION:
            resolution_kind = MACHINE_WRITEBACK_RESOLUTION_COMMITTED_NO_OP;
            commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_NO_OP;
            break;
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_REGISTER_RESULT:
            resolution_kind = MACHINE_WRITEBACK_RESOLUTION_DEFERRED_REGISTER_WRITEBACK;
            commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_REGISTER;
            break;
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_LOCAL_SLOT:
            resolution_kind = MACHINE_WRITEBACK_RESOLUTION_DEFERRED_LOCAL_WRITEBACK;
            commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_LOCAL_SLOT;
            break;
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_GLOBAL_SLOT:
            resolution_kind = MACHINE_WRITEBACK_RESOLUTION_DEFERRED_GLOBAL_WRITEBACK;
            commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_GLOBAL_SLOT;
            break;
        case MACHINE_MUTATION_RESOLUTION_DEFERRED_CALL_EFFECT:
            resolution_kind = MACHINE_WRITEBACK_RESOLUTION_DEFERRED_CALL_WRITEBACK;
            commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_CALL;
            break;
        case MACHINE_MUTATION_RESOLUTION_BLOCKED_ON_CONTROL:
            resolution_kind = MACHINE_WRITEBACK_RESOLUTION_BLOCKED_ON_CONTROL;
            commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_NONE;
            break;
        case MACHINE_MUTATION_RESOLUTION_BLOCKED_UNSUPPORTED:
            resolution_kind = MACHINE_WRITEBACK_RESOLUTION_BLOCKED_UNSUPPORTED;
            commit_kind = MACHINE_WRITEBACK_COMMIT_KIND_NONE;
            break;
        case MACHINE_MUTATION_RESOLUTION_NONE:
        default:
            machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-108: unsupported mutation resolution");
            return 0;
    }

    machine_writeback_file_free(out_writeback_file);
    if (!machine_mutation_clone_file(mutation_file, &out_writeback_file->mutation_file, NULL)) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-109: failed cloning mutation input");
        machine_writeback_file_free(out_writeback_file);
        return 0;
    }
    out_writeback_file->resolution_kind = resolution_kind;
    out_writeback_file->commit_kind = commit_kind;

    if (!machine_writeback_verify_file(out_writeback_file, error)) {
        machine_writeback_file_free(out_writeback_file);
        return 0;
    }
    return 1;
}

int machine_writeback_build_from_machine_mutation_report(const MachineMutationReport *report,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error) {
    const MachineMutationFile *mutation_file = NULL;

    if (!report) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-110: invalid mutation-report build contract");
        return 0;
    }
    if (!machine_mutation_report_get_file(report, &mutation_file) || !mutation_file) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-111: failed recovering mutation file from report");
        return 0;
    }
    return machine_writeback_build_from_machine_mutation_file(mutation_file, out_writeback_file, error);
}

#define MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineWritebackFile *out_writeback_file, MachineWritebackError *error) { \
    MachineMutationFile mutation_file; \
    int ok; \
    machine_mutation_file_init(&mutation_file); \
    ok = builder(source, &mutation_file, NULL) && \
        machine_writeback_build_from_machine_mutation_file(&mutation_file, out_writeback_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-112: failed building writeback wrapper"); \
    } \
    machine_mutation_file_free(&mutation_file); \
    return ok; \
}

MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_state_file,
    MachineStateFile, machine_mutation_build_from_machine_state_file)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_state_report,
    MachineStateReport, machine_mutation_build_from_machine_state_report)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_transition_file,
    MachineTransitionFile, machine_mutation_build_from_machine_transition_file)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_transition_report,
    MachineTransitionReport, machine_mutation_build_from_machine_transition_report)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_interp_file,
    MachineInterpFile, machine_mutation_build_from_machine_interp_file)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_interp_report,
    MachineInterpReport, machine_mutation_build_from_machine_interp_report)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_mutation_build_from_machine_payload_decode_file)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_mutation_build_from_machine_payload_decode_report)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_decode_file,
    MachineDecodeFile, machine_mutation_build_from_machine_decode_file)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_decode_report,
    MachineDecodeReport, machine_mutation_build_from_machine_decode_report)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_step_file,
    MachineStepFile, machine_mutation_build_from_machine_step_file)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_step_report,
    MachineStepReport, machine_mutation_build_from_machine_step_report)
MACHINE_WRITEBACK_DEFINE_BUILD_FROM_WRAPPER(machine_writeback_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_mutation_build_from_machine_ir_report)

int machine_writeback_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error) {
    MachineMutationFile mutation_file;
    int ok;

    machine_mutation_file_init(&mutation_file);
    ok = machine_mutation_build_from_machine_ir_report_with_profile(report, profile, &mutation_file, NULL) &&
        machine_writeback_build_from_machine_mutation_file(&mutation_file, out_writeback_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-113: failed building profiled writeback wrapper");
    }
    machine_mutation_file_free(&mutation_file);
    return ok;
}

int machine_writeback_build_report_from_file(const MachineWritebackFile *source,
    MachineWritebackReport *out_report,
    MachineWritebackError *error) {
    if (!source || !out_report) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-114: invalid report build contract");
        return 0;
    }
    if (!machine_writeback_verify_file(source, error)) {
        return 0;
    }

    machine_writeback_report_free(out_report);
    if (!machine_writeback_clone_file(source, &out_report->file, error)) {
        machine_writeback_report_free(out_report);
        return 0;
    }
    if (!machine_writeback_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_writeback_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_mutation_build_report_from_file(&out_report->file.mutation_file, &out_report->mutation_report, NULL) ||
        !machine_writeback_file_get_writeback_summary(&out_report->file, &out_report->writeback_summary)) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-115: failed summarizing writeback report");
        machine_writeback_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineWritebackReport *out_report, MachineWritebackError *error) { \
    MachineWritebackFile writeback_file; \
    int ok; \
    machine_writeback_file_init(&writeback_file); \
    ok = builder(source, &writeback_file, error) && \
        machine_writeback_build_report_from_file(&writeback_file, out_report, error); \
    machine_writeback_file_free(&writeback_file); \
    return ok; \
}

MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_mutation_file,
    MachineMutationFile, machine_writeback_build_from_machine_mutation_file)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_mutation_report,
    MachineMutationReport, machine_writeback_build_from_machine_mutation_report)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_state_file,
    MachineStateFile, machine_writeback_build_from_machine_state_file)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_state_report,
    MachineStateReport, machine_writeback_build_from_machine_state_report)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_transition_file,
    MachineTransitionFile, machine_writeback_build_from_machine_transition_file)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_transition_report,
    MachineTransitionReport, machine_writeback_build_from_machine_transition_report)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_interp_file,
    MachineInterpFile, machine_writeback_build_from_machine_interp_file)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_interp_report,
    MachineInterpReport, machine_writeback_build_from_machine_interp_report)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_writeback_build_from_machine_payload_decode_file)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_writeback_build_from_machine_payload_decode_report)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_decode_file,
    MachineDecodeFile, machine_writeback_build_from_machine_decode_file)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_decode_report,
    MachineDecodeReport, machine_writeback_build_from_machine_decode_report)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_step_file,
    MachineStepFile, machine_writeback_build_from_machine_step_file)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_step_report,
    MachineStepReport, machine_writeback_build_from_machine_step_report)
MACHINE_WRITEBACK_DEFINE_REPORT_FROM_WRAPPER(machine_writeback_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_writeback_build_from_machine_ir_report)

int machine_writeback_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineWritebackReport *out_report,
    MachineWritebackError *error) {
    MachineWritebackFile writeback_file;
    int ok;

    machine_writeback_file_init(&writeback_file);
    ok = machine_writeback_build_from_machine_ir_report_with_profile(report, profile, &writeback_file, error) &&
        machine_writeback_build_report_from_file(&writeback_file, out_report, error);
    machine_writeback_file_free(&writeback_file);
    return ok;
}

int machine_writeback_report_refresh(MachineWritebackReport *report,
    MachineWritebackError *error) {
    MachineWritebackReport refreshed_report;
    int ok;

    if (!report) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-116: invalid report refresh contract");
        return 0;
    }
    machine_writeback_report_init(&refreshed_report);
    ok = machine_writeback_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_writeback_report_free(report);
        *report = refreshed_report;
    } else {
        machine_writeback_report_free(&refreshed_report);
    }
    return ok;
}

int machine_writeback_dump_file(const MachineWritebackFile *writeback_file,
    char **out_text,
    MachineWritebackError *error) {
    MachineWritebackStringBuilder builder;
    MachineWritebackHeaderSummary header_summary;
    MachineWritebackSummary writeback_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&writeback_summary, 0, sizeof(writeback_summary));
    if (!writeback_file || !out_text ||
        !machine_writeback_verify_file(writeback_file, error) ||
        !machine_writeback_file_get_header_summary(writeback_file, &header_summary) ||
        !machine_writeback_file_get_writeback_summary(writeback_file, &writeback_summary)) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-117: invalid dump contract");
        return 0;
    }

    if (!machine_writeback_append_format(
            &builder,
            "machine_writeback profile=%s mutation=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_mutation_resolution_kind_name(header_summary.mutation_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_writeback_append_format(
            &builder,
            "writeback: resolution=%s commit=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu has-state=%s status=%s pc=%s",
            machine_writeback_resolution_kind_name(writeback_summary.resolution_kind),
            machine_writeback_commit_kind_name(writeback_summary.commit_kind),
            machine_mutation_resolution_kind_name(writeback_summary.mutation_resolution_kind),
            machine_mutation_effect_kind_name(writeback_summary.mutation_effect_kind),
            machine_transition_resolution_kind_name(writeback_summary.transition_resolution_kind),
            machine_interp_action_kind_name(writeback_summary.action_kind),
            (unsigned int)writeback_summary.raw_byte,
            (unsigned int)writeback_summary.tag_value,
            writeback_summary.is_known ? "yes" : "no",
            writeback_summary.tag_name ? writeback_summary.tag_name : "-",
            writeback_summary.instruction_byte_count,
            writeback_summary.has_state ? "yes" : "no",
            writeback_summary.has_state ? machine_step_status_name(writeback_summary.state_status) : "-",
            writeback_summary.has_state ? "" : "-")) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-118: out of memory building dump");
        return 0;
    }
    if (writeback_summary.has_state &&
        !machine_writeback_append_format(&builder, "0x%zx", writeback_summary.program_counter)) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-119: out of memory appending pc");
        return 0;
    }
    if (!machine_writeback_append_format(
            &builder,
            " current-segment=%s",
            writeback_summary.has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-120: out of memory appending current segment");
        return 0;
    }
    if (writeback_summary.has_current_fetch &&
        !machine_writeback_append_format(&builder, "%zu", writeback_summary.current_segment_index)) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-121: out of memory appending current segment value");
        return 0;
    }
    if (!machine_writeback_append_format(
            &builder,
            " current-byte=%s",
            writeback_summary.has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-122: out of memory appending current byte");
        return 0;
    }
    if (writeback_summary.has_current_fetch &&
        !machine_writeback_append_format(&builder, "0x%02x", (unsigned int)writeback_summary.current_byte)) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-123: out of memory appending current byte value");
        return 0;
    }
    if (!machine_writeback_append_targets(&builder, &writeback_summary) ||
        !machine_writeback_append_return_immediate(&builder, &writeback_summary) ||
        !machine_writeback_append_format(&builder, "\n")) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-124: out of memory terminating dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_writeback_build_dump_from_file(const MachineWritebackFile *source,
    char **out_text,
    MachineWritebackError *error) {
    return machine_writeback_dump_file(source, out_text, error);
}

#define MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineWritebackError *error) { \
    MachineWritebackFile writeback_file; \
    int ok; \
    machine_writeback_file_init(&writeback_file); \
    ok = builder(source, &writeback_file, error) && \
        machine_writeback_dump_file(&writeback_file, out_text, error); \
    machine_writeback_file_free(&writeback_file); \
    return ok; \
}

MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_mutation_file,
    MachineMutationFile, machine_writeback_build_from_machine_mutation_file)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_mutation_report,
    MachineMutationReport, machine_writeback_build_from_machine_mutation_report)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_state_file,
    MachineStateFile, machine_writeback_build_from_machine_state_file)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_state_report,
    MachineStateReport, machine_writeback_build_from_machine_state_report)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_transition_file,
    MachineTransitionFile, machine_writeback_build_from_machine_transition_file)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_transition_report,
    MachineTransitionReport, machine_writeback_build_from_machine_transition_report)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_interp_file,
    MachineInterpFile, machine_writeback_build_from_machine_interp_file)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_interp_report,
    MachineInterpReport, machine_writeback_build_from_machine_interp_report)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_writeback_build_from_machine_payload_decode_file)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_writeback_build_from_machine_payload_decode_report)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_decode_file,
    MachineDecodeFile, machine_writeback_build_from_machine_decode_file)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_decode_report,
    MachineDecodeReport, machine_writeback_build_from_machine_decode_report)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_step_file,
    MachineStepFile, machine_writeback_build_from_machine_step_file)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_step_report,
    MachineStepReport, machine_writeback_build_from_machine_step_report)
MACHINE_WRITEBACK_DEFINE_DUMP_FROM_WRAPPER(machine_writeback_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_writeback_build_from_machine_ir_report)

int machine_writeback_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineWritebackError *error) {
    MachineWritebackFile writeback_file;
    int ok;

    machine_writeback_file_init(&writeback_file);
    ok = machine_writeback_build_from_machine_ir_report_with_profile(report, profile, &writeback_file, error) &&
        machine_writeback_dump_file(&writeback_file, out_text, error);
    machine_writeback_file_free(&writeback_file);
    return ok;
}

int machine_writeback_report_get_summary(const MachineWritebackReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_writeback_report_get_overview_artifact(const MachineWritebackReport *report,
    MachineWritebackReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->mutation_report = &report->mutation_report;
    out_artifact->writeback_summary = &report->writeback_summary;
    return 1;
}

int machine_writeback_report_get_file(const MachineWritebackReport *report,
    const MachineWritebackFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_writeback_report_get_mutation_file(const MachineWritebackReport *report,
    const MachineMutationFile **out_mutation_file) {
    if (!report || !out_mutation_file) {
        return 0;
    }
    *out_mutation_file = &report->file.mutation_file;
    return 1;
}

int machine_writeback_report_get_mutation_report(const MachineWritebackReport *report,
    const MachineMutationReport **out_mutation_report) {
    if (!report || !out_mutation_report) {
        return 0;
    }
    *out_mutation_report = &report->mutation_report;
    return 1;
}

#define MACHINE_WRITEBACK_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineWritebackReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_WRITEBACK_DEFINE_REPORT_ARTIFACT_GETTER(machine_writeback_report_get_header_summary_artifact,
    header_summary, MachineWritebackHeaderSummary)
MACHINE_WRITEBACK_DEFINE_REPORT_ARTIFACT_GETTER(machine_writeback_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineWritebackTargetPolicySummary)
MACHINE_WRITEBACK_DEFINE_REPORT_ARTIFACT_GETTER(machine_writeback_report_get_writeback_summary_artifact,
    writeback_summary, MachineWritebackSummary)

int machine_writeback_report_overview_artifact_get_mutation_report(
    const MachineWritebackReportOverviewArtifact *artifact,
    const MachineMutationReport **out_mutation_report) {
    if (!artifact || !out_mutation_report) {
        return 0;
    }
    *out_mutation_report = artifact->mutation_report;
    return 1;
}

int machine_writeback_report_overview_artifact_get_writeback_summary_artifact(
    const MachineWritebackReportOverviewArtifact *artifact,
    const MachineWritebackSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->writeback_summary;
    return 1;
}

int machine_writeback_dump_report(const MachineWritebackReport *report,
    char **out_text,
    MachineWritebackError *error) {
    MachineWritebackStringBuilder builder;
    const MachineWritebackSummary *writeback_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_writeback_report_get_writeback_summary_artifact(report, &writeback_summary) ||
        !writeback_summary) {
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-125: invalid report dump contract");
        return 0;
    }

    if (!machine_writeback_append_format(
            &builder,
            "machine_writeback profile=%s mutation=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_mutation_resolution_kind_name(report->header_summary.mutation_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_writeback_append_format(
            &builder,
            "writeback: resolution=%s commit=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu has-state=%s status=%s pc=%s",
            machine_writeback_resolution_kind_name(writeback_summary->resolution_kind),
            machine_writeback_commit_kind_name(writeback_summary->commit_kind),
            machine_mutation_resolution_kind_name(writeback_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(writeback_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(writeback_summary->transition_resolution_kind),
            machine_interp_action_kind_name(writeback_summary->action_kind),
            (unsigned int)writeback_summary->raw_byte,
            (unsigned int)writeback_summary->tag_value,
            writeback_summary->is_known ? "yes" : "no",
            writeback_summary->tag_name ? writeback_summary->tag_name : "-",
            writeback_summary->instruction_byte_count,
            writeback_summary->has_state ? "yes" : "no",
            writeback_summary->has_state ? machine_step_status_name(writeback_summary->state_status) : "-",
            writeback_summary->has_state ? "" : "-")) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-126: out of memory building report dump");
        return 0;
    }
    if (writeback_summary->has_state &&
        !machine_writeback_append_format(&builder, "0x%zx", writeback_summary->program_counter)) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-127: out of memory appending report pc");
        return 0;
    }
    if (!machine_writeback_append_format(
            &builder,
            " current-segment=%s",
            writeback_summary->has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-128: out of memory appending report current segment");
        return 0;
    }
    if (writeback_summary->has_current_fetch &&
        !machine_writeback_append_format(&builder, "%zu", writeback_summary->current_segment_index)) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-129: out of memory appending report current segment value");
        return 0;
    }
    if (!machine_writeback_append_format(
            &builder,
            " current-byte=%s",
            writeback_summary->has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-130: out of memory appending report current byte");
        return 0;
    }
    if (writeback_summary->has_current_fetch &&
        !machine_writeback_append_format(&builder, "0x%02x", (unsigned int)writeback_summary->current_byte)) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-131: out of memory appending report current byte value");
        return 0;
    }
    if (!machine_writeback_append_targets(&builder, writeback_summary) ||
        !machine_writeback_append_return_immediate(&builder, writeback_summary) ||
        !machine_writeback_append_format(&builder, "\nreport_overview:\n") ||
        !machine_writeback_append_format(
            &builder,
            "  origin: mutation=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_mutation_resolution_kind_name(report->header_summary.mutation_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer) ||
        !machine_writeback_append_format(
            &builder,
            "  policy: profile=%s no-op=%s register=%s slot=%s call=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.commits_no_op_halt ? "yes" : "no",
            report->target_policy_summary.defers_register_writeback ? "yes" : "no",
            report->target_policy_summary.defers_slot_writeback ? "yes" : "no",
            report->target_policy_summary.defers_call_writeback ? "yes" : "no") ||
        !machine_writeback_append_format(
            &builder,
            "  writeback: resolution=%s commit=%s has-state=%s status=%s pc=%s",
            machine_writeback_resolution_kind_name(writeback_summary->resolution_kind),
            machine_writeback_commit_kind_name(writeback_summary->commit_kind),
            writeback_summary->has_state ? "yes" : "no",
            writeback_summary->has_state ? machine_step_status_name(writeback_summary->state_status) : "-",
            writeback_summary->has_state ? "" : "-")) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-132: out of memory building report overview");
        return 0;
    }
    if (writeback_summary->has_state &&
        !machine_writeback_append_format(&builder, "0x%zx", writeback_summary->program_counter)) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-133: out of memory appending report overview pc");
        return 0;
    }
    if (!machine_writeback_append_targets(&builder, writeback_summary) ||
        !machine_writeback_append_return_immediate(&builder, writeback_summary) ||
        !machine_writeback_append_format(&builder, "\n")) {
        free(builder.data);
        machine_writeback_set_error(error, 0, 0, "MACHINE-WRITEBACK-134: out of memory terminating report overview");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_writeback_build_report_dump_from_file(const MachineWritebackFile *source,
    char **out_text,
    MachineWritebackError *error) {
    MachineWritebackReport report;
    int ok;

    machine_writeback_report_init(&report);
    ok = machine_writeback_build_report_from_file(source, &report, error) &&
        machine_writeback_dump_report(&report, out_text, error);
    machine_writeback_report_free(&report);
    return ok;
}

#define MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineWritebackError *error) { \
    MachineWritebackReport report; \
    int ok; \
    machine_writeback_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_writeback_dump_report(&report, out_text, error); \
    machine_writeback_report_free(&report); \
    return ok; \
}

MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_mutation_file,
    MachineMutationFile, machine_writeback_build_report_from_machine_mutation_file)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_mutation_report,
    MachineMutationReport, machine_writeback_build_report_from_machine_mutation_report)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_state_file,
    MachineStateFile, machine_writeback_build_report_from_machine_state_file)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_state_report,
    MachineStateReport, machine_writeback_build_report_from_machine_state_report)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_transition_file,
    MachineTransitionFile, machine_writeback_build_report_from_machine_transition_file)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_transition_report,
    MachineTransitionReport, machine_writeback_build_report_from_machine_transition_report)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_interp_file,
    MachineInterpFile, machine_writeback_build_report_from_machine_interp_file)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_interp_report,
    MachineInterpReport, machine_writeback_build_report_from_machine_interp_report)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_writeback_build_report_from_machine_payload_decode_file)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_writeback_build_report_from_machine_payload_decode_report)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_decode_file,
    MachineDecodeFile, machine_writeback_build_report_from_machine_decode_file)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_decode_report,
    MachineDecodeReport, machine_writeback_build_report_from_machine_decode_report)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_writeback_build_report_from_machine_step_file)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_writeback_build_report_from_machine_step_report)
MACHINE_WRITEBACK_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_writeback_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_writeback_build_report_from_machine_ir_report)

int machine_writeback_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineWritebackError *error) {
    MachineWritebackReport report_file;
    int ok;

    machine_writeback_report_init(&report_file);
    ok = machine_writeback_build_report_from_machine_ir_report_with_profile(report, profile, &report_file, error) &&
        machine_writeback_dump_report(&report_file, out_text, error);
    machine_writeback_report_free(&report_file);
    return ok;
}
