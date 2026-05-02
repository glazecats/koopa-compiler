#include "machine/apply.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineApplyStringBuilder;

static void machine_apply_set_error(MachineApplyError *error, int line, int column, const char *fmt, ...);
static int machine_apply_append_format(MachineApplyStringBuilder *builder, const char *fmt, ...);
static int machine_apply_append_targets(MachineApplyStringBuilder *builder,
    const MachineApplySummary *apply_summary);
static int machine_apply_append_return_immediate(MachineApplyStringBuilder *builder,
    const MachineApplySummary *apply_summary);
static int machine_apply_append_payload_bytes(MachineApplyStringBuilder *builder,
    const MachineApplySummary *apply_summary);
static int machine_apply_append_immediate_hint(MachineApplyStringBuilder *builder,
    const MachineApplySummary *apply_summary);
static int machine_apply_append_optional_pc(MachineApplyStringBuilder *builder,
    int has_state,
    size_t value);
static int machine_apply_append_optional_segment(MachineApplyStringBuilder *builder,
    int has_fetch,
    size_t value);
static int machine_apply_append_optional_byte(MachineApplyStringBuilder *builder,
    int has_fetch,
    unsigned char value);

static void machine_apply_set_error(MachineApplyError *error, int line, int column, const char *fmt, ...) {
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

static int machine_apply_append_format(MachineApplyStringBuilder *builder, const char *fmt, ...) {
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

static int machine_apply_append_targets(MachineApplyStringBuilder *builder,
    const MachineApplySummary *apply_summary) {
    if (!builder || !apply_summary) {
        return 0;
    }
    if (!apply_summary->has_primary_target_block) {
        return machine_apply_append_format(builder, " targets=[]");
    }
    if (!apply_summary->has_secondary_target_block) {
        return machine_apply_append_format(
            builder,
            " targets=[%zu]",
            apply_summary->primary_target_block_index);
    }
    return machine_apply_append_format(
        builder,
        " targets=[%zu,%zu]",
        apply_summary->primary_target_block_index,
        apply_summary->secondary_target_block_index);
}

static int machine_apply_append_return_immediate(MachineApplyStringBuilder *builder,
    const MachineApplySummary *apply_summary) {
    if (!builder || !apply_summary) {
        return 0;
    }
    if (!apply_summary->has_return_immediate) {
        return machine_apply_append_format(builder, " return-imm=-");
    }
    return machine_apply_append_format(builder, " return-imm=%zu", apply_summary->return_immediate);
}

static int machine_apply_append_payload_bytes(MachineApplyStringBuilder *builder,
    const MachineApplySummary *apply_summary) {
    size_t index;

    if (!builder || !apply_summary) {
        return 0;
    }
    if (!machine_apply_append_format(builder, " payload=[")) {
        return 0;
    }
    for (index = 0u; index < apply_summary->payload_byte_count; ++index) {
        if (!machine_apply_append_format(
                builder,
                "%s0x%02x",
                index == 0u ? "" : ",",
                (unsigned int)apply_summary->payload_bytes[index])) {
            return 0;
        }
    }
    return machine_apply_append_format(builder, "]");
}

static int machine_apply_append_immediate_hint(MachineApplyStringBuilder *builder,
    const MachineApplySummary *apply_summary) {
    if (!builder || !apply_summary) {
        return 0;
    }
    if (!apply_summary->has_immediate_hint) {
        return machine_apply_append_format(builder, " imm=-");
    }
    return machine_apply_append_format(builder, " imm=%zu", apply_summary->immediate_hint);
}

static int machine_apply_append_optional_pc(MachineApplyStringBuilder *builder,
    int has_state,
    size_t value) {
    if (!builder) {
        return 0;
    }
    if (!has_state) {
        return machine_apply_append_format(builder, "-");
    }
    return machine_apply_append_format(builder, "0x%zx", value);
}

static int machine_apply_append_optional_segment(MachineApplyStringBuilder *builder,
    int has_fetch,
    size_t value) {
    if (!builder) {
        return 0;
    }
    if (!has_fetch) {
        return machine_apply_append_format(builder, "-");
    }
    return machine_apply_append_format(builder, "%zu", value);
}

static int machine_apply_append_optional_byte(MachineApplyStringBuilder *builder,
    int has_fetch,
    unsigned char value) {
    if (!builder) {
        return 0;
    }
    if (!has_fetch) {
        return machine_apply_append_format(builder, "-");
    }
    return machine_apply_append_format(builder, "0x%02x", (unsigned int)value);
}

void machine_apply_file_init(MachineApplyFile *apply_file) {
    if (!apply_file) {
        return;
    }
    machine_commit_file_init(&apply_file->commit_file);
    apply_file->resolution_kind = MACHINE_APPLY_RESOLUTION_NONE;
    apply_file->apply_kind = MACHINE_APPLY_KIND_NONE;
    apply_file->payload_byte_count = 0u;
    memset(apply_file->payload_bytes, 0, sizeof(apply_file->payload_bytes));
    apply_file->has_immediate_hint = 0;
    apply_file->immediate_hint = 0u;
    apply_file->has_applied_state = 0;
    apply_file->applied_status = MACHINE_STEP_STATUS_READY;
    apply_file->applied_program_counter = 0u;
    apply_file->applied_stack_pointer = 0u;
    apply_file->has_applied_fetch = 0;
    apply_file->applied_segment_index = 0u;
    apply_file->applied_byte = 0u;
    apply_file->applied_byte_memory_offset = 0u;
    apply_file->has_preview_state = 0;
    apply_file->preview_status = MACHINE_STEP_STATUS_READY;
    apply_file->preview_program_counter = 0u;
    apply_file->preview_stack_pointer = 0u;
    apply_file->has_preview_fetch = 0;
    apply_file->preview_segment_index = 0u;
    apply_file->preview_byte = 0u;
    apply_file->preview_byte_memory_offset = 0u;
}

void machine_apply_file_free(MachineApplyFile *apply_file) {
    if (!apply_file) {
        return;
    }
    machine_commit_file_free(&apply_file->commit_file);
    machine_apply_file_init(apply_file);
}

void machine_apply_report_init(MachineApplyReport *report) {
    if (!report) {
        return;
    }
    machine_apply_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_commit_report_init(&report->commit_report);
    memset(&report->apply_summary, 0, sizeof(report->apply_summary));
}

void machine_apply_report_free(MachineApplyReport *report) {
    if (!report) {
        return;
    }
    machine_commit_report_free(&report->commit_report);
    machine_apply_file_free(&report->file);
    machine_apply_report_init(report);
}

const char *machine_apply_resolution_kind_name(MachineApplyResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_APPLY_RESOLUTION_NONE:
            return "none";
        case MACHINE_APPLY_RESOLUTION_APPLIED_STATE:
            return "applied-state";
        case MACHINE_APPLY_RESOLUTION_PENDING_REGISTER_APPLICATION:
            return "pending-register-application";
        case MACHINE_APPLY_RESOLUTION_PENDING_LOCAL_APPLICATION:
            return "pending-local-application";
        case MACHINE_APPLY_RESOLUTION_PENDING_GLOBAL_APPLICATION:
            return "pending-global-application";
        case MACHINE_APPLY_RESOLUTION_PENDING_CALL_APPLICATION:
            return "pending-call-application";
        case MACHINE_APPLY_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_APPLY_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_apply_kind_name(MachineApplyKind apply_kind) {
    switch (apply_kind) {
        case MACHINE_APPLY_KIND_NONE:
            return "none";
        case MACHINE_APPLY_KIND_STATE:
            return "state";
        case MACHINE_APPLY_KIND_REGISTER:
            return "register";
        case MACHINE_APPLY_KIND_LOCAL_SLOT:
            return "local-slot";
        case MACHINE_APPLY_KIND_GLOBAL_SLOT:
            return "global-slot";
        case MACHINE_APPLY_KIND_CALL:
            return "call";
    }
    return "unknown";
}

int machine_apply_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineApplyTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->applies_no_op_state = 1;
    out_summary->surfaces_register_application = 1;
    out_summary->surfaces_slot_application = 1;
    out_summary->surfaces_call_application = 1;
    out_summary->surfaces_state_preview = 1;
    return 1;
}

int machine_apply_file_get_target_policy_summary(const MachineApplyFile *apply_file,
    MachineApplyTargetPolicySummary *out_summary) {
    if (!apply_file || !out_summary) {
        return 0;
    }
    return machine_apply_get_target_policy_summary(
        apply_file->commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_apply_file_get_summary(const MachineApplyFile *apply_file,
    size_t *out_mapped_byte_count) {
    if (!apply_file) {
        return 0;
    }
    return machine_commit_file_get_summary(&apply_file->commit_file, out_mapped_byte_count);
}

int machine_apply_file_get_header_summary(const MachineApplyFile *apply_file,
    MachineApplyHeaderSummary *out_summary) {
    MachineCommitHeaderSummary commit_header_summary;

    memset(&commit_header_summary, 0, sizeof(commit_header_summary));
    if (!apply_file || !out_summary ||
        !machine_commit_file_get_header_summary(&apply_file->commit_file, &commit_header_summary)) {
        return 0;
    }
    out_summary->target_profile = commit_header_summary.target_profile;
    out_summary->commit_resolution_kind = apply_file->commit_file.resolution_kind;
    out_summary->origin_step_status = commit_header_summary.origin_step_status;
    out_summary->origin_program_counter = commit_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = commit_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = commit_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = commit_header_summary.mapped_byte_count;
    return 1;
}

int machine_apply_file_get_commit_summary(const MachineApplyFile *apply_file,
    MachineCommitSummary *out_summary) {
    if (!apply_file || !out_summary) {
        return 0;
    }
    return machine_commit_file_get_commit_summary(&apply_file->commit_file, out_summary);
}

int machine_apply_file_get_apply_summary(const MachineApplyFile *apply_file,
    MachineApplySummary *out_summary) {
    MachineCommitSummary commit_summary;
    MachinePayloadDecodeSummary payload_summary;

    memset(&commit_summary, 0, sizeof(commit_summary));
    memset(&payload_summary, 0, sizeof(payload_summary));
    if (!apply_file || !out_summary ||
        !machine_apply_file_get_commit_summary(apply_file, &commit_summary) ||
        !machine_payload_decode_file_get_payload_summary(
            &apply_file->commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file,
            &payload_summary)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->resolution_kind = apply_file->resolution_kind;
    out_summary->apply_kind = apply_file->apply_kind;
    out_summary->commit_resolution_kind = commit_summary.resolution_kind;
    out_summary->commit_kind = commit_summary.commit_kind;
    out_summary->writeback_resolution_kind = commit_summary.writeback_resolution_kind;
    out_summary->writeback_commit_kind = commit_summary.writeback_commit_kind;
    out_summary->mutation_resolution_kind = commit_summary.mutation_resolution_kind;
    out_summary->mutation_effect_kind = commit_summary.mutation_effect_kind;
    out_summary->state_resolution_kind = commit_summary.state_resolution_kind;
    out_summary->transition_resolution_kind = commit_summary.transition_resolution_kind;
    out_summary->action_kind = commit_summary.action_kind;
    out_summary->payload_kind = payload_summary.kind;
    out_summary->tag_class = commit_summary.tag_class;
    out_summary->raw_byte = commit_summary.raw_byte;
    out_summary->tag_value = commit_summary.tag_value;
    out_summary->is_known = commit_summary.is_known;
    out_summary->tag_name = commit_summary.tag_name;
    out_summary->instruction_byte_count = commit_summary.instruction_byte_count;
    out_summary->payload_byte_count = apply_file->payload_byte_count;
    memcpy(out_summary->payload_bytes, apply_file->payload_bytes, sizeof(out_summary->payload_bytes));
    out_summary->has_immediate_hint = apply_file->has_immediate_hint;
    out_summary->immediate_hint = apply_file->immediate_hint;
    out_summary->has_applied_state = apply_file->has_applied_state;
    out_summary->applied_status = apply_file->applied_status;
    out_summary->applied_program_counter = apply_file->applied_program_counter;
    out_summary->applied_stack_pointer = apply_file->applied_stack_pointer;
    out_summary->has_applied_fetch = apply_file->has_applied_fetch;
    out_summary->applied_segment_index = apply_file->applied_segment_index;
    out_summary->applied_byte = apply_file->applied_byte;
    out_summary->applied_byte_memory_offset = apply_file->applied_byte_memory_offset;
    out_summary->has_preview_state = apply_file->has_preview_state;
    out_summary->preview_status = apply_file->preview_status;
    out_summary->preview_program_counter = apply_file->preview_program_counter;
    out_summary->preview_stack_pointer = apply_file->preview_stack_pointer;
    out_summary->has_preview_fetch = apply_file->has_preview_fetch;
    out_summary->preview_segment_index = apply_file->preview_segment_index;
    out_summary->preview_byte = apply_file->preview_byte;
    out_summary->preview_byte_memory_offset = apply_file->preview_byte_memory_offset;
    out_summary->has_primary_target_block = commit_summary.has_primary_target_block;
    out_summary->primary_target_block_index = commit_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = commit_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = commit_summary.secondary_target_block_index;
    out_summary->has_return_immediate = commit_summary.has_return_immediate;
    out_summary->return_immediate = commit_summary.return_immediate;
    return 1;
}

int machine_apply_verify_file(const MachineApplyFile *apply_file,
    MachineApplyError *error) {
    MachineCommitSummary commit_summary;
    MachineStateSummary state_summary;
    MachinePayloadDecodeSummary payload_summary;
    MachineInterpSummary interp_summary;
    int expected_has_immediate_hint;
    size_t expected_immediate_hint;

    memset(&commit_summary, 0, sizeof(commit_summary));
    memset(&state_summary, 0, sizeof(state_summary));
    memset(&payload_summary, 0, sizeof(payload_summary));
    memset(&interp_summary, 0, sizeof(interp_summary));
    if (!apply_file) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-100: invalid apply file");
        return 0;
    }
    if (!machine_commit_verify_file(&apply_file->commit_file, NULL) ||
        !machine_apply_file_get_commit_summary(apply_file, &commit_summary) ||
        !machine_state_file_get_state_summary(
            &apply_file->commit_file.writeback_file.mutation_file.state_file, &state_summary) ||
        !machine_interp_file_get_interp_summary(
            &apply_file->commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file,
            &interp_summary) ||
        !machine_payload_decode_file_get_payload_summary(
            &apply_file->commit_file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file,
            &payload_summary)) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-101: invalid commit input");
        return 0;
    }

    expected_has_immediate_hint = payload_summary.payload_byte_count > 0u ? 1 : 0;
    expected_immediate_hint = payload_summary.immediate_value;
    if (interp_summary.has_return_immediate) {
        expected_has_immediate_hint = 1;
        expected_immediate_hint = interp_summary.return_immediate;
    }
    if (apply_file->payload_byte_count != payload_summary.payload_byte_count ||
        memcmp(apply_file->payload_bytes, payload_summary.payload_bytes, sizeof(apply_file->payload_bytes)) != 0 ||
        apply_file->has_immediate_hint != expected_has_immediate_hint ||
        (apply_file->has_immediate_hint && apply_file->immediate_hint != expected_immediate_hint)) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-102: invalid payload application hint");
        return 0;
    }

    switch (apply_file->resolution_kind) {
        case MACHINE_APPLY_RESOLUTION_APPLIED_STATE:
            if (apply_file->commit_file.resolution_kind != MACHINE_COMMIT_RESOLUTION_COMMITTED_STATE ||
                apply_file->apply_kind != MACHINE_APPLY_KIND_STATE ||
                !apply_file->has_applied_state ||
                !apply_file->has_preview_state ||
                apply_file->applied_status != commit_summary.committed_status ||
                apply_file->applied_program_counter != commit_summary.program_counter ||
                apply_file->applied_stack_pointer != commit_summary.stack_pointer ||
                apply_file->has_applied_fetch != commit_summary.has_current_fetch ||
                (commit_summary.has_current_fetch &&
                    (apply_file->applied_segment_index != commit_summary.current_segment_index ||
                        apply_file->applied_byte != commit_summary.current_byte ||
                        apply_file->applied_byte_memory_offset != commit_summary.current_byte_memory_offset)) ||
                apply_file->preview_status != state_summary.state_status ||
                apply_file->preview_program_counter != state_summary.program_counter ||
                apply_file->preview_stack_pointer != state_summary.stack_pointer ||
                apply_file->has_preview_fetch != state_summary.has_current_fetch ||
                (state_summary.has_current_fetch &&
                    (apply_file->preview_segment_index != state_summary.current_segment_index ||
                        apply_file->preview_byte != state_summary.current_byte ||
                        apply_file->preview_byte_memory_offset != state_summary.current_byte_memory_offset))) {
                machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-103: invalid applied-state application");
                return 0;
            }
            break;
        case MACHINE_APPLY_RESOLUTION_PENDING_REGISTER_APPLICATION:
            if (apply_file->commit_file.resolution_kind != MACHINE_COMMIT_RESOLUTION_DEFERRED_REGISTER_COMMIT ||
                apply_file->apply_kind != MACHINE_APPLY_KIND_REGISTER ||
                apply_file->has_applied_state ||
                !apply_file->has_preview_state ||
                !state_summary.has_state ||
                apply_file->preview_status != state_summary.state_status ||
                apply_file->preview_program_counter != state_summary.program_counter ||
                apply_file->preview_stack_pointer != state_summary.stack_pointer ||
                apply_file->has_preview_fetch != state_summary.has_current_fetch ||
                (state_summary.has_current_fetch &&
                    (apply_file->preview_segment_index != state_summary.current_segment_index ||
                        apply_file->preview_byte != state_summary.current_byte ||
                        apply_file->preview_byte_memory_offset != state_summary.current_byte_memory_offset))) {
                machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-104: invalid pending register application");
                return 0;
            }
            break;
        case MACHINE_APPLY_RESOLUTION_PENDING_LOCAL_APPLICATION:
            if (apply_file->commit_file.resolution_kind != MACHINE_COMMIT_RESOLUTION_DEFERRED_LOCAL_COMMIT ||
                apply_file->apply_kind != MACHINE_APPLY_KIND_LOCAL_SLOT ||
                apply_file->has_applied_state ||
                !apply_file->has_preview_state ||
                !state_summary.has_state ||
                apply_file->preview_status != state_summary.state_status ||
                apply_file->preview_program_counter != state_summary.program_counter ||
                apply_file->preview_stack_pointer != state_summary.stack_pointer ||
                apply_file->has_preview_fetch != state_summary.has_current_fetch ||
                (state_summary.has_current_fetch &&
                    (apply_file->preview_segment_index != state_summary.current_segment_index ||
                        apply_file->preview_byte != state_summary.current_byte ||
                        apply_file->preview_byte_memory_offset != state_summary.current_byte_memory_offset))) {
                machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-105: invalid pending local application");
                return 0;
            }
            break;
        case MACHINE_APPLY_RESOLUTION_PENDING_GLOBAL_APPLICATION:
            if (apply_file->commit_file.resolution_kind != MACHINE_COMMIT_RESOLUTION_DEFERRED_GLOBAL_COMMIT ||
                apply_file->apply_kind != MACHINE_APPLY_KIND_GLOBAL_SLOT ||
                apply_file->has_applied_state ||
                !apply_file->has_preview_state ||
                !state_summary.has_state ||
                apply_file->preview_status != state_summary.state_status ||
                apply_file->preview_program_counter != state_summary.program_counter ||
                apply_file->preview_stack_pointer != state_summary.stack_pointer ||
                apply_file->has_preview_fetch != state_summary.has_current_fetch ||
                (state_summary.has_current_fetch &&
                    (apply_file->preview_segment_index != state_summary.current_segment_index ||
                        apply_file->preview_byte != state_summary.current_byte ||
                        apply_file->preview_byte_memory_offset != state_summary.current_byte_memory_offset))) {
                machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-106: invalid pending global application");
                return 0;
            }
            break;
        case MACHINE_APPLY_RESOLUTION_PENDING_CALL_APPLICATION:
            if (apply_file->commit_file.resolution_kind != MACHINE_COMMIT_RESOLUTION_DEFERRED_CALL_COMMIT ||
                apply_file->apply_kind != MACHINE_APPLY_KIND_CALL ||
                apply_file->has_applied_state ||
                !apply_file->has_preview_state ||
                !state_summary.has_state ||
                apply_file->preview_status != state_summary.state_status ||
                apply_file->preview_program_counter != state_summary.program_counter ||
                apply_file->preview_stack_pointer != state_summary.stack_pointer ||
                apply_file->has_preview_fetch != state_summary.has_current_fetch ||
                (state_summary.has_current_fetch &&
                    (apply_file->preview_segment_index != state_summary.current_segment_index ||
                        apply_file->preview_byte != state_summary.current_byte ||
                        apply_file->preview_byte_memory_offset != state_summary.current_byte_memory_offset))) {
                machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-107: invalid pending call application");
                return 0;
            }
            break;
        case MACHINE_APPLY_RESOLUTION_BLOCKED_ON_CONTROL:
            if (apply_file->commit_file.resolution_kind != MACHINE_COMMIT_RESOLUTION_BLOCKED_ON_CONTROL ||
                apply_file->apply_kind != MACHINE_APPLY_KIND_NONE ||
                apply_file->has_applied_state ||
                apply_file->has_preview_state ||
                state_summary.has_state) {
                machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-108: invalid blocked-control application");
                return 0;
            }
            break;
        case MACHINE_APPLY_RESOLUTION_BLOCKED_UNSUPPORTED:
            if (apply_file->commit_file.resolution_kind != MACHINE_COMMIT_RESOLUTION_BLOCKED_UNSUPPORTED ||
                apply_file->apply_kind != MACHINE_APPLY_KIND_NONE ||
                apply_file->has_applied_state ||
                apply_file->has_preview_state ||
                state_summary.has_state) {
                machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-109: invalid blocked-unsupported application");
                return 0;
            }
            break;
        case MACHINE_APPLY_RESOLUTION_NONE:
        default:
            machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-110: invalid apply resolution kind");
            return 0;
    }
    return 1;
}

int machine_apply_clone_file(const MachineApplyFile *source,
    MachineApplyFile *out_clone,
    MachineApplyError *error) {
    if (!source || !out_clone) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-111: invalid clone contract");
        return 0;
    }
    if (!machine_apply_verify_file(source, error)) {
        return 0;
    }

    machine_apply_file_free(out_clone);
    if (!machine_commit_clone_file(&source->commit_file, &out_clone->commit_file, NULL)) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-112: failed cloning commit input");
        machine_apply_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->apply_kind = source->apply_kind;
    out_clone->payload_byte_count = source->payload_byte_count;
    memcpy(out_clone->payload_bytes, source->payload_bytes, sizeof(out_clone->payload_bytes));
    out_clone->has_immediate_hint = source->has_immediate_hint;
    out_clone->immediate_hint = source->immediate_hint;
    out_clone->has_applied_state = source->has_applied_state;
    out_clone->applied_status = source->applied_status;
    out_clone->applied_program_counter = source->applied_program_counter;
    out_clone->applied_stack_pointer = source->applied_stack_pointer;
    out_clone->has_applied_fetch = source->has_applied_fetch;
    out_clone->applied_segment_index = source->applied_segment_index;
    out_clone->applied_byte = source->applied_byte;
    out_clone->applied_byte_memory_offset = source->applied_byte_memory_offset;
    out_clone->has_preview_state = source->has_preview_state;
    out_clone->preview_status = source->preview_status;
    out_clone->preview_program_counter = source->preview_program_counter;
    out_clone->preview_stack_pointer = source->preview_stack_pointer;
    out_clone->has_preview_fetch = source->has_preview_fetch;
    out_clone->preview_segment_index = source->preview_segment_index;
    out_clone->preview_byte = source->preview_byte;
    out_clone->preview_byte_memory_offset = source->preview_byte_memory_offset;
    return 1;
}

int machine_apply_build_from_machine_commit_file(const MachineCommitFile *commit_file,
    MachineApplyFile *out_apply_file,
    MachineApplyError *error) {
    MachineStateSummary state_summary;
    MachinePayloadDecodeSummary payload_summary;
    MachineInterpSummary interp_summary;

    memset(&state_summary, 0, sizeof(state_summary));
    memset(&payload_summary, 0, sizeof(payload_summary));
    memset(&interp_summary, 0, sizeof(interp_summary));
    if (!commit_file || !out_apply_file) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-113: invalid build contract");
        return 0;
    }
    if (!machine_commit_verify_file(commit_file, NULL) ||
        !machine_state_file_get_state_summary(
            &commit_file->writeback_file.mutation_file.state_file, &state_summary) ||
        !machine_interp_file_get_interp_summary(
            &commit_file->writeback_file.mutation_file.state_file.transition_file.interp_file,
            &interp_summary) ||
        !machine_payload_decode_file_get_payload_summary(
            &commit_file->writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file,
            &payload_summary)) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-114: invalid commit input");
        return 0;
    }

    machine_apply_file_free(out_apply_file);
    if (!machine_commit_clone_file(commit_file, &out_apply_file->commit_file, NULL)) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-115: failed cloning commit input");
        machine_apply_file_free(out_apply_file);
        return 0;
    }

    out_apply_file->payload_byte_count = payload_summary.payload_byte_count;
    memcpy(out_apply_file->payload_bytes, payload_summary.payload_bytes, sizeof(out_apply_file->payload_bytes));
    out_apply_file->has_immediate_hint = payload_summary.payload_byte_count > 0u ? 1 : 0;
    out_apply_file->immediate_hint = payload_summary.immediate_value;
    if (interp_summary.has_return_immediate) {
        out_apply_file->has_immediate_hint = 1;
        out_apply_file->immediate_hint = interp_summary.return_immediate;
    }

    switch (commit_file->resolution_kind) {
        case MACHINE_COMMIT_RESOLUTION_COMMITTED_STATE:
            out_apply_file->resolution_kind = MACHINE_APPLY_RESOLUTION_APPLIED_STATE;
            out_apply_file->apply_kind = MACHINE_APPLY_KIND_STATE;
            out_apply_file->has_applied_state = 1;
            out_apply_file->applied_status = commit_file->committed_status;
            out_apply_file->applied_program_counter = commit_file->program_counter;
            out_apply_file->applied_stack_pointer = commit_file->stack_pointer;
            out_apply_file->has_applied_fetch = commit_file->has_current_fetch;
            out_apply_file->applied_segment_index = commit_file->current_segment_index;
            out_apply_file->applied_byte = commit_file->current_byte;
            out_apply_file->applied_byte_memory_offset = commit_file->current_byte_memory_offset;
            out_apply_file->has_preview_state = state_summary.has_state;
            out_apply_file->preview_status = state_summary.state_status;
            out_apply_file->preview_program_counter = state_summary.program_counter;
            out_apply_file->preview_stack_pointer = state_summary.stack_pointer;
            out_apply_file->has_preview_fetch = state_summary.has_current_fetch;
            out_apply_file->preview_segment_index = state_summary.current_segment_index;
            out_apply_file->preview_byte = state_summary.current_byte;
            out_apply_file->preview_byte_memory_offset = state_summary.current_byte_memory_offset;
            break;
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_REGISTER_COMMIT:
            out_apply_file->resolution_kind = MACHINE_APPLY_RESOLUTION_PENDING_REGISTER_APPLICATION;
            out_apply_file->apply_kind = MACHINE_APPLY_KIND_REGISTER;
            out_apply_file->has_preview_state = state_summary.has_state;
            out_apply_file->preview_status = state_summary.state_status;
            out_apply_file->preview_program_counter = state_summary.program_counter;
            out_apply_file->preview_stack_pointer = state_summary.stack_pointer;
            out_apply_file->has_preview_fetch = state_summary.has_current_fetch;
            out_apply_file->preview_segment_index = state_summary.current_segment_index;
            out_apply_file->preview_byte = state_summary.current_byte;
            out_apply_file->preview_byte_memory_offset = state_summary.current_byte_memory_offset;
            break;
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_LOCAL_COMMIT:
            out_apply_file->resolution_kind = MACHINE_APPLY_RESOLUTION_PENDING_LOCAL_APPLICATION;
            out_apply_file->apply_kind = MACHINE_APPLY_KIND_LOCAL_SLOT;
            out_apply_file->has_preview_state = state_summary.has_state;
            out_apply_file->preview_status = state_summary.state_status;
            out_apply_file->preview_program_counter = state_summary.program_counter;
            out_apply_file->preview_stack_pointer = state_summary.stack_pointer;
            out_apply_file->has_preview_fetch = state_summary.has_current_fetch;
            out_apply_file->preview_segment_index = state_summary.current_segment_index;
            out_apply_file->preview_byte = state_summary.current_byte;
            out_apply_file->preview_byte_memory_offset = state_summary.current_byte_memory_offset;
            break;
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_GLOBAL_COMMIT:
            out_apply_file->resolution_kind = MACHINE_APPLY_RESOLUTION_PENDING_GLOBAL_APPLICATION;
            out_apply_file->apply_kind = MACHINE_APPLY_KIND_GLOBAL_SLOT;
            out_apply_file->has_preview_state = state_summary.has_state;
            out_apply_file->preview_status = state_summary.state_status;
            out_apply_file->preview_program_counter = state_summary.program_counter;
            out_apply_file->preview_stack_pointer = state_summary.stack_pointer;
            out_apply_file->has_preview_fetch = state_summary.has_current_fetch;
            out_apply_file->preview_segment_index = state_summary.current_segment_index;
            out_apply_file->preview_byte = state_summary.current_byte;
            out_apply_file->preview_byte_memory_offset = state_summary.current_byte_memory_offset;
            break;
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_CALL_COMMIT:
            out_apply_file->resolution_kind = MACHINE_APPLY_RESOLUTION_PENDING_CALL_APPLICATION;
            out_apply_file->apply_kind = MACHINE_APPLY_KIND_CALL;
            out_apply_file->has_preview_state = state_summary.has_state;
            out_apply_file->preview_status = state_summary.state_status;
            out_apply_file->preview_program_counter = state_summary.program_counter;
            out_apply_file->preview_stack_pointer = state_summary.stack_pointer;
            out_apply_file->has_preview_fetch = state_summary.has_current_fetch;
            out_apply_file->preview_segment_index = state_summary.current_segment_index;
            out_apply_file->preview_byte = state_summary.current_byte;
            out_apply_file->preview_byte_memory_offset = state_summary.current_byte_memory_offset;
            break;
        case MACHINE_COMMIT_RESOLUTION_BLOCKED_ON_CONTROL:
            out_apply_file->resolution_kind = MACHINE_APPLY_RESOLUTION_BLOCKED_ON_CONTROL;
            out_apply_file->apply_kind = MACHINE_APPLY_KIND_NONE;
            break;
        case MACHINE_COMMIT_RESOLUTION_BLOCKED_UNSUPPORTED:
            out_apply_file->resolution_kind = MACHINE_APPLY_RESOLUTION_BLOCKED_UNSUPPORTED;
            out_apply_file->apply_kind = MACHINE_APPLY_KIND_NONE;
            break;
        case MACHINE_COMMIT_RESOLUTION_NONE:
        default:
            machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-116: unsupported commit resolution");
            machine_apply_file_free(out_apply_file);
            return 0;
    }

    if (!machine_apply_verify_file(out_apply_file, error)) {
        machine_apply_file_free(out_apply_file);
        return 0;
    }
    return 1;
}

