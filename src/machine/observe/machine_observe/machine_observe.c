#include "machine/observe.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineObserveStringBuilder;

static void machine_observe_set_error(MachineObserveError *error, int line, int column, const char *fmt, ...);
static int machine_observe_append_format(MachineObserveStringBuilder *builder, const char *fmt, ...);
static int machine_observe_append_targets(MachineObserveStringBuilder *builder,
    const MachineObserveSummary *observe_summary);
static int machine_observe_append_return_immediate(MachineObserveStringBuilder *builder,
    const MachineObserveSummary *observe_summary);
static int machine_observe_append_payload_bytes(MachineObserveStringBuilder *builder,
    const MachineObserveSummary *observe_summary);
static int machine_observe_append_immediate_hint(MachineObserveStringBuilder *builder,
    const MachineObserveSummary *observe_summary);

static void machine_observe_set_error(MachineObserveError *error, int line, int column, const char *fmt, ...) {
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

static int machine_observe_append_format(MachineObserveStringBuilder *builder, const char *fmt, ...) {
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

static int machine_observe_append_targets(MachineObserveStringBuilder *builder,
    const MachineObserveSummary *observe_summary) {
    if (!builder || !observe_summary) {
        return 0;
    }
    if (!observe_summary->has_primary_target_block) {
        return machine_observe_append_format(builder, " targets=[]");
    }
    if (!observe_summary->has_secondary_target_block) {
        return machine_observe_append_format(
            builder,
            " targets=[%zu]",
            observe_summary->primary_target_block_index);
    }
    return machine_observe_append_format(
        builder,
        " targets=[%zu,%zu]",
        observe_summary->primary_target_block_index,
        observe_summary->secondary_target_block_index);
}

static int machine_observe_append_return_immediate(MachineObserveStringBuilder *builder,
    const MachineObserveSummary *observe_summary) {
    if (!builder || !observe_summary) {
        return 0;
    }
    if (!observe_summary->has_return_immediate) {
        return machine_observe_append_format(builder, " return-imm=-");
    }
    return machine_observe_append_format(builder, " return-imm=%zu", observe_summary->return_immediate);
}

static int machine_observe_append_payload_bytes(MachineObserveStringBuilder *builder,
    const MachineObserveSummary *observe_summary) {
    size_t index;

    if (!builder || !observe_summary) {
        return 0;
    }
    if (!machine_observe_append_format(builder, " payload=[")) {
        return 0;
    }
    for (index = 0u; index < observe_summary->payload_byte_count; ++index) {
        if (!machine_observe_append_format(
                builder,
                "%s0x%02x",
                index == 0u ? "" : ",",
                (unsigned int)observe_summary->payload_bytes[index])) {
            return 0;
        }
    }
    return machine_observe_append_format(builder, "]");
}

static int machine_observe_append_immediate_hint(MachineObserveStringBuilder *builder,
    const MachineObserveSummary *observe_summary) {
    if (!builder || !observe_summary) {
        return 0;
    }
    if (!observe_summary->has_immediate_hint) {
        return machine_observe_append_format(builder, " imm=-");
    }
    return machine_observe_append_format(builder, " imm=%zu", observe_summary->immediate_hint);
}

void machine_observe_file_init(MachineObserveFile *observe_file) {
    if (!observe_file) {
        return;
    }
    machine_apply_file_init(&observe_file->apply_file);
    observe_file->resolution_kind = MACHINE_OBSERVE_RESOLUTION_NONE;
    observe_file->observe_kind = MACHINE_OBSERVE_KIND_NONE;
    observe_file->has_observed_state = 0;
    observe_file->observed_status = MACHINE_STEP_STATUS_READY;
    observe_file->program_counter = 0u;
    observe_file->stack_pointer = 0u;
    observe_file->has_current_fetch = 0;
    observe_file->current_segment_index = 0u;
    observe_file->current_byte = 0u;
    observe_file->current_byte_memory_offset = 0u;
}

void machine_observe_file_free(MachineObserveFile *observe_file) {
    if (!observe_file) {
        return;
    }
    machine_apply_file_free(&observe_file->apply_file);
    machine_observe_file_init(observe_file);
}

void machine_observe_report_init(MachineObserveReport *report) {
    if (!report) {
        return;
    }
    machine_observe_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_apply_report_init(&report->apply_report);
    memset(&report->observe_summary, 0, sizeof(report->observe_summary));
    memset(&report->current_fetch_summary, 0, sizeof(report->current_fetch_summary));
}

void machine_observe_report_free(MachineObserveReport *report) {
    if (!report) {
        return;
    }
    machine_apply_report_free(&report->apply_report);
    machine_observe_file_free(&report->file);
    machine_observe_report_init(report);
}

const char *machine_observe_resolution_kind_name(MachineObserveResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_OBSERVE_RESOLUTION_NONE:
            return "none";
        case MACHINE_OBSERVE_RESOLUTION_EXACT_STATE:
            return "exact-state";
        case MACHINE_OBSERVE_RESOLUTION_PREVIEW_STATE:
            return "preview-state";
        case MACHINE_OBSERVE_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_OBSERVE_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_observe_kind_name(MachineObserveKind observe_kind) {
    switch (observe_kind) {
        case MACHINE_OBSERVE_KIND_NONE:
            return "none";
        case MACHINE_OBSERVE_KIND_STATE:
            return "state";
    }
    return "unknown";
}

int machine_observe_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineObserveTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->surfaces_exact_state = 1;
    out_summary->surfaces_preview_state = 1;
    return 1;
}

int machine_observe_file_get_target_policy_summary(const MachineObserveFile *observe_file,
    MachineObserveTargetPolicySummary *out_summary) {
    if (!observe_file || !out_summary) {
        return 0;
    }
    return machine_observe_get_target_policy_summary(
        observe_file->apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_observe_file_get_summary(const MachineObserveFile *observe_file,
    size_t *out_mapped_byte_count) {
    if (!observe_file) {
        return 0;
    }
    return machine_apply_file_get_summary(&observe_file->apply_file, out_mapped_byte_count);
}

int machine_observe_file_get_header_summary(const MachineObserveFile *observe_file,
    MachineObserveHeaderSummary *out_summary) {
    MachineApplyHeaderSummary apply_header_summary;

    memset(&apply_header_summary, 0, sizeof(apply_header_summary));
    if (!observe_file || !out_summary ||
        !machine_apply_file_get_header_summary(&observe_file->apply_file, &apply_header_summary)) {
        return 0;
    }
    out_summary->target_profile = apply_header_summary.target_profile;
    out_summary->apply_resolution_kind = observe_file->apply_file.resolution_kind;
    out_summary->origin_step_status = apply_header_summary.origin_step_status;
    out_summary->origin_program_counter = apply_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = apply_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = apply_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = apply_header_summary.mapped_byte_count;
    return 1;
}

int machine_observe_file_get_apply_summary(const MachineObserveFile *observe_file,
    MachineApplySummary *out_summary) {
    if (!observe_file || !out_summary) {
        return 0;
    }
    return machine_apply_file_get_apply_summary(&observe_file->apply_file, out_summary);
}

int machine_observe_file_get_observe_summary(const MachineObserveFile *observe_file,
    MachineObserveSummary *out_summary) {
    MachineApplySummary apply_summary;

    memset(&apply_summary, 0, sizeof(apply_summary));
    if (!observe_file || !out_summary ||
        !machine_observe_file_get_apply_summary(observe_file, &apply_summary)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->resolution_kind = observe_file->resolution_kind;
    out_summary->observe_kind = observe_file->observe_kind;
    out_summary->apply_resolution_kind = apply_summary.resolution_kind;
    out_summary->apply_kind = apply_summary.apply_kind;
    out_summary->commit_resolution_kind = apply_summary.commit_resolution_kind;
    out_summary->commit_kind = apply_summary.commit_kind;
    out_summary->writeback_resolution_kind = apply_summary.writeback_resolution_kind;
    out_summary->writeback_commit_kind = apply_summary.writeback_commit_kind;
    out_summary->mutation_resolution_kind = apply_summary.mutation_resolution_kind;
    out_summary->mutation_effect_kind = apply_summary.mutation_effect_kind;
    out_summary->state_resolution_kind = apply_summary.state_resolution_kind;
    out_summary->transition_resolution_kind = apply_summary.transition_resolution_kind;
    out_summary->action_kind = apply_summary.action_kind;
    out_summary->payload_kind = apply_summary.payload_kind;
    out_summary->tag_class = apply_summary.tag_class;
    out_summary->raw_byte = apply_summary.raw_byte;
    out_summary->tag_value = apply_summary.tag_value;
    out_summary->is_known = apply_summary.is_known;
    out_summary->tag_name = apply_summary.tag_name;
    out_summary->instruction_byte_count = apply_summary.instruction_byte_count;
    out_summary->payload_byte_count = apply_summary.payload_byte_count;
    memcpy(out_summary->payload_bytes, apply_summary.payload_bytes, sizeof(out_summary->payload_bytes));
    out_summary->has_immediate_hint = apply_summary.has_immediate_hint;
    out_summary->immediate_hint = apply_summary.immediate_hint;
    out_summary->is_exact_state = observe_file->resolution_kind == MACHINE_OBSERVE_RESOLUTION_EXACT_STATE;
    out_summary->has_observed_state = observe_file->has_observed_state;
    out_summary->observed_status = observe_file->observed_status;
    out_summary->program_counter = observe_file->program_counter;
    out_summary->stack_pointer = observe_file->stack_pointer;
    out_summary->has_current_fetch = observe_file->has_current_fetch;
    out_summary->current_segment_index = observe_file->current_segment_index;
    out_summary->current_byte = observe_file->current_byte;
    out_summary->current_byte_memory_offset = observe_file->current_byte_memory_offset;
    out_summary->has_primary_target_block = apply_summary.has_primary_target_block;
    out_summary->primary_target_block_index = apply_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = apply_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = apply_summary.secondary_target_block_index;
    out_summary->has_return_immediate = apply_summary.has_return_immediate;
    out_summary->return_immediate = apply_summary.return_immediate;
    return 1;
}

int machine_observe_file_get_current_fetch_summary(const MachineObserveFile *observe_file,
    MachineObserveCurrentFetchSummary *out_summary) {
    MachineRuntimeSegmentSummary segment_summary;
    const MachineRuntimeSegment *segment = NULL;

    memset(&segment_summary, 0, sizeof(segment_summary));
    if (!observe_file || !observe_file->has_current_fetch) {
        return 0;
    }
    if (!machine_runtime_file_get_segment(
            &observe_file->apply_file.commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file,
            observe_file->current_segment_index,
            &segment) ||
        !segment ||
        !machine_runtime_segment_get_summary(segment, &segment_summary)) {
        return 0;
    }
    if (!out_summary) {
        return 1;
    }
    out_summary->byte_virtual_address = observe_file->program_counter;
    out_summary->byte_memory_offset = observe_file->current_byte_memory_offset;
    out_summary->segment_index = observe_file->current_segment_index;
    out_summary->segment_name = segment_summary.name;
    out_summary->byte_value = observe_file->current_byte;
    return 1;
}

int machine_observe_verify_file(const MachineObserveFile *observe_file,
    MachineObserveError *error) {
    MachineApplySummary apply_summary;

    memset(&apply_summary, 0, sizeof(apply_summary));
    if (!observe_file) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-100: invalid observe file");
        return 0;
    }
    if (!machine_apply_verify_file(&observe_file->apply_file, NULL) ||
        !machine_observe_file_get_apply_summary(observe_file, &apply_summary)) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-101: invalid apply input");
        return 0;
    }

    switch (observe_file->resolution_kind) {
        case MACHINE_OBSERVE_RESOLUTION_EXACT_STATE:
            if (observe_file->apply_file.resolution_kind != MACHINE_APPLY_RESOLUTION_APPLIED_STATE ||
                observe_file->observe_kind != MACHINE_OBSERVE_KIND_STATE ||
                !observe_file->has_observed_state ||
                !apply_summary.has_applied_state ||
                observe_file->observed_status != apply_summary.applied_status ||
                observe_file->program_counter != apply_summary.applied_program_counter ||
                observe_file->stack_pointer != apply_summary.applied_stack_pointer ||
                observe_file->has_current_fetch != apply_summary.has_applied_fetch ||
                (apply_summary.has_applied_fetch &&
                    (observe_file->current_segment_index != apply_summary.applied_segment_index ||
                        observe_file->current_byte != apply_summary.applied_byte ||
                        observe_file->current_byte_memory_offset != apply_summary.applied_byte_memory_offset)) ||
                !machine_observe_file_get_current_fetch_summary(observe_file, NULL) != !apply_summary.has_applied_fetch) {
                machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-102: invalid exact-state observation");
                return 0;
            }
            break;
        case MACHINE_OBSERVE_RESOLUTION_PREVIEW_STATE:
            if ((observe_file->apply_file.resolution_kind != MACHINE_APPLY_RESOLUTION_PENDING_REGISTER_APPLICATION &&
                    observe_file->apply_file.resolution_kind != MACHINE_APPLY_RESOLUTION_PENDING_LOCAL_APPLICATION &&
                    observe_file->apply_file.resolution_kind != MACHINE_APPLY_RESOLUTION_PENDING_GLOBAL_APPLICATION &&
                    observe_file->apply_file.resolution_kind != MACHINE_APPLY_RESOLUTION_PENDING_CALL_APPLICATION) ||
                observe_file->observe_kind != MACHINE_OBSERVE_KIND_STATE ||
                !observe_file->has_observed_state ||
                !apply_summary.has_preview_state ||
                observe_file->observed_status != apply_summary.preview_status ||
                observe_file->program_counter != apply_summary.preview_program_counter ||
                observe_file->stack_pointer != apply_summary.preview_stack_pointer ||
                observe_file->has_current_fetch != apply_summary.has_preview_fetch ||
                (apply_summary.has_preview_fetch &&
                    (observe_file->current_segment_index != apply_summary.preview_segment_index ||
                        observe_file->current_byte != apply_summary.preview_byte ||
                        observe_file->current_byte_memory_offset != apply_summary.preview_byte_memory_offset)) ||
                !machine_observe_file_get_current_fetch_summary(observe_file, NULL) != !apply_summary.has_preview_fetch) {
                machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-103: invalid preview-state observation");
                return 0;
            }
            break;
        case MACHINE_OBSERVE_RESOLUTION_BLOCKED_ON_CONTROL:
            if (observe_file->apply_file.resolution_kind != MACHINE_APPLY_RESOLUTION_BLOCKED_ON_CONTROL ||
                observe_file->observe_kind != MACHINE_OBSERVE_KIND_NONE ||
                observe_file->has_observed_state) {
                machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-104: invalid blocked-control observation");
                return 0;
            }
            break;
        case MACHINE_OBSERVE_RESOLUTION_BLOCKED_UNSUPPORTED:
            if (observe_file->apply_file.resolution_kind != MACHINE_APPLY_RESOLUTION_BLOCKED_UNSUPPORTED ||
                observe_file->observe_kind != MACHINE_OBSERVE_KIND_NONE ||
                observe_file->has_observed_state) {
                machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-105: invalid blocked-unsupported observation");
                return 0;
            }
            break;
        case MACHINE_OBSERVE_RESOLUTION_NONE:
        default:
            machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-106: invalid observe resolution kind");
            return 0;
    }

    return 1;
}

int machine_observe_clone_file(const MachineObserveFile *source,
    MachineObserveFile *out_clone,
    MachineObserveError *error) {
    if (!source || !out_clone) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-107: invalid clone contract");
        return 0;
    }
    if (!machine_observe_verify_file(source, error)) {
        return 0;
    }

    machine_observe_file_free(out_clone);
    if (!machine_apply_clone_file(&source->apply_file, &out_clone->apply_file, NULL)) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-108: failed cloning apply input");
        machine_observe_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->observe_kind = source->observe_kind;
    out_clone->has_observed_state = source->has_observed_state;
    out_clone->observed_status = source->observed_status;
    out_clone->program_counter = source->program_counter;
    out_clone->stack_pointer = source->stack_pointer;
    out_clone->has_current_fetch = source->has_current_fetch;
    out_clone->current_segment_index = source->current_segment_index;
    out_clone->current_byte = source->current_byte;
    out_clone->current_byte_memory_offset = source->current_byte_memory_offset;
    return 1;
}

int machine_observe_build_from_machine_apply_file(const MachineApplyFile *apply_file,
    MachineObserveFile *out_observe_file,
    MachineObserveError *error) {
    if (!apply_file || !out_observe_file) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-109: invalid build contract");
        return 0;
    }
    if (!machine_apply_verify_file(apply_file, NULL)) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-110: invalid apply input");
        return 0;
    }

    machine_observe_file_free(out_observe_file);
    if (!machine_apply_clone_file(apply_file, &out_observe_file->apply_file, NULL)) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-111: failed cloning apply input");
        machine_observe_file_free(out_observe_file);
        return 0;
    }

    switch (apply_file->resolution_kind) {
        case MACHINE_APPLY_RESOLUTION_APPLIED_STATE:
            out_observe_file->resolution_kind = MACHINE_OBSERVE_RESOLUTION_EXACT_STATE;
            out_observe_file->observe_kind = MACHINE_OBSERVE_KIND_STATE;
            out_observe_file->has_observed_state = 1;
            out_observe_file->observed_status = apply_file->applied_status;
            out_observe_file->program_counter = apply_file->applied_program_counter;
            out_observe_file->stack_pointer = apply_file->applied_stack_pointer;
            out_observe_file->has_current_fetch = apply_file->has_applied_fetch;
            out_observe_file->current_segment_index = apply_file->applied_segment_index;
            out_observe_file->current_byte = apply_file->applied_byte;
            out_observe_file->current_byte_memory_offset = apply_file->applied_byte_memory_offset;
            break;
        case MACHINE_APPLY_RESOLUTION_PENDING_REGISTER_APPLICATION:
        case MACHINE_APPLY_RESOLUTION_PENDING_LOCAL_APPLICATION:
        case MACHINE_APPLY_RESOLUTION_PENDING_GLOBAL_APPLICATION:
        case MACHINE_APPLY_RESOLUTION_PENDING_CALL_APPLICATION:
            out_observe_file->resolution_kind = MACHINE_OBSERVE_RESOLUTION_PREVIEW_STATE;
            out_observe_file->observe_kind = MACHINE_OBSERVE_KIND_STATE;
            out_observe_file->has_observed_state = 1;
            out_observe_file->observed_status = apply_file->preview_status;
            out_observe_file->program_counter = apply_file->preview_program_counter;
            out_observe_file->stack_pointer = apply_file->preview_stack_pointer;
            out_observe_file->has_current_fetch = apply_file->has_preview_fetch;
            out_observe_file->current_segment_index = apply_file->preview_segment_index;
            out_observe_file->current_byte = apply_file->preview_byte;
            out_observe_file->current_byte_memory_offset = apply_file->preview_byte_memory_offset;
            break;
        case MACHINE_APPLY_RESOLUTION_BLOCKED_ON_CONTROL:
            out_observe_file->resolution_kind = MACHINE_OBSERVE_RESOLUTION_BLOCKED_ON_CONTROL;
            out_observe_file->observe_kind = MACHINE_OBSERVE_KIND_NONE;
            break;
        case MACHINE_APPLY_RESOLUTION_BLOCKED_UNSUPPORTED:
            out_observe_file->resolution_kind = MACHINE_OBSERVE_RESOLUTION_BLOCKED_UNSUPPORTED;
            out_observe_file->observe_kind = MACHINE_OBSERVE_KIND_NONE;
            break;
        case MACHINE_APPLY_RESOLUTION_NONE:
        default:
            machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-112: unsupported apply resolution");
            machine_observe_file_free(out_observe_file);
            return 0;
    }

    if (!machine_observe_verify_file(out_observe_file, error)) {
        machine_observe_file_free(out_observe_file);
        return 0;
    }
    return 1;
}

