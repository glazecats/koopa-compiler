#include "machine/commit.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineCommitStringBuilder;

static void machine_commit_set_error(MachineCommitError *error, int line, int column, const char *fmt, ...);
static int machine_commit_append_format(MachineCommitStringBuilder *builder, const char *fmt, ...);
static int machine_commit_append_targets(MachineCommitStringBuilder *builder,
    const MachineCommitSummary *commit_summary);
static int machine_commit_append_return_immediate(MachineCommitStringBuilder *builder,
    const MachineCommitSummary *commit_summary);

static void machine_commit_set_error(MachineCommitError *error, int line, int column, const char *fmt, ...) {
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

static int machine_commit_append_format(MachineCommitStringBuilder *builder, const char *fmt, ...) {
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

static int machine_commit_append_targets(MachineCommitStringBuilder *builder,
    const MachineCommitSummary *commit_summary) {
    if (!builder || !commit_summary) {
        return 0;
    }
    if (!commit_summary->has_primary_target_block) {
        return machine_commit_append_format(builder, " targets=[]");
    }
    if (!commit_summary->has_secondary_target_block) {
        return machine_commit_append_format(
            builder,
            " targets=[%zu]",
            commit_summary->primary_target_block_index);
    }
    return machine_commit_append_format(
        builder,
        " targets=[%zu,%zu]",
        commit_summary->primary_target_block_index,
        commit_summary->secondary_target_block_index);
}

static int machine_commit_append_return_immediate(MachineCommitStringBuilder *builder,
    const MachineCommitSummary *commit_summary) {
    if (!builder || !commit_summary) {
        return 0;
    }
    if (!commit_summary->has_return_immediate) {
        return machine_commit_append_format(builder, " return-imm=-");
    }
    return machine_commit_append_format(
        builder,
        " return-imm=%zu",
        commit_summary->return_immediate);
}

void machine_commit_file_init(MachineCommitFile *commit_file) {
    if (!commit_file) {
        return;
    }
    machine_writeback_file_init(&commit_file->writeback_file);
    commit_file->resolution_kind = MACHINE_COMMIT_RESOLUTION_NONE;
    commit_file->commit_kind = MACHINE_COMMIT_KIND_NONE;
    commit_file->has_committed_state = 0;
    commit_file->committed_status = MACHINE_STEP_STATUS_READY;
    commit_file->program_counter = 0u;
    commit_file->stack_pointer = 0u;
    commit_file->has_current_fetch = 0;
    commit_file->current_segment_index = 0u;
    commit_file->current_byte = 0u;
    commit_file->current_byte_memory_offset = 0u;
}

void machine_commit_file_free(MachineCommitFile *commit_file) {
    if (!commit_file) {
        return;
    }
    machine_writeback_file_free(&commit_file->writeback_file);
    machine_commit_file_init(commit_file);
}

void machine_commit_report_init(MachineCommitReport *report) {
    if (!report) {
        return;
    }
    machine_commit_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_writeback_report_init(&report->writeback_report);
    memset(&report->commit_summary, 0, sizeof(report->commit_summary));
    memset(&report->current_fetch_summary, 0, sizeof(report->current_fetch_summary));
}

void machine_commit_report_free(MachineCommitReport *report) {
    if (!report) {
        return;
    }
    machine_writeback_report_free(&report->writeback_report);
    machine_commit_file_free(&report->file);
    machine_commit_report_init(report);
}

const char *machine_commit_resolution_kind_name(MachineCommitResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_COMMIT_RESOLUTION_NONE:
            return "none";
        case MACHINE_COMMIT_RESOLUTION_COMMITTED_STATE:
            return "committed-state";
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_REGISTER_COMMIT:
            return "deferred-register-commit";
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_LOCAL_COMMIT:
            return "deferred-local-commit";
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_GLOBAL_COMMIT:
            return "deferred-global-commit";
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_CALL_COMMIT:
            return "deferred-call-commit";
        case MACHINE_COMMIT_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_COMMIT_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_commit_kind_name(MachineCommitKind commit_kind) {
    switch (commit_kind) {
        case MACHINE_COMMIT_KIND_NONE:
            return "none";
        case MACHINE_COMMIT_KIND_STATE:
            return "state";
        case MACHINE_COMMIT_KIND_REGISTER:
            return "register";
        case MACHINE_COMMIT_KIND_LOCAL_SLOT:
            return "local-slot";
        case MACHINE_COMMIT_KIND_GLOBAL_SLOT:
            return "global-slot";
        case MACHINE_COMMIT_KIND_CALL:
            return "call";
    }
    return "unknown";
}

int machine_commit_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineCommitTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->commits_no_op_state = 1;
    out_summary->defers_register_commit = 1;
    out_summary->defers_slot_commit = 1;
    out_summary->defers_call_commit = 1;
    return 1;
}

int machine_commit_file_get_target_policy_summary(const MachineCommitFile *commit_file,
    MachineCommitTargetPolicySummary *out_summary) {
    if (!commit_file || !out_summary) {
        return 0;
    }
    return machine_commit_get_target_policy_summary(
        commit_file->writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.target_profile,
        out_summary);
}

int machine_commit_file_get_summary(const MachineCommitFile *commit_file,
    size_t *out_mapped_byte_count) {
    if (!commit_file) {
        return 0;
    }
    return machine_writeback_file_get_summary(&commit_file->writeback_file, out_mapped_byte_count);
}

int machine_commit_file_get_header_summary(const MachineCommitFile *commit_file,
    MachineCommitHeaderSummary *out_summary) {
    MachineWritebackHeaderSummary writeback_header_summary;

    memset(&writeback_header_summary, 0, sizeof(writeback_header_summary));
    if (!commit_file || !out_summary ||
        !machine_writeback_file_get_header_summary(&commit_file->writeback_file, &writeback_header_summary)) {
        return 0;
    }
    out_summary->target_profile = writeback_header_summary.target_profile;
    out_summary->writeback_resolution_kind = commit_file->writeback_file.resolution_kind;
    out_summary->origin_step_status = writeback_header_summary.origin_step_status;
    out_summary->origin_program_counter = writeback_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = writeback_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = writeback_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = writeback_header_summary.mapped_byte_count;
    return 1;
}

int machine_commit_file_get_writeback_summary(const MachineCommitFile *commit_file,
    MachineWritebackSummary *out_summary) {
    if (!commit_file || !out_summary) {
        return 0;
    }
    return machine_writeback_file_get_writeback_summary(&commit_file->writeback_file, out_summary);
}

int machine_commit_file_get_commit_summary(const MachineCommitFile *commit_file,
    MachineCommitSummary *out_summary) {
    MachineWritebackSummary writeback_summary;

    memset(&writeback_summary, 0, sizeof(writeback_summary));
    if (!commit_file || !out_summary ||
        !machine_commit_file_get_writeback_summary(commit_file, &writeback_summary)) {
        return 0;
    }
    out_summary->resolution_kind = commit_file->resolution_kind;
    out_summary->commit_kind = commit_file->commit_kind;
    out_summary->writeback_resolution_kind = writeback_summary.resolution_kind;
    out_summary->writeback_commit_kind = writeback_summary.commit_kind;
    out_summary->mutation_resolution_kind = writeback_summary.mutation_resolution_kind;
    out_summary->mutation_effect_kind = writeback_summary.mutation_effect_kind;
    out_summary->state_resolution_kind = writeback_summary.state_resolution_kind;
    out_summary->transition_resolution_kind = writeback_summary.transition_resolution_kind;
    out_summary->action_kind = writeback_summary.action_kind;
    out_summary->payload_kind = writeback_summary.payload_kind;
    out_summary->tag_class = writeback_summary.tag_class;
    out_summary->raw_byte = writeback_summary.raw_byte;
    out_summary->tag_value = writeback_summary.tag_value;
    out_summary->is_known = writeback_summary.is_known;
    out_summary->tag_name = writeback_summary.tag_name;
    out_summary->instruction_byte_count = writeback_summary.instruction_byte_count;
    out_summary->has_committed_state = commit_file->has_committed_state;
    out_summary->committed_status = commit_file->committed_status;
    out_summary->program_counter = commit_file->program_counter;
    out_summary->stack_pointer = commit_file->stack_pointer;
    out_summary->has_current_fetch = commit_file->has_current_fetch;
    out_summary->current_segment_index = commit_file->current_segment_index;
    out_summary->current_byte = commit_file->current_byte;
    out_summary->current_byte_memory_offset = commit_file->current_byte_memory_offset;
    out_summary->has_primary_target_block = writeback_summary.has_primary_target_block;
    out_summary->primary_target_block_index = writeback_summary.primary_target_block_index;
    out_summary->has_secondary_target_block = writeback_summary.has_secondary_target_block;
    out_summary->secondary_target_block_index = writeback_summary.secondary_target_block_index;
    out_summary->has_return_immediate = writeback_summary.has_return_immediate;
    out_summary->return_immediate = writeback_summary.return_immediate;
    return 1;
}

int machine_commit_file_get_current_fetch_summary(const MachineCommitFile *commit_file,
    MachineCommitCurrentFetchSummary *out_summary) {
    MachineRuntimeSegmentSummary segment_summary;
    const MachineRuntimeSegment *segment = NULL;

    memset(&segment_summary, 0, sizeof(segment_summary));
    if (!commit_file || !commit_file->has_current_fetch) {
        return 0;
    }
    if (!machine_runtime_file_get_segment(
            &commit_file->writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file,
            commit_file->current_segment_index,
            &segment) ||
        !segment ||
        !machine_runtime_segment_get_summary(segment, &segment_summary)) {
        return 0;
    }
    if (!out_summary) {
        return 1;
    }
    out_summary->byte_virtual_address = commit_file->program_counter;
    out_summary->byte_memory_offset = commit_file->current_byte_memory_offset;
    out_summary->segment_index = commit_file->current_segment_index;
    out_summary->segment_name = segment_summary.name;
    out_summary->byte_value = commit_file->current_byte;
    return 1;
}

int machine_commit_verify_file(const MachineCommitFile *commit_file,
    MachineCommitError *error) {
    MachineWritebackSummary writeback_summary;
    MachineStateSummary state_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    const MachineRuntimeSegment *segment = NULL;
    unsigned char current_byte = 0u;

    memset(&writeback_summary, 0, sizeof(writeback_summary));
    memset(&state_summary, 0, sizeof(state_summary));
    memset(&runtime_memory_summary, 0, sizeof(runtime_memory_summary));
    if (!commit_file) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-100: invalid commit file");
        return 0;
    }
    if (!machine_writeback_verify_file(&commit_file->writeback_file, NULL) ||
        !machine_commit_file_get_writeback_summary(commit_file, &writeback_summary) ||
        !machine_state_file_get_state_summary(&commit_file->writeback_file.mutation_file.state_file, &state_summary) ||
        !machine_state_file_get_runtime_memory_summary(
            &commit_file->writeback_file.mutation_file.state_file, &runtime_memory_summary)) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-101: invalid writeback input");
        return 0;
    }

    switch (commit_file->resolution_kind) {
        case MACHINE_COMMIT_RESOLUTION_COMMITTED_STATE:
            if (commit_file->writeback_file.resolution_kind != MACHINE_WRITEBACK_RESOLUTION_COMMITTED_NO_OP ||
                !commit_file->has_committed_state ||
                !state_summary.has_state ||
                commit_file->commit_kind != MACHINE_COMMIT_KIND_STATE ||
                commit_file->committed_status != state_summary.state_status ||
                commit_file->program_counter != state_summary.program_counter ||
                commit_file->stack_pointer != state_summary.stack_pointer ||
                commit_file->has_current_fetch != state_summary.has_current_fetch ||
                (state_summary.has_current_fetch &&
                    (commit_file->current_segment_index != state_summary.current_segment_index ||
                        commit_file->current_byte != state_summary.current_byte ||
                        commit_file->current_byte_memory_offset != state_summary.current_byte_memory_offset)) ||
                (state_summary.has_current_fetch &&
                    (!machine_runtime_file_get_segment(
                         &commit_file->writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file,
                         commit_file->current_segment_index,
                         &segment) ||
                        !segment || !segment->executable ||
                        commit_file->program_counter < runtime_memory_summary.base_virtual_address ||
                        commit_file->current_byte_memory_offset !=
                            commit_file->program_counter - runtime_memory_summary.base_virtual_address ||
                        !machine_runtime_file_get_memory_byte_at_virtual_address(
                            &commit_file->writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file,
                            commit_file->program_counter,
                            &current_byte) ||
                        current_byte != commit_file->current_byte ||
                        !machine_commit_file_get_current_fetch_summary(commit_file, NULL)))) {
                machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-102: invalid committed state");
                return 0;
            }
            break;
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_REGISTER_COMMIT:
            if (commit_file->writeback_file.resolution_kind !=
                    MACHINE_WRITEBACK_RESOLUTION_DEFERRED_REGISTER_WRITEBACK ||
                commit_file->commit_kind != MACHINE_COMMIT_KIND_REGISTER ||
                commit_file->has_committed_state) {
                machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-103: invalid deferred register commit");
                return 0;
            }
            break;
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_LOCAL_COMMIT:
            if (commit_file->writeback_file.resolution_kind !=
                    MACHINE_WRITEBACK_RESOLUTION_DEFERRED_LOCAL_WRITEBACK ||
                commit_file->commit_kind != MACHINE_COMMIT_KIND_LOCAL_SLOT ||
                commit_file->has_committed_state) {
                machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-104: invalid deferred local commit");
                return 0;
            }
            break;
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_GLOBAL_COMMIT:
            if (commit_file->writeback_file.resolution_kind !=
                    MACHINE_WRITEBACK_RESOLUTION_DEFERRED_GLOBAL_WRITEBACK ||
                commit_file->commit_kind != MACHINE_COMMIT_KIND_GLOBAL_SLOT ||
                commit_file->has_committed_state) {
                machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-105: invalid deferred global commit");
                return 0;
            }
            break;
        case MACHINE_COMMIT_RESOLUTION_DEFERRED_CALL_COMMIT:
            if (commit_file->writeback_file.resolution_kind !=
                    MACHINE_WRITEBACK_RESOLUTION_DEFERRED_CALL_WRITEBACK ||
                commit_file->commit_kind != MACHINE_COMMIT_KIND_CALL ||
                commit_file->has_committed_state) {
                machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-106: invalid deferred call commit");
                return 0;
            }
            break;
        case MACHINE_COMMIT_RESOLUTION_BLOCKED_ON_CONTROL:
            if (commit_file->writeback_file.resolution_kind != MACHINE_WRITEBACK_RESOLUTION_BLOCKED_ON_CONTROL ||
                commit_file->commit_kind != MACHINE_COMMIT_KIND_NONE ||
                commit_file->has_committed_state) {
                machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-107: invalid blocked control commit");
                return 0;
            }
            break;
        case MACHINE_COMMIT_RESOLUTION_BLOCKED_UNSUPPORTED:
            if (commit_file->writeback_file.resolution_kind != MACHINE_WRITEBACK_RESOLUTION_BLOCKED_UNSUPPORTED ||
                commit_file->commit_kind != MACHINE_COMMIT_KIND_NONE ||
                commit_file->has_committed_state) {
                machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-108: invalid blocked unsupported commit");
                return 0;
            }
            break;
        case MACHINE_COMMIT_RESOLUTION_NONE:
        default:
            machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-109: invalid commit resolution kind");
            return 0;
    }
    return 1;
}