int machine_apply_build_from_machine_commit_report(const MachineCommitReport *report,
    MachineApplyFile *out_apply_file,
    MachineApplyError *error) {
    const MachineCommitFile *commit_file = NULL;

    if (!report) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-117: invalid build-from-commit-report contract");
        return 0;
    }
    if (!machine_commit_report_get_file(report, &commit_file) || !commit_file) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-118: failed recovering commit file from report");
        return 0;
    }
    return machine_apply_build_from_machine_commit_file(commit_file, out_apply_file, error);
}

#define MACHINE_APPLY_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineApplyFile *out_apply_file, MachineApplyError *error) { \
    MachineCommitFile commit_file; \
    int ok; \
    machine_commit_file_init(&commit_file); \
    ok = builder(source, &commit_file, NULL) && \
        machine_apply_build_from_machine_commit_file(&commit_file, out_apply_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-119: failed building apply wrapper"); \
    } \
    machine_commit_file_free(&commit_file); \
    return ok; \
}

MACHINE_APPLY_DEFINE_BUILD_FROM_WRAPPER(machine_apply_build_from_machine_step_file,
    MachineStepFile, machine_commit_build_from_machine_step_file)
MACHINE_APPLY_DEFINE_BUILD_FROM_WRAPPER(machine_apply_build_from_machine_step_report,
    MachineStepReport, machine_commit_build_from_machine_step_report)
MACHINE_APPLY_DEFINE_BUILD_FROM_WRAPPER(machine_apply_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_commit_build_from_machine_ir_report)

int machine_apply_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineApplyFile *out_apply_file,
    MachineApplyError *error) {
    MachineCommitFile commit_file;
    int ok;

    machine_commit_file_init(&commit_file);
    ok = machine_commit_build_from_machine_ir_report_with_profile(report, profile, &commit_file, NULL) &&
        machine_apply_build_from_machine_commit_file(&commit_file, out_apply_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-120: failed building profiled apply wrapper");
    }
    machine_commit_file_free(&commit_file);
    return ok;
}