int machine_observe_build_from_machine_apply_report(const MachineApplyReport *report,
    MachineObserveFile *out_observe_file,
    MachineObserveError *error) {
    const MachineApplyFile *apply_file = NULL;

    if (!report) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-113: invalid build-from-apply-report contract");
        return 0;
    }
    if (!machine_apply_report_get_file(report, &apply_file) || !apply_file) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-114: failed recovering apply file from report");
        return 0;
    }
    return machine_observe_build_from_machine_apply_file(apply_file, out_observe_file, error);
}

#define MACHINE_OBSERVE_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineObserveFile *out_observe_file, MachineObserveError *error) { \
    MachineApplyFile apply_file; \
    int ok; \
    machine_apply_file_init(&apply_file); \
    ok = builder(source, &apply_file, NULL) && \
        machine_observe_build_from_machine_apply_file(&apply_file, out_observe_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-115: failed building observe wrapper"); \
    } \
    machine_apply_file_free(&apply_file); \
    return ok; \
}

MACHINE_OBSERVE_DEFINE_BUILD_FROM_WRAPPER(machine_observe_build_from_machine_commit_file,
    MachineCommitFile, machine_apply_build_from_machine_commit_file)
MACHINE_OBSERVE_DEFINE_BUILD_FROM_WRAPPER(machine_observe_build_from_machine_commit_report,
    MachineCommitReport, machine_apply_build_from_machine_commit_report)
MACHINE_OBSERVE_DEFINE_BUILD_FROM_WRAPPER(machine_observe_build_from_machine_step_file,
    MachineStepFile, machine_apply_build_from_machine_step_file)
MACHINE_OBSERVE_DEFINE_BUILD_FROM_WRAPPER(machine_observe_build_from_machine_step_report,
    MachineStepReport, machine_apply_build_from_machine_step_report)
MACHINE_OBSERVE_DEFINE_BUILD_FROM_WRAPPER(machine_observe_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_apply_build_from_machine_ir_report)

int machine_observe_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineObserveFile *out_observe_file,
    MachineObserveError *error) {
    MachineApplyFile apply_file;
    int ok;

    machine_apply_file_init(&apply_file);
    ok = machine_apply_build_from_machine_ir_report_with_profile(report, profile, &apply_file, NULL) &&
        machine_observe_build_from_machine_apply_file(&apply_file, out_observe_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-116: failed building profiled observe wrapper");
    }
    machine_apply_file_free(&apply_file);
    return ok;
}