int machine_commit_clone_file(const MachineCommitFile *source,
    MachineCommitFile *out_clone,
    MachineCommitError *error) {
    if (!source || !out_clone) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-110: invalid clone contract");
        return 0;
    }
    if (!machine_commit_verify_file(source, error)) {
        return 0;
    }

    machine_commit_file_free(out_clone);
    if (!machine_writeback_clone_file(&source->writeback_file, &out_clone->writeback_file, NULL)) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-111: failed cloning writeback input");
        machine_commit_file_free(out_clone);
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->commit_kind = source->commit_kind;
    out_clone->has_committed_state = source->has_committed_state;
    out_clone->committed_status = source->committed_status;
    out_clone->program_counter = source->program_counter;
    out_clone->stack_pointer = source->stack_pointer;
    out_clone->has_current_fetch = source->has_current_fetch;
    out_clone->current_segment_index = source->current_segment_index;
    out_clone->current_byte = source->current_byte;
    out_clone->current_byte_memory_offset = source->current_byte_memory_offset;
    return 1;
}

int machine_commit_build_from_machine_writeback_file(const MachineWritebackFile *writeback_file,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error) {
    MachineStateSummary state_summary;

    memset(&state_summary, 0, sizeof(state_summary));
    if (!writeback_file || !out_commit_file) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-112: invalid build contract");
        return 0;
    }
    if (!machine_writeback_verify_file(writeback_file, NULL)) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-113: invalid writeback input");
        return 0;
    }

    machine_commit_file_free(out_commit_file);
    if (!machine_writeback_clone_file(writeback_file, &out_commit_file->writeback_file, NULL)) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-114: failed cloning writeback input");
        machine_commit_file_free(out_commit_file);
        return 0;
    }

    switch (writeback_file->resolution_kind) {
        case MACHINE_WRITEBACK_RESOLUTION_COMMITTED_NO_OP:
            if (!machine_state_file_get_state_summary(
                    &writeback_file->mutation_file.state_file, &state_summary) ||
                !state_summary.has_state) {
                machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-115: committed no-op requires committed state");
                machine_commit_file_free(out_commit_file);
                return 0;
            }
            out_commit_file->resolution_kind = MACHINE_COMMIT_RESOLUTION_COMMITTED_STATE;
            out_commit_file->commit_kind = MACHINE_COMMIT_KIND_STATE;
            out_commit_file->has_committed_state = 1;
            out_commit_file->committed_status = state_summary.state_status;
            out_commit_file->program_counter = state_summary.program_counter;
            out_commit_file->stack_pointer = state_summary.stack_pointer;
            out_commit_file->has_current_fetch = state_summary.has_current_fetch;
            out_commit_file->current_segment_index = state_summary.current_segment_index;
            out_commit_file->current_byte = state_summary.current_byte;
            out_commit_file->current_byte_memory_offset = state_summary.current_byte_memory_offset;
            break;
        case MACHINE_WRITEBACK_RESOLUTION_DEFERRED_REGISTER_WRITEBACK:
            out_commit_file->resolution_kind = MACHINE_COMMIT_RESOLUTION_DEFERRED_REGISTER_COMMIT;
            out_commit_file->commit_kind = MACHINE_COMMIT_KIND_REGISTER;
            break;
        case MACHINE_WRITEBACK_RESOLUTION_DEFERRED_LOCAL_WRITEBACK:
            out_commit_file->resolution_kind = MACHINE_COMMIT_RESOLUTION_DEFERRED_LOCAL_COMMIT;
            out_commit_file->commit_kind = MACHINE_COMMIT_KIND_LOCAL_SLOT;
            break;
        case MACHINE_WRITEBACK_RESOLUTION_DEFERRED_GLOBAL_WRITEBACK:
            out_commit_file->resolution_kind = MACHINE_COMMIT_RESOLUTION_DEFERRED_GLOBAL_COMMIT;
            out_commit_file->commit_kind = MACHINE_COMMIT_KIND_GLOBAL_SLOT;
            break;
        case MACHINE_WRITEBACK_RESOLUTION_DEFERRED_CALL_WRITEBACK:
            out_commit_file->resolution_kind = MACHINE_COMMIT_RESOLUTION_DEFERRED_CALL_COMMIT;
            out_commit_file->commit_kind = MACHINE_COMMIT_KIND_CALL;
            break;
        case MACHINE_WRITEBACK_RESOLUTION_BLOCKED_ON_CONTROL:
            out_commit_file->resolution_kind = MACHINE_COMMIT_RESOLUTION_BLOCKED_ON_CONTROL;
            out_commit_file->commit_kind = MACHINE_COMMIT_KIND_NONE;
            break;
        case MACHINE_WRITEBACK_RESOLUTION_BLOCKED_UNSUPPORTED:
            out_commit_file->resolution_kind = MACHINE_COMMIT_RESOLUTION_BLOCKED_UNSUPPORTED;
            out_commit_file->commit_kind = MACHINE_COMMIT_KIND_NONE;
            break;
        case MACHINE_WRITEBACK_RESOLUTION_NONE:
        default:
            machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-116: unsupported writeback resolution");
            machine_commit_file_free(out_commit_file);
            return 0;
    }

    if (!machine_commit_verify_file(out_commit_file, error)) {
        machine_commit_file_free(out_commit_file);
        return 0;
    }
    return 1;
}