int machine_apply_build_report_from_file(const MachineApplyFile *source,
    MachineApplyReport *out_report,
    MachineApplyError *error) {
    if (!source || !out_report) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-121: invalid report build contract");
        return 0;
    }
    if (!machine_apply_verify_file(source, error)) {
        return 0;
    }

    machine_apply_report_free(out_report);
    if (!machine_apply_clone_file(source, &out_report->file, error)) {
        machine_apply_report_free(out_report);
        return 0;
    }
    if (!machine_apply_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_apply_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_commit_build_report_from_file(&out_report->file.commit_file, &out_report->commit_report, NULL) ||
        !machine_apply_file_get_apply_summary(&out_report->file, &out_report->apply_summary)) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-122: failed summarizing apply report");
        machine_apply_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_APPLY_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineApplyReport *out_report, MachineApplyError *error) { \
    MachineApplyFile apply_file; \
    int ok; \
    machine_apply_file_init(&apply_file); \
    ok = builder(source, &apply_file, error) && \
        machine_apply_build_report_from_file(&apply_file, out_report, error); \
    machine_apply_file_free(&apply_file); \
    return ok; \
}

MACHINE_APPLY_DEFINE_REPORT_FROM_WRAPPER(machine_apply_build_report_from_machine_commit_file,
    MachineCommitFile, machine_apply_build_from_machine_commit_file)