int machine_observe_build_report_from_file(const MachineObserveFile *source,
    MachineObserveReport *out_report,
    MachineObserveError *error) {
    if (!source || !out_report) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-117: invalid report build contract");
        return 0;
    }
    if (!machine_observe_verify_file(source, error)) {
        return 0;
    }

    machine_observe_report_free(out_report);
    if (!machine_observe_clone_file(source, &out_report->file, error)) {
        machine_observe_report_free(out_report);
        return 0;
    }
    if (!machine_observe_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_observe_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_apply_build_report_from_file(&out_report->file.apply_file, &out_report->apply_report, NULL) ||
        !machine_observe_file_get_observe_summary(&out_report->file, &out_report->observe_summary)) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-118: failed summarizing observe report");
        machine_observe_report_free(out_report);
        return 0;
    }
    if (out_report->file.has_current_fetch &&
        !machine_observe_file_get_current_fetch_summary(&out_report->file, &out_report->current_fetch_summary)) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-119: failed summarizing observed fetch");
        machine_observe_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_OBSERVE_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineObserveReport *out_report, MachineObserveError *error) { \
    MachineObserveFile observe_file; \
    int ok; \
    machine_observe_file_init(&observe_file); \
    ok = builder(source, &observe_file, error) && \
        machine_observe_build_report_from_file(&observe_file, out_report, error); \
    machine_observe_file_free(&observe_file); \
    return ok; \
}