int machine_commit_build_from_machine_writeback_report(const MachineWritebackReport *report,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error) {
    const MachineWritebackFile *writeback_file = NULL;

    if (!report) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-117: invalid writeback-report build contract");
        return 0;
    }
    if (!machine_writeback_report_get_file(report, &writeback_file) || !writeback_file) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-118: failed recovering writeback file from report");
        return 0;
    }
    return machine_commit_build_from_machine_writeback_file(writeback_file, out_commit_file, error);
}

#define MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineCommitFile *out_commit_file, MachineCommitError *error) { \
    MachineWritebackFile writeback_file; \
    int ok; \
    machine_writeback_file_init(&writeback_file); \
    ok = builder(source, &writeback_file, NULL) && \
        machine_commit_build_from_machine_writeback_file(&writeback_file, out_commit_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-119: failed building commit wrapper"); \
    } \
    machine_writeback_file_free(&writeback_file); \
    return ok; \
}

MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_mutation_file,
    MachineMutationFile, machine_writeback_build_from_machine_mutation_file)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_mutation_report,
    MachineMutationReport, machine_writeback_build_from_machine_mutation_report)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_state_file,
    MachineStateFile, machine_writeback_build_from_machine_state_file)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_state_report,
    MachineStateReport, machine_writeback_build_from_machine_state_report)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_transition_file,
    MachineTransitionFile, machine_writeback_build_from_machine_transition_file)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_transition_report,
    MachineTransitionReport, machine_writeback_build_from_machine_transition_report)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_interp_file,
    MachineInterpFile, machine_writeback_build_from_machine_interp_file)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_interp_report,
    MachineInterpReport, machine_writeback_build_from_machine_interp_report)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_writeback_build_from_machine_payload_decode_file)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_writeback_build_from_machine_payload_decode_report)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_decode_file,
    MachineDecodeFile, machine_writeback_build_from_machine_decode_file)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_decode_report,
    MachineDecodeReport, machine_writeback_build_from_machine_decode_report)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_step_file,
    MachineStepFile, machine_writeback_build_from_machine_step_file)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_step_report,
    MachineStepReport, machine_writeback_build_from_machine_step_report)