MACHINE_APPLY_DEFINE_REPORT_FROM_WRAPPER(machine_apply_build_report_from_machine_commit_report,
    MachineCommitReport, machine_apply_build_from_machine_commit_report)
MACHINE_APPLY_DEFINE_REPORT_FROM_WRAPPER(machine_apply_build_report_from_machine_step_file,
    MachineStepFile, machine_apply_build_from_machine_step_file)
MACHINE_APPLY_DEFINE_REPORT_FROM_WRAPPER(machine_apply_build_report_from_machine_step_report,
    MachineStepReport, machine_apply_build_from_machine_step_report)
MACHINE_APPLY_DEFINE_REPORT_FROM_WRAPPER(machine_apply_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_apply_build_from_machine_ir_report)

int machine_apply_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineApplyReport *out_report,
    MachineApplyError *error) {
    MachineApplyFile apply_file;
    int ok;

    machine_apply_file_init(&apply_file);
    ok = machine_apply_build_from_machine_ir_report_with_profile(report, profile, &apply_file, error) &&
        machine_apply_build_report_from_file(&apply_file, out_report, error);
    machine_apply_file_free(&apply_file);
    return ok;
}

int machine_apply_report_refresh(MachineApplyReport *report,
    MachineApplyError *error) {
    MachineApplyReport refreshed_report;
    int ok;

    if (!report) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-123: invalid report refresh contract");
        return 0;
    }
    machine_apply_report_init(&refreshed_report);
    ok = machine_apply_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_apply_report_free(report);
        *report = refreshed_report;
    } else {
        machine_apply_report_free(&refreshed_report);
    }
    return ok;
}