MACHINE_OBSERVE_DEFINE_REPORT_FROM_WRAPPER(machine_observe_build_report_from_machine_apply_file,
    MachineApplyFile, machine_observe_build_from_machine_apply_file)
MACHINE_OBSERVE_DEFINE_REPORT_FROM_WRAPPER(machine_observe_build_report_from_machine_apply_report,
    MachineApplyReport, machine_observe_build_from_machine_apply_report)
MACHINE_OBSERVE_DEFINE_REPORT_FROM_WRAPPER(machine_observe_build_report_from_machine_commit_file,
    MachineCommitFile, machine_observe_build_from_machine_commit_file)
MACHINE_OBSERVE_DEFINE_REPORT_FROM_WRAPPER(machine_observe_build_report_from_machine_commit_report,
    MachineCommitReport, machine_observe_build_from_machine_commit_report)
MACHINE_OBSERVE_DEFINE_REPORT_FROM_WRAPPER(machine_observe_build_report_from_machine_step_file,
    MachineStepFile, machine_observe_build_from_machine_step_file)
MACHINE_OBSERVE_DEFINE_REPORT_FROM_WRAPPER(machine_observe_build_report_from_machine_step_report,
    MachineStepReport, machine_observe_build_from_machine_step_report)
MACHINE_OBSERVE_DEFINE_REPORT_FROM_WRAPPER(machine_observe_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_observe_build_from_machine_ir_report)