MACHINE_COMMIT_DEFINE_BUILD_FROM_WRAPPER(machine_commit_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_writeback_build_from_machine_ir_report)

int machine_commit_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error) {
    MachineWritebackFile writeback_file;
    int ok;

    machine_writeback_file_init(&writeback_file);
    ok = machine_writeback_build_from_machine_ir_report_with_profile(report, profile, &writeback_file, NULL) &&
        machine_commit_build_from_machine_writeback_file(&writeback_file, out_commit_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-120: failed building profiled commit wrapper");
    }
    machine_writeback_file_free(&writeback_file);
    return ok;
}

int machine_commit_build_report_from_file(const MachineCommitFile *source,
    MachineCommitReport *out_report,
    MachineCommitError *error) {
    if (!source || !out_report) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-121: invalid report build contract");
        return 0;
    }
    if (!machine_commit_verify_file(source, error)) {
        return 0;
    }

    machine_commit_report_free(out_report);
    if (!machine_commit_clone_file(source, &out_report->file, error)) {
        machine_commit_report_free(out_report);
        return 0;
    }
    if (!machine_commit_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_commit_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_writeback_build_report_from_file(&out_report->file.writeback_file, &out_report->writeback_report, NULL) ||
        !machine_commit_file_get_commit_summary(&out_report->file, &out_report->commit_summary)) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-122: failed summarizing commit report");
        machine_commit_report_free(out_report);
        return 0;
    }
    if (out_report->file.has_current_fetch &&
        !machine_commit_file_get_current_fetch_summary(&out_report->file, &out_report->current_fetch_summary)) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-123: failed summarizing committed fetch");
        machine_commit_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineCommitReport *out_report, MachineCommitError *error) { \
    MachineCommitFile commit_file; \
    int ok; \
    machine_commit_file_init(&commit_file); \
    ok = builder(source, &commit_file, error) && \
        machine_commit_build_report_from_file(&commit_file, out_report, error); \
    machine_commit_file_free(&commit_file); \
    return ok; \
}

MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_writeback_file,
    MachineWritebackFile, machine_commit_build_from_machine_writeback_file)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_writeback_report,
    MachineWritebackReport, machine_commit_build_from_machine_writeback_report)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_mutation_file,
    MachineMutationFile, machine_commit_build_from_machine_mutation_file)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_mutation_report,
    MachineMutationReport, machine_commit_build_from_machine_mutation_report)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_state_file,
    MachineStateFile, machine_commit_build_from_machine_state_file)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_state_report,
    MachineStateReport, machine_commit_build_from_machine_state_report)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_transition_file,
    MachineTransitionFile, machine_commit_build_from_machine_transition_file)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_transition_report,
    MachineTransitionReport, machine_commit_build_from_machine_transition_report)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_interp_file,
    MachineInterpFile, machine_commit_build_from_machine_interp_file)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_interp_report,
    MachineInterpReport, machine_commit_build_from_machine_interp_report)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_commit_build_from_machine_payload_decode_file)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_commit_build_from_machine_payload_decode_report)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_decode_file,
    MachineDecodeFile, machine_commit_build_from_machine_decode_file)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_decode_report,
    MachineDecodeReport, machine_commit_build_from_machine_decode_report)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_step_file,
    MachineStepFile, machine_commit_build_from_machine_step_file)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_step_report,
    MachineStepReport, machine_commit_build_from_machine_step_report)
MACHINE_COMMIT_DEFINE_REPORT_FROM_WRAPPER(machine_commit_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_commit_build_from_machine_ir_report)

int machine_commit_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineCommitReport *out_report,
    MachineCommitError *error) {
    MachineCommitFile commit_file;
    int ok;

    machine_commit_file_init(&commit_file);
    ok = machine_commit_build_from_machine_ir_report_with_profile(report, profile, &commit_file, error) &&
        machine_commit_build_report_from_file(&commit_file, out_report, error);
    machine_commit_file_free(&commit_file);
    return ok;
}

int machine_commit_report_refresh(MachineCommitReport *report,
    MachineCommitError *error) {
    MachineCommitReport refreshed_report;
    int ok;

    if (!report) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-124: invalid report refresh contract");
        return 0;
    }
    machine_commit_report_init(&refreshed_report);
    ok = machine_commit_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_commit_report_free(report);
        *report = refreshed_report;
    } else {
        machine_commit_report_free(&refreshed_report);
    }
    return ok;
}