int machine_apply_dump_file(const MachineApplyFile *apply_file,
    char **out_text,
    MachineApplyError *error) {
    MachineApplyStringBuilder builder;
    MachineApplyHeaderSummary header_summary;
    MachineApplySummary apply_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&apply_summary, 0, sizeof(apply_summary));
    if (!apply_file || !out_text ||
        !machine_apply_verify_file(apply_file, error) ||
        !machine_apply_file_get_header_summary(apply_file, &header_summary) ||
        !machine_apply_file_get_apply_summary(apply_file, &apply_summary)) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-124: invalid dump contract");
        return 0;
    }

    if (!machine_apply_append_format(
            &builder,
            "machine_apply profile=%s commit=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_commit_resolution_kind_name(header_summary.commit_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_apply_append_format(
            &builder,
            "apply: resolution=%s kind=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_apply_resolution_kind_name(apply_summary.resolution_kind),
            machine_apply_kind_name(apply_summary.apply_kind),
            machine_commit_resolution_kind_name(apply_summary.commit_resolution_kind),
            machine_writeback_resolution_kind_name(apply_summary.writeback_resolution_kind),
            machine_mutation_resolution_kind_name(apply_summary.mutation_resolution_kind),
            machine_mutation_effect_kind_name(apply_summary.mutation_effect_kind),
            machine_transition_resolution_kind_name(apply_summary.transition_resolution_kind),
            machine_interp_action_kind_name(apply_summary.action_kind),
            (unsigned int)apply_summary.raw_byte,
            (unsigned int)apply_summary.tag_value,
            apply_summary.is_known ? "yes" : "no",
            apply_summary.tag_name ? apply_summary.tag_name : "-",
            apply_summary.instruction_byte_count) ||
        !machine_apply_append_payload_bytes(&builder, &apply_summary) ||
        !machine_apply_append_immediate_hint(&builder, &apply_summary) ||
        !machine_apply_append_format(
            &builder,
            " has-applied-state=%s applied-status=%s applied-pc=",
            apply_summary.has_applied_state ? "yes" : "no",
            apply_summary.has_applied_state ? machine_step_status_name(apply_summary.applied_status) : "-") ||
        !machine_apply_append_optional_pc(&builder, apply_summary.has_applied_state, apply_summary.applied_program_counter) ||
        !machine_apply_append_format(&builder, " applied-segment=") ||
        !machine_apply_append_optional_segment(&builder, apply_summary.has_applied_fetch, apply_summary.applied_segment_index) ||
        !machine_apply_append_format(&builder, " applied-byte=") ||
        !machine_apply_append_optional_byte(&builder, apply_summary.has_applied_fetch, apply_summary.applied_byte) ||
        !machine_apply_append_format(
            &builder,
            " has-preview-state=%s preview-status=%s preview-pc=",
            apply_summary.has_preview_state ? "yes" : "no",
            apply_summary.has_preview_state ? machine_step_status_name(apply_summary.preview_status) : "-") ||
        !machine_apply_append_optional_pc(&builder, apply_summary.has_preview_state, apply_summary.preview_program_counter) ||
        !machine_apply_append_format(&builder, " preview-segment=") ||
        !machine_apply_append_optional_segment(&builder, apply_summary.has_preview_fetch, apply_summary.preview_segment_index) ||
        !machine_apply_append_format(&builder, " preview-byte=") ||
        !machine_apply_append_optional_byte(&builder, apply_summary.has_preview_fetch, apply_summary.preview_byte) ||
        !machine_apply_append_targets(&builder, &apply_summary) ||
        !machine_apply_append_return_immediate(&builder, &apply_summary) ||
        !machine_apply_append_format(&builder, "\n")) {
        free(builder.data);
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-125: out of memory building dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_apply_build_dump_from_file(const MachineApplyFile *source,
    char **out_text,
    MachineApplyError *error) {
    return machine_apply_dump_file(source, out_text, error);
}

#define MACHINE_APPLY_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineApplyError *error) { \
    MachineApplyFile apply_file; \
    int ok; \
    machine_apply_file_init(&apply_file); \
    ok = builder(source, &apply_file, error) && \
        machine_apply_dump_file(&apply_file, out_text, error); \
    machine_apply_file_free(&apply_file); \
    return ok; \
}