int machine_observe_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineObserveReport *out_report,
    MachineObserveError *error) {
    MachineObserveFile observe_file;
    int ok;

    machine_observe_file_init(&observe_file);
    ok = machine_observe_build_from_machine_ir_report_with_profile(report, profile, &observe_file, error) &&
        machine_observe_build_report_from_file(&observe_file, out_report, error);
    machine_observe_file_free(&observe_file);
    return ok;
}

int machine_observe_report_refresh(MachineObserveReport *report,
    MachineObserveError *error) {
    MachineObserveReport refreshed_report;
    int ok;

    if (!report) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-120: invalid report refresh contract");
        return 0;
    }
    machine_observe_report_init(&refreshed_report);
    ok = machine_observe_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_observe_report_free(report);
        *report = refreshed_report;
    } else {
        machine_observe_report_free(&refreshed_report);
    }
    return ok;
}

int machine_observe_dump_file(const MachineObserveFile *observe_file,
    char **out_text,
    MachineObserveError *error) {
    MachineObserveStringBuilder builder;
    MachineObserveHeaderSummary header_summary;
    MachineObserveSummary observe_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&observe_summary, 0, sizeof(observe_summary));
    if (!observe_file || !out_text ||
        !machine_observe_verify_file(observe_file, error) ||
        !machine_observe_file_get_header_summary(observe_file, &header_summary) ||
        !machine_observe_file_get_observe_summary(observe_file, &observe_summary)) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-121: invalid dump contract");
        return 0;
    }

    if (!machine_observe_append_format(
            &builder,
            "machine_observe profile=%s apply=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_apply_resolution_kind_name(header_summary.apply_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_observe_append_format(
            &builder,
            "observe: resolution=%s kind=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_observe_resolution_kind_name(observe_summary.resolution_kind),
            machine_observe_kind_name(observe_summary.observe_kind),
            machine_apply_resolution_kind_name(observe_summary.apply_resolution_kind),
            machine_commit_resolution_kind_name(observe_summary.commit_resolution_kind),
            machine_writeback_resolution_kind_name(observe_summary.writeback_resolution_kind),
            machine_mutation_resolution_kind_name(observe_summary.mutation_resolution_kind),
            machine_mutation_effect_kind_name(observe_summary.mutation_effect_kind),
            machine_transition_resolution_kind_name(observe_summary.transition_resolution_kind),
            machine_interp_action_kind_name(observe_summary.action_kind),
            (unsigned int)observe_summary.raw_byte,
            (unsigned int)observe_summary.tag_value,
            observe_summary.is_known ? "yes" : "no",
            observe_summary.tag_name ? observe_summary.tag_name : "-",
            observe_summary.instruction_byte_count) ||
        !machine_observe_append_payload_bytes(&builder, &observe_summary) ||
        !machine_observe_append_immediate_hint(&builder, &observe_summary) ||
        !machine_observe_append_format(
            &builder,
            " exact=%s has-state=%s status=%s pc=%s",
            observe_summary.is_exact_state ? "yes" : "no",
            observe_summary.has_observed_state ? "yes" : "no",
            observe_summary.has_observed_state ? machine_step_status_name(observe_summary.observed_status) : "-",
            observe_summary.has_observed_state ? "" : "-")) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-122: out of memory building dump");
        return 0;
    }
    if (observe_summary.has_observed_state &&
        !machine_observe_append_format(&builder, "0x%zx", observe_summary.program_counter)) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-123: out of memory appending observed pc");
        return 0;
    }
    if (!machine_observe_append_format(
            &builder,
            " current-segment=%s",
            observe_summary.has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-124: out of memory appending current segment");
        return 0;
    }
    if (observe_summary.has_current_fetch &&
        !machine_observe_append_format(&builder, "%zu", observe_summary.current_segment_index)) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-125: out of memory appending current segment value");
        return 0;
    }
    if (!machine_observe_append_format(
            &builder,
            " current-byte=%s",
            observe_summary.has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-126: out of memory appending current byte");
        return 0;
    }
    if (observe_summary.has_current_fetch &&
        !machine_observe_append_format(&builder, "0x%02x", (unsigned int)observe_summary.current_byte)) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-127: out of memory appending current byte value");
        return 0;
    }
    if (!machine_observe_append_targets(&builder, &observe_summary) ||
        !machine_observe_append_return_immediate(&builder, &observe_summary) ||
        !machine_observe_append_format(&builder, "\n")) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-128: out of memory terminating dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_observe_build_dump_from_file(const MachineObserveFile *source,
    char **out_text,
    MachineObserveError *error) {
    return machine_observe_dump_file(source, out_text, error);
}