int machine_commit_dump_file(const MachineCommitFile *commit_file,
    char **out_text,
    MachineCommitError *error) {
    MachineCommitStringBuilder builder;
    MachineCommitHeaderSummary header_summary;
    MachineCommitSummary commit_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&commit_summary, 0, sizeof(commit_summary));
    if (!commit_file || !out_text ||
        !machine_commit_verify_file(commit_file, error) ||
        !machine_commit_file_get_header_summary(commit_file, &header_summary) ||
        !machine_commit_file_get_commit_summary(commit_file, &commit_summary)) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-125: invalid dump contract");
        return 0;
    }

    if (!machine_commit_append_format(
            &builder,
            "machine_commit profile=%s elf_origin=%s elf_semantics=%s writeback=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_elf_target_profile_name(
                commit_file->writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                commit_file->writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_writeback_resolution_kind_name(header_summary.writeback_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count) ||
        !machine_commit_append_format(
            &builder,
            "commit: resolution=%s kind=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu has-committed-state=%s status=%s pc=%s",
            machine_commit_resolution_kind_name(commit_summary.resolution_kind),
            machine_commit_kind_name(commit_summary.commit_kind),
            machine_writeback_resolution_kind_name(commit_summary.writeback_resolution_kind),
            machine_mutation_resolution_kind_name(commit_summary.mutation_resolution_kind),
            machine_mutation_effect_kind_name(commit_summary.mutation_effect_kind),
            machine_transition_resolution_kind_name(commit_summary.transition_resolution_kind),
            machine_interp_action_kind_name(commit_summary.action_kind),
            (unsigned int)commit_summary.raw_byte,
            (unsigned int)commit_summary.tag_value,
            commit_summary.is_known ? "yes" : "no",
            commit_summary.tag_name ? commit_summary.tag_name : "-",
            commit_summary.instruction_byte_count,
            commit_summary.has_committed_state ? "yes" : "no",
            commit_summary.has_committed_state ? machine_step_status_name(commit_summary.committed_status) : "-",
            commit_summary.has_committed_state ? "" : "-")) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-126: out of memory building dump");
        return 0;
    }
    if (commit_summary.has_committed_state &&
        !machine_commit_append_format(&builder, "0x%zx", commit_summary.program_counter)) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-127: out of memory appending committed pc");
        return 0;
    }
    if (!machine_commit_append_format(
            &builder,
            " current-segment=%s",
            commit_summary.has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-128: out of memory appending current segment");
        return 0;
    }
    if (commit_summary.has_current_fetch &&
        !machine_commit_append_format(&builder, "%zu", commit_summary.current_segment_index)) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-129: out of memory appending current segment value");
        return 0;
    }
    if (!machine_commit_append_format(
            &builder,
            " current-byte=%s",
            commit_summary.has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-130: out of memory appending current byte");
        return 0;
    }
    if (commit_summary.has_current_fetch &&
        !machine_commit_append_format(&builder, "0x%02x", (unsigned int)commit_summary.current_byte)) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-131: out of memory appending current byte value");
        return 0;
    }
    if (!machine_commit_append_targets(&builder, &commit_summary) ||
        !machine_commit_append_return_immediate(&builder, &commit_summary) ||
        !machine_commit_append_format(&builder, "\n")) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-132: out of memory terminating dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_commit_build_dump_from_file(const MachineCommitFile *source,
    char **out_text,
    MachineCommitError *error) {
    return machine_commit_dump_file(source, out_text, error);
}

#define MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineCommitError *error) { \
    MachineCommitFile commit_file; \
    int ok; \
    machine_commit_file_init(&commit_file); \
    ok = builder(source, &commit_file, error) && \
        machine_commit_dump_file(&commit_file, out_text, error); \
    machine_commit_file_free(&commit_file); \
    return ok; \
}

MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_writeback_file,
    MachineWritebackFile, machine_commit_build_from_machine_writeback_file)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_writeback_report,
    MachineWritebackReport, machine_commit_build_from_machine_writeback_report)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_mutation_file,
    MachineMutationFile, machine_commit_build_from_machine_mutation_file)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_mutation_report,
    MachineMutationReport, machine_commit_build_from_machine_mutation_report)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_state_file,
    MachineStateFile, machine_commit_build_from_machine_state_file)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_state_report,
    MachineStateReport, machine_commit_build_from_machine_state_report)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_transition_file,
    MachineTransitionFile, machine_commit_build_from_machine_transition_file)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_transition_report,
    MachineTransitionReport, machine_commit_build_from_machine_transition_report)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_interp_file,
    MachineInterpFile, machine_commit_build_from_machine_interp_file)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_interp_report,
    MachineInterpReport, machine_commit_build_from_machine_interp_report)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_commit_build_from_machine_payload_decode_file)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_commit_build_from_machine_payload_decode_report)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_decode_file,
    MachineDecodeFile, machine_commit_build_from_machine_decode_file)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_decode_report,
    MachineDecodeReport, machine_commit_build_from_machine_decode_report)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_step_file,
    MachineStepFile, machine_commit_build_from_machine_step_file)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_step_report,
    MachineStepReport, machine_commit_build_from_machine_step_report)
MACHINE_COMMIT_DEFINE_DUMP_FROM_WRAPPER(machine_commit_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_commit_build_from_machine_ir_report)