MACHINE_APPLY_DEFINE_DUMP_FROM_WRAPPER(machine_apply_build_dump_from_machine_commit_file,
    MachineCommitFile, machine_apply_build_from_machine_commit_file)
MACHINE_APPLY_DEFINE_DUMP_FROM_WRAPPER(machine_apply_build_dump_from_machine_commit_report,
    MachineCommitReport, machine_apply_build_from_machine_commit_report)
MACHINE_APPLY_DEFINE_DUMP_FROM_WRAPPER(machine_apply_build_dump_from_machine_step_file,
    MachineStepFile, machine_apply_build_from_machine_step_file)
MACHINE_APPLY_DEFINE_DUMP_FROM_WRAPPER(machine_apply_build_dump_from_machine_step_report,
    MachineStepReport, machine_apply_build_from_machine_step_report)
MACHINE_APPLY_DEFINE_DUMP_FROM_WRAPPER(machine_apply_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_apply_build_from_machine_ir_report)

int machine_apply_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineApplyError *error) {
    MachineApplyFile apply_file;
    int ok;

    machine_apply_file_init(&apply_file);
    ok = machine_apply_build_from_machine_ir_report_with_profile(report, profile, &apply_file, error) &&
        machine_apply_dump_file(&apply_file, out_text, error);
    machine_apply_file_free(&apply_file);
    return ok;
}