#define MACHINE_OBSERVE_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineObserveError *error) { \
    MachineObserveFile observe_file; \
    int ok; \
    machine_observe_file_init(&observe_file); \
    ok = builder(source, &observe_file, error) && \
        machine_observe_dump_file(&observe_file, out_text, error); \
    machine_observe_file_free(&observe_file); \
    return ok; \
}

MACHINE_OBSERVE_DEFINE_DUMP_FROM_WRAPPER(machine_observe_build_dump_from_machine_apply_file,
    MachineApplyFile, machine_observe_build_from_machine_apply_file)
MACHINE_OBSERVE_DEFINE_DUMP_FROM_WRAPPER(machine_observe_build_dump_from_machine_apply_report,
    MachineApplyReport, machine_observe_build_from_machine_apply_report)
MACHINE_OBSERVE_DEFINE_DUMP_FROM_WRAPPER(machine_observe_build_dump_from_machine_commit_file,
    MachineCommitFile, machine_observe_build_from_machine_commit_file)
MACHINE_OBSERVE_DEFINE_DUMP_FROM_WRAPPER(machine_observe_build_dump_from_machine_commit_report,
    MachineCommitReport, machine_observe_build_from_machine_commit_report)
MACHINE_OBSERVE_DEFINE_DUMP_FROM_WRAPPER(machine_observe_build_dump_from_machine_step_file,
    MachineStepFile, machine_observe_build_from_machine_step_file)