int machine_commit_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineCommitError *error) {
    MachineCommitFile commit_file;
    int ok;

    machine_commit_file_init(&commit_file);
    ok = machine_commit_build_from_machine_ir_report_with_profile(report, profile, &commit_file, error) &&
        machine_commit_dump_file(&commit_file, out_text, error);
    machine_commit_file_free(&commit_file);
    return ok;
}

int machine_commit_report_get_summary(const MachineCommitReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_commit_file_get_source_elf_artifact_summary(const MachineCommitFile *commit_file,
    MachineElfArtifactSummary *out_summary) {
    if (!commit_file || !out_summary) {
        return 0;
    }
    *out_summary =
        commit_file->writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file
            .decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file
            .source_elf_artifact_summary;
    return 1;
}

int machine_commit_report_get_overview_artifact(const MachineCommitReport *report,
    MachineCommitReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->writeback_report = &report->writeback_report;
    out_artifact->commit_summary = &report->commit_summary;
    out_artifact->current_fetch_summary = report->file.has_current_fetch ? &report->current_fetch_summary : NULL;
    return 1;
}

int machine_commit_report_get_file(const MachineCommitReport *report,
    const MachineCommitFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_commit_report_get_writeback_file(const MachineCommitReport *report,
    const MachineWritebackFile **out_writeback_file) {
    if (!report || !out_writeback_file) {
        return 0;
    }
    *out_writeback_file = &report->file.writeback_file;
    return 1;
}

int machine_commit_report_get_writeback_report(const MachineCommitReport *report,
    const MachineWritebackReport **out_writeback_report) {
    if (!report || !out_writeback_report) {
        return 0;
    }
    *out_writeback_report = &report->writeback_report;
    return 1;
}

int machine_commit_report_get_source_elf_artifact_summary_artifact(
    const MachineCommitReport *report,
    const MachineElfArtifactSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->file.writeback_file.mutation_file.state_file.transition_file.interp_file
                         .payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file
                         .exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

#define MACHINE_COMMIT_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineCommitReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_COMMIT_DEFINE_REPORT_ARTIFACT_GETTER(machine_commit_report_get_header_summary_artifact,
    header_summary, MachineCommitHeaderSummary)
MACHINE_COMMIT_DEFINE_REPORT_ARTIFACT_GETTER(machine_commit_report_get_target_policy_summary_artifact,
    target_policy_summary, MachineCommitTargetPolicySummary)
MACHINE_COMMIT_DEFINE_REPORT_ARTIFACT_GETTER(machine_commit_report_get_commit_summary_artifact,
    commit_summary, MachineCommitSummary)

int machine_commit_report_get_current_fetch_summary_artifact(const MachineCommitReport *report,
    const MachineCommitCurrentFetchSummary **out_summary) {
    if (!report || !out_summary || !report->file.has_current_fetch) {
        return 0;
    }
    *out_summary = &report->current_fetch_summary;
    return 1;
}

int machine_commit_report_overview_artifact_get_writeback_report(
    const MachineCommitReportOverviewArtifact *artifact,
    const MachineWritebackReport **out_writeback_report) {
    if (!artifact || !out_writeback_report) {
        return 0;
    }
    *out_writeback_report = artifact->writeback_report;
    return 1;
}

int machine_commit_report_overview_artifact_get_commit_summary_artifact(
    const MachineCommitReportOverviewArtifact *artifact,
    const MachineCommitSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->commit_summary;
    return 1;
}

int machine_commit_report_overview_artifact_get_current_fetch_summary_artifact(
    const MachineCommitReportOverviewArtifact *artifact,
    const MachineCommitCurrentFetchSummary **out_summary) {
    if (!artifact || !out_summary || !artifact->current_fetch_summary) {
        return 0;
    }
    *out_summary = artifact->current_fetch_summary;
    return 1;
}

int machine_commit_dump_report(const MachineCommitReport *report,
    char **out_text,
    MachineCommitError *error) {
    MachineCommitStringBuilder builder;
    const MachineCommitSummary *commit_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_commit_report_get_commit_summary_artifact(report, &commit_summary) ||
        !commit_summary) {
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-133: invalid report dump contract");
        return 0;
    }

    if (!machine_commit_append_format(
            &builder,
            "machine_commit profile=%s elf_origin=%s elf_semantics=%s writeback=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_writeback_resolution_kind_name(report->header_summary.writeback_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count) ||
        !machine_commit_append_format(
            &builder,
            "commit: resolution=%s kind=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu has-committed-state=%s status=%s pc=%s",
            machine_commit_resolution_kind_name(commit_summary->resolution_kind),
            machine_commit_kind_name(commit_summary->commit_kind),
            machine_writeback_resolution_kind_name(commit_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(commit_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(commit_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(commit_summary->transition_resolution_kind),
            machine_interp_action_kind_name(commit_summary->action_kind),
            (unsigned int)commit_summary->raw_byte,
            (unsigned int)commit_summary->tag_value,
            commit_summary->is_known ? "yes" : "no",
            commit_summary->tag_name ? commit_summary->tag_name : "-",
            commit_summary->instruction_byte_count,
            commit_summary->has_committed_state ? "yes" : "no",
            commit_summary->has_committed_state ? machine_step_status_name(commit_summary->committed_status) : "-",
            commit_summary->has_committed_state ? "" : "-")) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-134: out of memory building report dump");
        return 0;
    }
    if (commit_summary->has_committed_state &&
        !machine_commit_append_format(&builder, "0x%zx", commit_summary->program_counter)) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-135: out of memory appending report pc");
        return 0;
    }
    if (!machine_commit_append_format(
            &builder,
            " current-segment=%s",
            commit_summary->has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-136: out of memory appending report current segment");
        return 0;
    }
    if (commit_summary->has_current_fetch &&
        !machine_commit_append_format(&builder, "%zu", commit_summary->current_segment_index)) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-137: out of memory appending report current segment value");
        return 0;
    }
    if (!machine_commit_append_format(
            &builder,
            " current-byte=%s",
            commit_summary->has_current_fetch ? "" : "-")) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-138: out of memory appending report current byte");
        return 0;
    }
    if (commit_summary->has_current_fetch &&
        !machine_commit_append_format(&builder, "0x%02x", (unsigned int)commit_summary->current_byte)) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-139: out of memory appending report current byte value");
        return 0;
    }
    if (!machine_commit_append_targets(&builder, commit_summary) ||
        !machine_commit_append_return_immediate(&builder, commit_summary) ||
        !machine_commit_append_format(&builder, "\nreport_overview:\n") ||
        !machine_commit_append_format(
            &builder,
            "  origin: writeback=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx\n",
            machine_writeback_resolution_kind_name(report->header_summary.writeback_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer) ||
        !machine_commit_append_format(
            &builder,
            "  elf_source: target=%s origin=%s semantics=%s\n",
            machine_elf_target_profile_name(
                report->file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.writeback_file.mutation_file.state_file.transition_file.interp_file.payload_decode_file.decode_file.step_file.launch_file.runtime_file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics)) ||
        !machine_commit_append_format(
            &builder,
            "  policy: profile=%s state=%s register=%s slot=%s call=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.commits_no_op_state ? "yes" : "no",
            report->target_policy_summary.defers_register_commit ? "yes" : "no",
            report->target_policy_summary.defers_slot_commit ? "yes" : "no",
            report->target_policy_summary.defers_call_commit ? "yes" : "no") ||
        !machine_commit_append_format(
            &builder,
            "  commit: resolution=%s kind=%s has-state=%s status=%s pc=%s",
            machine_commit_resolution_kind_name(commit_summary->resolution_kind),
            machine_commit_kind_name(commit_summary->commit_kind),
            commit_summary->has_committed_state ? "yes" : "no",
            commit_summary->has_committed_state ? machine_step_status_name(commit_summary->committed_status) : "-",
            commit_summary->has_committed_state ? "" : "-")) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-140: out of memory building report overview");
        return 0;
    }
    if (commit_summary->has_committed_state &&
        !machine_commit_append_format(&builder, "0x%zx", commit_summary->program_counter)) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-141: out of memory appending report overview pc");
        return 0;
    }
    if (!machine_commit_append_targets(&builder, commit_summary) ||
        !machine_commit_append_return_immediate(&builder, commit_summary) ||
        !machine_commit_append_format(&builder, "\n")) {
        free(builder.data);
        machine_commit_set_error(error, 0, 0, "MACHINE-COMMIT-142: out of memory terminating report overview");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_commit_build_report_dump_from_file(const MachineCommitFile *source,
    char **out_text,
    MachineCommitError *error) {
    MachineCommitReport report;
    int ok;

    machine_commit_report_init(&report);
    ok = machine_commit_build_report_from_file(source, &report, error) &&
        machine_commit_dump_report(&report, out_text, error);
    machine_commit_report_free(&report);
    return ok;
}

#define MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineCommitError *error) { \
    MachineCommitReport report; \
    int ok; \
    machine_commit_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_commit_dump_report(&report, out_text, error); \
    machine_commit_report_free(&report); \
    return ok; \
}

MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_writeback_file,
    MachineWritebackFile, machine_commit_build_report_from_machine_writeback_file)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_writeback_report,
    MachineWritebackReport, machine_commit_build_report_from_machine_writeback_report)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_mutation_file,
    MachineMutationFile, machine_commit_build_report_from_machine_mutation_file)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_mutation_report,
    MachineMutationReport, machine_commit_build_report_from_machine_mutation_report)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_state_file,
    MachineStateFile, machine_commit_build_report_from_machine_state_file)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_state_report,
    MachineStateReport, machine_commit_build_report_from_machine_state_report)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_transition_file,
    MachineTransitionFile, machine_commit_build_report_from_machine_transition_file)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_transition_report,
    MachineTransitionReport, machine_commit_build_report_from_machine_transition_report)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_interp_file,
    MachineInterpFile, machine_commit_build_report_from_machine_interp_file)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_interp_report,
    MachineInterpReport, machine_commit_build_report_from_machine_interp_report)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_payload_decode_file,
    MachinePayloadDecodeFile, machine_commit_build_report_from_machine_payload_decode_file)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_payload_decode_report,
    MachinePayloadDecodeReport, machine_commit_build_report_from_machine_payload_decode_report)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_decode_file,
    MachineDecodeFile, machine_commit_build_report_from_machine_decode_file)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_decode_report,
    MachineDecodeReport, machine_commit_build_report_from_machine_decode_report)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_commit_build_report_from_machine_step_file)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_commit_build_report_from_machine_step_report)
MACHINE_COMMIT_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_commit_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_commit_build_report_from_machine_ir_report)

int machine_commit_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineCommitError *error) {
    MachineCommitReport report_file;
    int ok;

    machine_commit_report_init(&report_file);
    ok = machine_commit_build_report_from_machine_ir_report_with_profile(report, profile, &report_file, error) &&
        machine_commit_dump_report(&report_file, out_text, error);
    machine_commit_report_free(&report_file);
    return ok;
}