int machine_apply_report_get_summary(const MachineApplyReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_apply_report_get_overview_artifact(const MachineApplyReport *report,
    MachineApplyReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->commit_report = &report->commit_report;
    out_artifact->apply_summary = &report->apply_summary;
    return 1;
}

int machine_apply_report_get_file(const MachineApplyReport *report,
    const MachineApplyFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_apply_report_get_commit_file(const MachineApplyReport *report,
    const MachineCommitFile **out_commit_file) {
    if (!report || !out_commit_file) {
        return 0;
    }
    *out_commit_file = &report->file.commit_file;
    return 1;
}

int machine_apply_report_get_commit_report(const MachineApplyReport *report,
    const MachineCommitReport **out_commit_report) {
    if (!report || !out_commit_report) {
        return 0;
    }
    *out_commit_report = &report->commit_report;
    return 1;
}

#define MACHINE_APPLY_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineApplyReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_APPLY_DEFINE_REPORT_ARTIFACT_GETTER(machine_apply_report_get_header_summary_artifact,
    header_summary, MachineApplyHeaderSummary)
MACHINE_APPLY_DEFINE_REPORT_ARTIFACT_GETTER(machine_apply_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineApplyTargetPolicySummary)
MACHINE_APPLY_DEFINE_REPORT_ARTIFACT_GETTER(machine_apply_report_get_apply_summary_artifact,
    apply_summary, MachineApplySummary)

int machine_apply_report_overview_artifact_get_commit_report(
    const MachineApplyReportOverviewArtifact *artifact,
    const MachineCommitReport **out_commit_report) {
    if (!artifact || !out_commit_report) {
        return 0;
    }
    *out_commit_report = artifact->commit_report;
    return 1;
}

int machine_apply_report_overview_artifact_get_apply_summary_artifact(
    const MachineApplyReportOverviewArtifact *artifact,
    const MachineApplySummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->apply_summary;
    return 1;
}

int machine_apply_dump_report(const MachineApplyReport *report,
    char **out_text,
    MachineApplyError *error) {
    MachineApplyStringBuilder builder;
    const MachineApplySummary *apply_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_apply_report_get_apply_summary_artifact(report, &apply_summary) ||
        !apply_summary) {
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-126: invalid report dump contract");
        return 0;
    }

    if (!machine_apply_append_format(
            &builder,
            "machine_apply profile=%s commit=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_commit_resolution_kind_name(report->header_summary.commit_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_apply_append_format(
            &builder,
            "apply: resolution=%s kind=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_apply_resolution_kind_name(apply_summary->resolution_kind),
            machine_apply_kind_name(apply_summary->apply_kind),
            machine_commit_resolution_kind_name(apply_summary->commit_resolution_kind),
            machine_writeback_resolution_kind_name(apply_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(apply_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(apply_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(apply_summary->transition_resolution_kind),
            machine_interp_action_kind_name(apply_summary->action_kind),
            (unsigned int)apply_summary->raw_byte,
            (unsigned int)apply_summary->tag_value,
            apply_summary->is_known ? "yes" : "no",
            apply_summary->tag_name ? apply_summary->tag_name : "-",
            apply_summary->instruction_byte_count) ||
        !machine_apply_append_payload_bytes(&builder, apply_summary) ||
        !machine_apply_append_immediate_hint(&builder, apply_summary) ||
        !machine_apply_append_format(
            &builder,
            " has-applied-state=%s applied-status=%s applied-pc=",
            apply_summary->has_applied_state ? "yes" : "no",
            apply_summary->has_applied_state ? machine_step_status_name(apply_summary->applied_status) : "-") ||
        !machine_apply_append_optional_pc(&builder, apply_summary->has_applied_state, apply_summary->applied_program_counter) ||
        !machine_apply_append_format(&builder, " applied-segment=") ||
        !machine_apply_append_optional_segment(&builder, apply_summary->has_applied_fetch, apply_summary->applied_segment_index) ||
        !machine_apply_append_format(&builder, " applied-byte=") ||
        !machine_apply_append_optional_byte(&builder, apply_summary->has_applied_fetch, apply_summary->applied_byte) ||
        !machine_apply_append_format(
            &builder,
            " has-preview-state=%s preview-status=%s preview-pc=",
            apply_summary->has_preview_state ? "yes" : "no",
            apply_summary->has_preview_state ? machine_step_status_name(apply_summary->preview_status) : "-") ||
        !machine_apply_append_optional_pc(&builder, apply_summary->has_preview_state, apply_summary->preview_program_counter) ||
        !machine_apply_append_format(&builder, " preview-segment=") ||
        !machine_apply_append_optional_segment(&builder, apply_summary->has_preview_fetch, apply_summary->preview_segment_index) ||
        !machine_apply_append_format(&builder, " preview-byte=") ||
        !machine_apply_append_optional_byte(&builder, apply_summary->has_preview_fetch, apply_summary->preview_byte) ||
        !machine_apply_append_targets(&builder, apply_summary) ||
        !machine_apply_append_return_immediate(&builder, apply_summary) ||
        !machine_apply_append_format(&builder, "\nreport_overview:\n") ||
        !machine_apply_append_format(
            &builder,
            "  origin: commit=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_commit_resolution_kind_name(report->header_summary.commit_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer) ||
        !machine_apply_append_format(
            &builder,
            "  policy: profile=%s state=%s register=%s slot=%s call=%s preview=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.applies_no_op_state ? "yes" : "no",
            report->target_policy_summary.surfaces_register_application ? "yes" : "no",
            report->target_policy_summary.surfaces_slot_application ? "yes" : "no",
            report->target_policy_summary.surfaces_call_application ? "yes" : "no",
            report->target_policy_summary.surfaces_state_preview ? "yes" : "no") ||
        !machine_apply_append_format(
            &builder,
            "  apply: resolution=%s kind=%s applied=%s preview=%s imm=%s apc=",
            machine_apply_resolution_kind_name(apply_summary->resolution_kind),
            machine_apply_kind_name(apply_summary->apply_kind),
            apply_summary->has_applied_state ? "yes" : "no",
            apply_summary->has_preview_state ? "yes" : "no",
            apply_summary->has_immediate_hint ? "" : "-")) {
        free(builder.data);
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-127: out of memory building report dump");
        return 0;
    }
    if (apply_summary->has_immediate_hint &&
        !machine_apply_append_format(&builder, "%zu", apply_summary->immediate_hint)) {
        free(builder.data);
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-128: out of memory appending report imm");
        return 0;
    }
    if (!machine_apply_append_optional_pc(&builder, apply_summary->has_applied_state, apply_summary->applied_program_counter) ||
        !machine_apply_append_format(&builder, " ppc=") ||
        !machine_apply_append_optional_pc(&builder, apply_summary->has_preview_state, apply_summary->preview_program_counter) ||
        !machine_apply_append_targets(&builder, apply_summary) ||
        !machine_apply_append_return_immediate(&builder, apply_summary) ||
        !machine_apply_append_format(&builder, "\n")) {
        free(builder.data);
        machine_apply_set_error(error, 0, 0, "MACHINE-APPLY-129: out of memory terminating report dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_apply_build_report_dump_from_file(const MachineApplyFile *source,
    char **out_text,
    MachineApplyError *error) {
    MachineApplyReport report;
    int ok;

    machine_apply_report_init(&report);
    ok = machine_apply_build_report_from_file(source, &report, error) &&
        machine_apply_dump_report(&report, out_text, error);
    machine_apply_report_free(&report);
    return ok;
}

#define MACHINE_APPLY_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineApplyError *error) { \
    MachineApplyReport report; \
    int ok; \
    machine_apply_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_apply_dump_report(&report, out_text, error); \
    machine_apply_report_free(&report); \
    return ok; \
}

MACHINE_APPLY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_apply_build_report_dump_from_machine_commit_file,
    MachineCommitFile, machine_apply_build_report_from_machine_commit_file)
MACHINE_APPLY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_apply_build_report_dump_from_machine_commit_report,
    MachineCommitReport, machine_apply_build_report_from_machine_commit_report)
MACHINE_APPLY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_apply_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_apply_build_report_from_machine_step_file)
MACHINE_APPLY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_apply_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_apply_build_report_from_machine_step_report)
MACHINE_APPLY_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_apply_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_apply_build_report_from_machine_ir_report)

int machine_apply_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineApplyError *error) {
    MachineApplyReport report_file;
    int ok;

    machine_apply_report_init(&report_file);
    ok = machine_apply_build_report_from_machine_ir_report_with_profile(report, profile, &report_file, error) &&
        machine_apply_dump_report(&report_file, out_text, error);
    machine_apply_report_free(&report_file);
    return ok;
}