MACHINE_OBSERVE_DEFINE_DUMP_FROM_WRAPPER(machine_observe_build_dump_from_machine_step_report,
    MachineStepReport, machine_observe_build_from_machine_step_report)
MACHINE_OBSERVE_DEFINE_DUMP_FROM_WRAPPER(machine_observe_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_observe_build_from_machine_ir_report)

int machine_observe_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineObserveError *error) {
    MachineObserveFile observe_file;
    int ok;

    machine_observe_file_init(&observe_file);
    ok = machine_observe_build_from_machine_ir_report_with_profile(report, profile, &observe_file, error) &&
        machine_observe_dump_file(&observe_file, out_text, error);
    machine_observe_file_free(&observe_file);
    return ok;
}

int machine_observe_report_get_summary(const MachineObserveReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_observe_report_get_overview_artifact(const MachineObserveReport *report,
    MachineObserveReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->apply_report = &report->apply_report;
    out_artifact->observe_summary = &report->observe_summary;
    out_artifact->current_fetch_summary = report->file.has_current_fetch ? &report->current_fetch_summary : NULL;
    return 1;
}

int machine_observe_report_get_file(const MachineObserveReport *report,
    const MachineObserveFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_observe_report_get_apply_file(const MachineObserveReport *report,
    const MachineApplyFile **out_apply_file) {
    if (!report || !out_apply_file) {
        return 0;
    }
    *out_apply_file = &report->file.apply_file;
    return 1;
}

int machine_observe_report_get_apply_report(const MachineObserveReport *report,
    const MachineApplyReport **out_apply_report) {
    if (!report || !out_apply_report) {
        return 0;
    }
    *out_apply_report = &report->apply_report;
    return 1;
}

#define MACHINE_OBSERVE_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineObserveReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_OBSERVE_DEFINE_REPORT_ARTIFACT_GETTER(machine_observe_report_get_header_summary_artifact,
    header_summary, MachineObserveHeaderSummary)
MACHINE_OBSERVE_DEFINE_REPORT_ARTIFACT_GETTER(machine_observe_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineObserveTargetPolicySummary)
MACHINE_OBSERVE_DEFINE_REPORT_ARTIFACT_GETTER(machine_observe_report_get_observe_summary_artifact,
    observe_summary, MachineObserveSummary)

int machine_observe_report_get_current_fetch_summary_artifact(const MachineObserveReport *report,
    const MachineObserveCurrentFetchSummary **out_summary) {
    if (!report || !out_summary || !report->file.has_current_fetch) {
        return 0;
    }
    *out_summary = &report->current_fetch_summary;
    return 1;
}

int machine_observe_report_overview_artifact_get_apply_report(
    const MachineObserveReportOverviewArtifact *artifact,
    const MachineApplyReport **out_apply_report) {
    if (!artifact || !out_apply_report) {
        return 0;
    }
    *out_apply_report = artifact->apply_report;
    return 1;
}

int machine_observe_report_overview_artifact_get_observe_summary_artifact(
    const MachineObserveReportOverviewArtifact *artifact,
    const MachineObserveSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->observe_summary;
    return 1;
}

int machine_observe_report_overview_artifact_get_current_fetch_summary_artifact(
    const MachineObserveReportOverviewArtifact *artifact,
    const MachineObserveCurrentFetchSummary **out_summary) {
    if (!artifact || !out_summary || !artifact->current_fetch_summary) {
        return 0;
    }
    *out_summary = artifact->current_fetch_summary;
    return 1;
}

int machine_observe_dump_report(const MachineObserveReport *report,
    char **out_text,
    MachineObserveError *error) {
    MachineObserveStringBuilder builder;
    const MachineObserveSummary *observe_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_observe_report_get_observe_summary_artifact(report, &observe_summary) ||
        !observe_summary) {
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-127: invalid report dump contract");
        return 0;
    }

    if (!machine_observe_append_format(
            &builder,
            "machine_observe profile=%s apply=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_apply_resolution_kind_name(report->header_summary.apply_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_observe_append_format(
            &builder,
            "observe: resolution=%s kind=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_observe_resolution_kind_name(observe_summary->resolution_kind),
            machine_observe_kind_name(observe_summary->observe_kind),
            machine_apply_resolution_kind_name(observe_summary->apply_resolution_kind),
            machine_commit_resolution_kind_name(observe_summary->commit_resolution_kind),
            machine_writeback_resolution_kind_name(observe_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(observe_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(observe_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(observe_summary->transition_resolution_kind),
            machine_interp_action_kind_name(observe_summary->action_kind),
            (unsigned int)observe_summary->raw_byte,
            (unsigned int)observe_summary->tag_value,
            observe_summary->is_known ? "yes" : "no",
            observe_summary->tag_name ? observe_summary->tag_name : "-",
            observe_summary->instruction_byte_count) ||
        !machine_observe_append_payload_bytes(&builder, observe_summary) ||
        !machine_observe_append_immediate_hint(&builder, observe_summary) ||
        !machine_observe_append_format(
            &builder,
            " exact=%s has-state=%s status=%s pc=%s",
            observe_summary->is_exact_state ? "yes" : "no",
            observe_summary->has_observed_state ? "yes" : "no",
            observe_summary->has_observed_state ? machine_step_status_name(observe_summary->observed_status) : "-",
            observe_summary->has_observed_state ? "" : "-")) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-128: out of memory building report dump");
        return 0;
    }
    if (observe_summary->has_observed_state &&
        !machine_observe_append_format(&builder, "0x%zx", observe_summary->program_counter)) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-129: out of memory appending report pc");
        return 0;
    }
    if (!machine_observe_append_format(
            &builder,
            " current-segment=%s",
            observe_summary->has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-130: out of memory appending report segment");
        return 0;
    }
    if (observe_summary->has_current_fetch &&
        !machine_observe_append_format(&builder, "%zu", observe_summary->current_segment_index)) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-131: out of memory appending report segment value");
        return 0;
    }
    if (!machine_observe_append_format(
            &builder,
            " current-byte=%s",
            observe_summary->has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-132: out of memory appending report byte");
        return 0;
    }
    if (observe_summary->has_current_fetch &&
        !machine_observe_append_format(&builder, "0x%02x", (unsigned int)observe_summary->current_byte)) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-133: out of memory appending report byte value");
        return 0;
    }
    if (!machine_observe_append_targets(&builder, observe_summary) ||
        !machine_observe_append_return_immediate(&builder, observe_summary) ||
        !machine_observe_append_format(&builder, "\nreport_overview:\n") ||
        !machine_observe_append_format(
            &builder,
            "  origin: apply=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_apply_resolution_kind_name(report->header_summary.apply_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer) ||
        !machine_observe_append_format(
            &builder,
            "  policy: profile=%s exact=%s preview=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.surfaces_exact_state ? "yes" : "no",
            report->target_policy_summary.surfaces_preview_state ? "yes" : "no") ||
        !machine_observe_append_format(
            &builder,
            "  observe: resolution=%s kind=%s exact=%s state=%s imm=%s pc=%s",
            machine_observe_resolution_kind_name(observe_summary->resolution_kind),
            machine_observe_kind_name(observe_summary->observe_kind),
            observe_summary->is_exact_state ? "yes" : "no",
            observe_summary->has_observed_state ? "yes" : "no",
            observe_summary->has_immediate_hint ? "" : "-",
            observe_summary->has_observed_state ? "" : "-")) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-134: out of memory building report overview");
        return 0;
    }
    if (observe_summary->has_immediate_hint &&
        !machine_observe_append_format(&builder, "%zu", observe_summary->immediate_hint)) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-135: out of memory appending report imm");
        return 0;
    }
    if (observe_summary->has_observed_state &&
        !machine_observe_append_format(&builder, "0x%zx", observe_summary->program_counter)) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-136: out of memory appending report overview pc");
        return 0;
    }
    if (!machine_observe_append_targets(&builder, observe_summary) ||
        !machine_observe_append_return_immediate(&builder, observe_summary) ||
        !machine_observe_append_format(&builder, "\n")) {
        free(builder.data);
        machine_observe_set_error(error, 0, 0, "MACHINE-OBSERVE-137: out of memory terminating report overview");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_observe_build_report_dump_from_file(const MachineObserveFile *source,
    char **out_text,
    MachineObserveError *error) {
    MachineObserveReport report;
    int ok;

    machine_observe_report_init(&report);
    ok = machine_observe_build_report_from_file(source, &report, error) &&
        machine_observe_dump_report(&report, out_text, error);
    machine_observe_report_free(&report);
    return ok;
}

#define MACHINE_OBSERVE_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineObserveError *error) { \
    MachineObserveReport report; \
    int ok; \
    machine_observe_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_observe_dump_report(&report, out_text, error); \
    machine_observe_report_free(&report); \
    return ok; \
}

MACHINE_OBSERVE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_observe_build_report_dump_from_machine_apply_file,
    MachineApplyFile, machine_observe_build_report_from_machine_apply_file)
MACHINE_OBSERVE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_observe_build_report_dump_from_machine_apply_report,
    MachineApplyReport, machine_observe_build_report_from_machine_apply_report)
MACHINE_OBSERVE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_observe_build_report_dump_from_machine_commit_file,
    MachineCommitFile, machine_observe_build_report_from_machine_commit_file)
MACHINE_OBSERVE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_observe_build_report_dump_from_machine_commit_report,
    MachineCommitReport, machine_observe_build_report_from_machine_commit_report)
MACHINE_OBSERVE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_observe_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_observe_build_report_from_machine_step_file)
MACHINE_OBSERVE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_observe_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_observe_build_report_from_machine_step_report)
MACHINE_OBSERVE_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_observe_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_observe_build_report_from_machine_ir_report)

int machine_observe_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineObserveError *error) {
    MachineObserveReport report_file;
    int ok;

    machine_observe_report_init(&report_file);
    ok = machine_observe_build_report_from_machine_ir_report_with_profile(report, profile, &report_file, error) &&
        machine_observe_dump_report(&report_file, out_text, error);
    machine_observe_report_free(&report_file);
    return ok;
}
