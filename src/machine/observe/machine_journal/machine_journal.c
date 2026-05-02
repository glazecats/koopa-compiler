#include "machine/journal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineJournalStringBuilder;

static void machine_journal_set_error(
    MachineJournalError *error,
    int line,
    int column,
    const char *fmt,
    ...);
static void machine_journal_set_error_from_log_error(
    MachineJournalError *error,
    const MachineLogError *log_error,
    const char *fallback_message);
static int machine_journal_append_format(
    MachineJournalStringBuilder *builder,
    const char *fmt,
    ...);
static int machine_journal_append_targets(
    MachineJournalStringBuilder *builder,
    const MachineJournalSummary *journal_summary);
static int machine_journal_append_return_immediate(
    MachineJournalStringBuilder *builder,
    const MachineJournalSummary *journal_summary);
static int machine_journal_append_payload_bytes(
    MachineJournalStringBuilder *builder,
    const MachineJournalSummary *journal_summary);
static int machine_journal_append_immediate_hint(
    MachineJournalStringBuilder *builder,
    const MachineJournalSummary *journal_summary);
static MachineJournalResolutionKind machine_journal_resolution_from_log(
    MachineLogResolutionKind resolution_kind);
static MachineJournalKind machine_journal_kind_from_log(MachineLogKind log_kind);

static void machine_journal_set_error(
    MachineJournalError *error,
    int line,
    int column,
    const char *fmt,
    ...) {
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

static void machine_journal_set_error_from_log_error(
    MachineJournalError *error,
    const MachineLogError *log_error,
    const char *fallback_message) {
    if (!error) {
        return;
    }
    if (log_error && log_error->message[0] != '\0') {
        machine_journal_set_error(
            error,
            log_error->line,
            log_error->column,
            "%s",
            log_error->message);
        return;
    }
    machine_journal_set_error(
        error,
        0,
        0,
        "%s",
        fallback_message ? fallback_message : "machine-journal failure");
}

static int machine_journal_append_format(
    MachineJournalStringBuilder *builder,
    const char *fmt,
    ...) {
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

static int machine_journal_append_targets(
    MachineJournalStringBuilder *builder,
    const MachineJournalSummary *journal_summary) {
    const MachineLogSummary *log_summary = NULL;

    if (!builder || !journal_summary) {
        return 0;
    }
    log_summary = &journal_summary->log_summary;
    if (!log_summary->has_primary_target_block) {
        return machine_journal_append_format(builder, " targets=[]");
    }
    if (!log_summary->has_secondary_target_block) {
        return machine_journal_append_format(
            builder, " targets=[%zu]", log_summary->primary_target_block_index);
    }
    return machine_journal_append_format(
        builder,
        " targets=[%zu,%zu]",
        log_summary->primary_target_block_index,
        log_summary->secondary_target_block_index);
}

static int machine_journal_append_return_immediate(
    MachineJournalStringBuilder *builder,
    const MachineJournalSummary *journal_summary) {
    const MachineLogSummary *log_summary = NULL;

    if (!builder || !journal_summary) {
        return 0;
    }
    log_summary = &journal_summary->log_summary;
    if (!log_summary->has_return_immediate) {
        return machine_journal_append_format(builder, " return-imm=-");
    }
    return machine_journal_append_format(builder, " return-imm=%zu", log_summary->return_immediate);
}

static int machine_journal_append_payload_bytes(
    MachineJournalStringBuilder *builder,
    const MachineJournalSummary *journal_summary) {
    const MachineLogSummary *log_summary = NULL;
    size_t index;

    if (!builder || !journal_summary) {
        return 0;
    }
    log_summary = &journal_summary->log_summary;
    if (!machine_journal_append_format(builder, " payload=[")) {
        return 0;
    }
    for (index = 0u; index < log_summary->payload_byte_count; ++index) {
        if (!machine_journal_append_format(
                builder,
                "%s0x%02x",
                index == 0u ? "" : ",",
                (unsigned int)log_summary->payload_bytes[index])) {
            return 0;
        }
    }
    return machine_journal_append_format(builder, "]");
}

static int machine_journal_append_immediate_hint(
    MachineJournalStringBuilder *builder,
    const MachineJournalSummary *journal_summary) {
    const MachineLogSummary *log_summary = NULL;

    if (!builder || !journal_summary) {
        return 0;
    }
    log_summary = &journal_summary->log_summary;
    if (!log_summary->has_immediate_hint) {
        return machine_journal_append_format(builder, " imm=-");
    }
    return machine_journal_append_format(builder, " imm=%zu", log_summary->immediate_hint);
}

static MachineJournalResolutionKind machine_journal_resolution_from_log(
    MachineLogResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_LOG_RESOLUTION_EXACT_LOG:
            return MACHINE_JOURNAL_RESOLUTION_EXACT_JOURNAL;
        case MACHINE_LOG_RESOLUTION_PREVIEW_LOG:
            return MACHINE_JOURNAL_RESOLUTION_PREVIEW_JOURNAL;
        case MACHINE_LOG_RESOLUTION_BLOCKED_ON_CONTROL:
            return MACHINE_JOURNAL_RESOLUTION_BLOCKED_ON_CONTROL;
        case MACHINE_LOG_RESOLUTION_BLOCKED_UNSUPPORTED:
            return MACHINE_JOURNAL_RESOLUTION_BLOCKED_UNSUPPORTED;
        case MACHINE_LOG_RESOLUTION_NONE:
        default:
            return MACHINE_JOURNAL_RESOLUTION_NONE;
    }
}

static MachineJournalKind machine_journal_kind_from_log(MachineLogKind log_kind) {
    switch (log_kind) {
        case MACHINE_LOG_KIND_PROGRAM_STOP_LOG:
            return MACHINE_JOURNAL_KIND_PROGRAM_STOP_JOURNAL;
        case MACHINE_LOG_KIND_VALUE_LOG:
            return MACHINE_JOURNAL_KIND_VALUE_JOURNAL;
        case MACHINE_LOG_KIND_LOCAL_UPDATE_LOG:
            return MACHINE_JOURNAL_KIND_LOCAL_UPDATE_JOURNAL;
        case MACHINE_LOG_KIND_GLOBAL_UPDATE_LOG:
            return MACHINE_JOURNAL_KIND_GLOBAL_UPDATE_JOURNAL;
        case MACHINE_LOG_KIND_CALL_LOG:
            return MACHINE_JOURNAL_KIND_CALL_JOURNAL;
        case MACHINE_LOG_KIND_BLOCKED_CONTROL_LOG:
            return MACHINE_JOURNAL_KIND_BLOCKED_CONTROL_JOURNAL;
        case MACHINE_LOG_KIND_BLOCKED_UNSUPPORTED_LOG:
            return MACHINE_JOURNAL_KIND_BLOCKED_UNSUPPORTED_JOURNAL;
        case MACHINE_LOG_KIND_NONE:
        default:
            return MACHINE_JOURNAL_KIND_NONE;
    }
}

void machine_journal_file_init(MachineJournalFile *journal_file) {
    if (!journal_file) {
        return;
    }
    machine_log_file_init(&journal_file->log_file);
    journal_file->resolution_kind = MACHINE_JOURNAL_RESOLUTION_NONE;
    journal_file->journal_kind = MACHINE_JOURNAL_KIND_NONE;
    journal_file->journal_record_count = 0u;
    journal_file->journal_record_index = 0u;
}

void machine_journal_file_free(MachineJournalFile *journal_file) {
    if (!journal_file) {
        return;
    }
    machine_log_file_free(&journal_file->log_file);
    machine_journal_file_init(journal_file);
}

void machine_journal_report_init(MachineJournalReport *report) {
    if (!report) {
        return;
    }
    machine_journal_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    machine_log_report_init(&report->log_report);
    memset(&report->journal_summary, 0, sizeof(report->journal_summary));
}

void machine_journal_report_free(MachineJournalReport *report) {
    if (!report) {
        return;
    }
    machine_log_report_free(&report->log_report);
    machine_journal_file_free(&report->file);
    machine_journal_report_init(report);
}

const char *machine_journal_resolution_kind_name(
    MachineJournalResolutionKind resolution_kind) {
    switch (resolution_kind) {
        case MACHINE_JOURNAL_RESOLUTION_NONE:
            return "none";
        case MACHINE_JOURNAL_RESOLUTION_EXACT_JOURNAL:
            return "exact-journal";
        case MACHINE_JOURNAL_RESOLUTION_PREVIEW_JOURNAL:
            return "preview-journal";
        case MACHINE_JOURNAL_RESOLUTION_BLOCKED_ON_CONTROL:
            return "blocked-on-control";
        case MACHINE_JOURNAL_RESOLUTION_BLOCKED_UNSUPPORTED:
            return "blocked-unsupported";
    }
    return "unknown";
}

const char *machine_journal_kind_name(MachineJournalKind journal_kind) {
    switch (journal_kind) {
        case MACHINE_JOURNAL_KIND_NONE:
            return "none";
        case MACHINE_JOURNAL_KIND_PROGRAM_STOP_JOURNAL:
            return "program-stop-journal";
        case MACHINE_JOURNAL_KIND_VALUE_JOURNAL:
            return "value-journal";
        case MACHINE_JOURNAL_KIND_LOCAL_UPDATE_JOURNAL:
            return "local-update-journal";
        case MACHINE_JOURNAL_KIND_GLOBAL_UPDATE_JOURNAL:
            return "global-update-journal";
        case MACHINE_JOURNAL_KIND_CALL_JOURNAL:
            return "call-journal";
        case MACHINE_JOURNAL_KIND_BLOCKED_CONTROL_JOURNAL:
            return "blocked-control-journal";
        case MACHINE_JOURNAL_KIND_BLOCKED_UNSUPPORTED_JOURNAL:
            return "blocked-unsupported-journal";
    }
    return "unknown";
}

int machine_journal_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineJournalTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->surfaces_exact_journal = 1;
    out_summary->surfaces_preview_journal = 1;
    out_summary->surfaces_single_record_journal = 1;
    return 1;
}

int machine_journal_file_get_target_policy_summary(
    const MachineJournalFile *journal_file,
    MachineJournalTargetPolicySummary *out_summary) {
    MachineLogTargetPolicySummary log_summary;

    memset(&log_summary, 0, sizeof(log_summary));
    if (!journal_file || !out_summary ||
        !machine_log_file_get_target_policy_summary(&journal_file->log_file, &log_summary)) {
        return 0;
    }
    out_summary->target_profile = log_summary.target_profile;
    out_summary->surfaces_exact_journal = log_summary.surfaces_exact_log;
    out_summary->surfaces_preview_journal = log_summary.surfaces_preview_log;
    out_summary->surfaces_single_record_journal = log_summary.surfaces_single_line_log;
    return 1;
}

int machine_journal_file_get_summary(const MachineJournalFile *journal_file,
    size_t *out_mapped_byte_count) {
    if (!journal_file) {
        return 0;
    }
    return machine_log_file_get_summary(&journal_file->log_file, out_mapped_byte_count);
}

int machine_journal_file_get_header_summary(const MachineJournalFile *journal_file,
    MachineJournalHeaderSummary *out_summary) {
    MachineLogHeaderSummary log_header_summary;
    MachineLogSummary log_summary;

    memset(&log_header_summary, 0, sizeof(log_header_summary));
    memset(&log_summary, 0, sizeof(log_summary));
    if (!journal_file || !out_summary ||
        !machine_log_file_get_header_summary(&journal_file->log_file, &log_header_summary) ||
        !machine_log_file_get_log_summary(&journal_file->log_file, &log_summary)) {
        return 0;
    }

    out_summary->target_profile = log_header_summary.target_profile;
    out_summary->log_resolution_kind = log_summary.resolution_kind;
    out_summary->timeline_resolution_kind = log_header_summary.timeline_resolution_kind;
    out_summary->origin_step_status = log_header_summary.origin_step_status;
    out_summary->origin_program_counter = log_header_summary.origin_program_counter;
    out_summary->origin_stack_pointer = log_header_summary.origin_stack_pointer;
    out_summary->origin_current_segment_index = log_header_summary.origin_current_segment_index;
    out_summary->mapped_byte_count = log_header_summary.mapped_byte_count;
    out_summary->journal_record_count = journal_file->journal_record_count;
    out_summary->journal_record_index = journal_file->journal_record_index;
    return 1;
}

int machine_journal_file_get_log_summary(const MachineJournalFile *journal_file,
    MachineLogSummary *out_summary) {
    if (!journal_file || !out_summary) {
        return 0;
    }
    return machine_log_file_get_log_summary(&journal_file->log_file, out_summary);
}

int machine_journal_file_get_journal_summary(const MachineJournalFile *journal_file,
    MachineJournalSummary *out_summary) {
    MachineLogSummary log_summary;

    memset(&log_summary, 0, sizeof(log_summary));
    if (!journal_file || !out_summary ||
        !machine_log_file_get_log_summary(&journal_file->log_file, &log_summary)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->resolution_kind = journal_file->resolution_kind;
    out_summary->journal_kind = journal_file->journal_kind;
    out_summary->log_resolution_kind = log_summary.resolution_kind;
    out_summary->log_kind = log_summary.log_kind;
    out_summary->is_exact_journal =
        journal_file->resolution_kind == MACHINE_JOURNAL_RESOLUTION_EXACT_JOURNAL;
    out_summary->is_single_record_journal = journal_file->journal_record_count == 1u;
    out_summary->journal_record_count = journal_file->journal_record_count;
    out_summary->journal_record_index = journal_file->journal_record_index;
    out_summary->log_summary = log_summary;
    return 1;
}

int machine_journal_verify_file(const MachineJournalFile *journal_file,
    MachineJournalError *error) {
    MachineLogError log_error;
    MachineLogSummary log_summary;
    MachineJournalResolutionKind expected_resolution;
    MachineJournalKind expected_kind;

    memset(&log_error, 0, sizeof(log_error));
    memset(&log_summary, 0, sizeof(log_summary));
    if (!journal_file) {
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-100: journal file is null");
        return 0;
    }
    if (!machine_log_verify_file(&journal_file->log_file, &log_error)) {
        machine_journal_set_error_from_log_error(
            error, &log_error, "MACHINE-JOURNAL-101: invalid machine log");
        return 0;
    }
    if (!machine_log_file_get_log_summary(&journal_file->log_file, &log_summary)) {
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-102: missing machine-log summary");
        return 0;
    }
    if (journal_file->journal_record_count != 1u || journal_file->journal_record_index != 0u) {
        machine_journal_set_error(
            error,
            0,
            0,
            "MACHINE-JOURNAL-103: first journal slice must stay single-record");
        return 0;
    }
    expected_resolution = machine_journal_resolution_from_log(log_summary.resolution_kind);
    expected_kind = machine_journal_kind_from_log(log_summary.log_kind);
    if (journal_file->resolution_kind != expected_resolution) {
        machine_journal_set_error(
            error,
            0,
            0,
            "MACHINE-JOURNAL-104: journal resolution must match machine-log resolution");
        return 0;
    }
    if (journal_file->journal_kind != expected_kind) {
        machine_journal_set_error(
            error,
            0,
            0,
            "MACHINE-JOURNAL-105: journal kind must match machine-log kind");
        return 0;
    }
    return 1;
}

int machine_journal_clone_file(const MachineJournalFile *source,
    MachineJournalFile *out_clone,
    MachineJournalError *error) {
    MachineLogError log_error;

    memset(&log_error, 0, sizeof(log_error));
    if (!source || !out_clone) {
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-106: invalid clone contract");
        return 0;
    }
    if (!machine_journal_verify_file(source, error)) {
        return 0;
    }
    machine_journal_file_init(out_clone);
    if (!machine_log_clone_file(&source->log_file, &out_clone->log_file, &log_error)) {
        machine_journal_set_error_from_log_error(
            error, &log_error, "MACHINE-JOURNAL-107: failed to clone machine-log file");
        return 0;
    }
    out_clone->resolution_kind = source->resolution_kind;
    out_clone->journal_kind = source->journal_kind;
    out_clone->journal_record_count = source->journal_record_count;
    out_clone->journal_record_index = source->journal_record_index;
    return 1;
}

int machine_journal_build_from_machine_log_file(const MachineLogFile *log_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error) {
    MachineLogError log_error;
    MachineLogSummary log_summary;

    memset(&log_error, 0, sizeof(log_error));
    memset(&log_summary, 0, sizeof(log_summary));
    if (!log_file || !out_journal_file) {
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-108: invalid build contract");
        return 0;
    }
    if (!machine_log_verify_file(log_file, &log_error)) {
        machine_journal_set_error_from_log_error(
            error, &log_error, "MACHINE-JOURNAL-109: invalid machine-log source");
        return 0;
    }
    if (!machine_log_file_get_log_summary(log_file, &log_summary)) {
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-110: missing machine-log summary");
        return 0;
    }

    machine_journal_file_free(out_journal_file);
    if (!machine_log_clone_file(log_file, &out_journal_file->log_file, &log_error)) {
        machine_journal_set_error_from_log_error(
            error, &log_error, "MACHINE-JOURNAL-111: failed to clone machine-log source");
        return 0;
    }
    out_journal_file->resolution_kind = machine_journal_resolution_from_log(log_summary.resolution_kind);
    out_journal_file->journal_kind = machine_journal_kind_from_log(log_summary.log_kind);
    out_journal_file->journal_record_count = 1u;
    out_journal_file->journal_record_index = 0u;

    if (!machine_journal_verify_file(out_journal_file, error)) {
        machine_journal_file_free(out_journal_file);
        return 0;
    }
    return 1;
}

int machine_journal_build_from_machine_log_report(const MachineLogReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error) {
    const MachineLogFile *log_file = NULL;

    if (!report || !out_journal_file) {
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-112: invalid report build contract");
        return 0;
    }
    if (!machine_log_report_get_file(report, &log_file) || !log_file) {
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-113: report does not expose machine-log file");
        return 0;
    }
    return machine_journal_build_from_machine_log_file(log_file, out_journal_file, error);
}

#define MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineJournalFile *out_journal_file, MachineJournalError *error) { \
    MachineLogFile log_file; \
    MachineLogError log_error; \
    int ok; \
    machine_log_file_init(&log_file); \
    memset(&log_error, 0, sizeof(log_error)); \
    ok = builder(source, &log_file, &log_error) && \
        machine_journal_build_from_machine_log_file(&log_file, out_journal_file, error); \
    if (!ok && error && error->message[0] == '\0') { \
        machine_journal_set_error_from_log_error(error, &log_error, "MACHINE-JOURNAL wrapper failure"); \
    } \
    machine_log_file_free(&log_file); \
    return ok; \
}

MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_timeline_file,
    MachineTimelineFile, machine_log_build_from_machine_timeline_file)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_timeline_report,
    MachineTimelineReport, machine_log_build_from_machine_timeline_report)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_history_file,
    MachineHistoryFile, machine_log_build_from_machine_history_file)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_history_report,
    MachineHistoryReport, machine_log_build_from_machine_history_report)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_outcome_file,
    MachineOutcomeFile, machine_log_build_from_machine_outcome_file)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_outcome_report,
    MachineOutcomeReport, machine_log_build_from_machine_outcome_report)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_event_file,
    MachineEventFile, machine_log_build_from_machine_event_file)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_event_report,
    MachineEventReport, machine_log_build_from_machine_event_report)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_trace_file,
    MachineTraceFile, machine_log_build_from_machine_trace_file)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_trace_report,
    MachineTraceReport, machine_log_build_from_machine_trace_report)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_delta_file,
    MachineDeltaFile, machine_log_build_from_machine_delta_file)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_delta_report,
    MachineDeltaReport, machine_log_build_from_machine_delta_report)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_observe_file,
    MachineObserveFile, machine_log_build_from_machine_observe_file)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_observe_report,
    MachineObserveReport, machine_log_build_from_machine_observe_report)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_apply_file,
    MachineApplyFile, machine_log_build_from_machine_apply_file)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_apply_report,
    MachineApplyReport, machine_log_build_from_machine_apply_report)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_commit_file,
    MachineCommitFile, machine_log_build_from_machine_commit_file)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_commit_report,
    MachineCommitReport, machine_log_build_from_machine_commit_report)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_step_file,
    MachineStepFile, machine_log_build_from_machine_step_file)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_step_report,
    MachineStepReport, machine_log_build_from_machine_step_report)
MACHINE_JOURNAL_DEFINE_BUILD_FROM_WRAPPER(machine_journal_build_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_log_build_from_machine_ir_report)

int machine_journal_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error) {
    MachineLogFile log_file;
    MachineLogError log_error;
    int ok;

    machine_log_file_init(&log_file);
    memset(&log_error, 0, sizeof(log_error));
    ok = machine_log_build_from_machine_ir_report_with_profile(
            report, profile, &log_file, &log_error) &&
        machine_journal_build_from_machine_log_file(&log_file, out_journal_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_journal_set_error_from_log_error(
            error, &log_error, "MACHINE-JOURNAL-114: profile bridge failure");
    }
    machine_log_file_free(&log_file);
    return ok;
}

int machine_journal_build_report_from_file(const MachineJournalFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error) {
    MachineLogError log_error;

    memset(&log_error, 0, sizeof(log_error));
    if (!source || !out_report) {
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-115: invalid report contract");
        return 0;
    }
    if (!machine_journal_verify_file(source, error)) {
        return 0;
    }

    machine_journal_report_free(out_report);
    if (!machine_journal_clone_file(source, &out_report->file, error) ||
        !machine_journal_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_journal_file_get_target_policy_summary(
            &out_report->file, &out_report->target_policy_summary) ||
        !machine_journal_file_get_journal_summary(&out_report->file, &out_report->journal_summary) ||
        !machine_log_build_report_from_file(
            &out_report->file.log_file, &out_report->log_report, &log_error)) {
        if (error && error->message[0] == '\0') {
            machine_journal_set_error_from_log_error(
                error, &log_error, "MACHINE-JOURNAL-116: failed to build report");
        }
        machine_journal_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, MachineJournalReport *out_report, MachineJournalError *error) { \
    MachineJournalFile journal_file; \
    int ok; \
    machine_journal_file_init(&journal_file); \
    ok = builder(source, &journal_file, error) && \
        machine_journal_build_report_from_file(&journal_file, out_report, error); \
    machine_journal_file_free(&journal_file); \
    return ok; \
}

MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_log_file,
    MachineLogFile, machine_journal_build_from_machine_log_file)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_log_report,
    MachineLogReport, machine_journal_build_from_machine_log_report)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_timeline_file,
    MachineTimelineFile, machine_journal_build_from_machine_timeline_file)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_timeline_report,
    MachineTimelineReport, machine_journal_build_from_machine_timeline_report)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_history_file,
    MachineHistoryFile, machine_journal_build_from_machine_history_file)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_history_report,
    MachineHistoryReport, machine_journal_build_from_machine_history_report)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_outcome_file,
    MachineOutcomeFile, machine_journal_build_from_machine_outcome_file)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_outcome_report,
    MachineOutcomeReport, machine_journal_build_from_machine_outcome_report)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_event_file,
    MachineEventFile, machine_journal_build_from_machine_event_file)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_event_report,
    MachineEventReport, machine_journal_build_from_machine_event_report)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_trace_file,
    MachineTraceFile, machine_journal_build_from_machine_trace_file)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_trace_report,
    MachineTraceReport, machine_journal_build_from_machine_trace_report)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_delta_file,
    MachineDeltaFile, machine_journal_build_from_machine_delta_file)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_delta_report,
    MachineDeltaReport, machine_journal_build_from_machine_delta_report)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_observe_file,
    MachineObserveFile, machine_journal_build_from_machine_observe_file)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_observe_report,
    MachineObserveReport, machine_journal_build_from_machine_observe_report)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_apply_file,
    MachineApplyFile, machine_journal_build_from_machine_apply_file)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_apply_report,
    MachineApplyReport, machine_journal_build_from_machine_apply_report)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_commit_file,
    MachineCommitFile, machine_journal_build_from_machine_commit_file)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_commit_report,
    MachineCommitReport, machine_journal_build_from_machine_commit_report)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_step_file,
    MachineStepFile, machine_journal_build_from_machine_step_file)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_step_report,
    MachineStepReport, machine_journal_build_from_machine_step_report)
MACHINE_JOURNAL_DEFINE_REPORT_FROM_WRAPPER(machine_journal_build_report_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_journal_build_from_machine_ir_report)

int machine_journal_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineJournalReport *out_report,
    MachineJournalError *error) {
    MachineJournalFile journal_file;
    int ok;

    machine_journal_file_init(&journal_file);
    ok = machine_journal_build_from_machine_ir_report_with_profile(
            report, profile, &journal_file, error) &&
        machine_journal_build_report_from_file(&journal_file, out_report, error);
    machine_journal_file_free(&journal_file);
    return ok;
}

int machine_journal_report_refresh(MachineJournalReport *report,
    MachineJournalError *error) {
    MachineJournalReport refreshed_report;
    int ok;

    machine_journal_report_init(&refreshed_report);
    ok = machine_journal_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_journal_report_free(report);
        *report = refreshed_report;
    } else {
        machine_journal_report_free(&refreshed_report);
    }
    return ok;
}

int machine_journal_dump_file(const MachineJournalFile *journal_file,
    char **out_text,
    MachineJournalError *error) {
    MachineJournalStringBuilder builder;
    MachineJournalHeaderSummary header_summary;
    MachineJournalSummary journal_summary;
    const MachineLogSummary *log_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    memset(&journal_summary, 0, sizeof(journal_summary));
    if (!journal_file || !out_text ||
        !machine_journal_verify_file(journal_file, error) ||
        !machine_journal_file_get_header_summary(journal_file, &header_summary) ||
        !machine_journal_file_get_journal_summary(journal_file, &journal_summary)) {
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-117: invalid dump contract");
        return 0;
    }
    log_summary = &journal_summary.log_summary;

    if (!machine_journal_append_format(
            &builder,
            "machine_journal profile=%s log=%s timeline=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu records=%zu record-index=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_log_resolution_kind_name(header_summary.log_resolution_kind),
            machine_timeline_resolution_kind_name(header_summary.timeline_resolution_kind),
            machine_step_status_name(header_summary.origin_step_status),
            header_summary.origin_program_counter,
            header_summary.origin_stack_pointer,
            header_summary.origin_current_segment_index,
            header_summary.mapped_byte_count,
            header_summary.journal_record_count,
            header_summary.journal_record_index) ||
        !machine_journal_append_format(
            &builder,
            "journal: resolution=%s kind=%s log=%s timeline=%s history=%s outcome=%s event=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_journal_resolution_kind_name(journal_summary.resolution_kind),
            machine_journal_kind_name(journal_summary.journal_kind),
            machine_log_kind_name(journal_summary.log_kind),
            machine_timeline_kind_name(log_summary->timeline_kind),
            machine_history_kind_name(log_summary->history_kind),
            machine_outcome_kind_name(log_summary->outcome_kind),
            machine_event_kind_name(log_summary->event_kind),
            machine_trace_resolution_kind_name(log_summary->trace_resolution_kind),
            machine_trace_change_class_name(log_summary->trace_change_class),
            machine_apply_resolution_kind_name(log_summary->apply_resolution_kind),
            machine_commit_resolution_kind_name(log_summary->commit_resolution_kind),
            machine_writeback_resolution_kind_name(log_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(log_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(log_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(log_summary->transition_resolution_kind),
            machine_interp_action_kind_name(log_summary->action_kind),
            (unsigned int)log_summary->raw_byte,
            (unsigned int)log_summary->tag_value,
            log_summary->is_known ? "yes" : "no",
            log_summary->tag_name ? log_summary->tag_name : "-",
            log_summary->instruction_byte_count) ||
        !machine_journal_append_payload_bytes(&builder, &journal_summary) ||
        !machine_journal_append_immediate_hint(&builder, &journal_summary) ||
        !machine_journal_append_format(
            &builder,
            " exact=%s single-record=%s records=%zu record-index=%zu origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            journal_summary.is_exact_journal ? "yes" : "no",
            journal_summary.is_single_record_journal ? "yes" : "no",
            journal_summary.journal_record_count,
            journal_summary.journal_record_index,
            machine_step_status_name(log_summary->origin_status),
            log_summary->has_observed_state ? machine_step_status_name(log_summary->observed_status) : "-",
            log_summary->has_status_change ? (log_summary->status_changed ? "yes" : "no") : "-",
            log_summary->has_program_counter_change ? (log_summary->program_counter_changed ? "yes" : "no") : "-",
            log_summary->has_stack_pointer_change ? (log_summary->stack_pointer_changed ? "yes" : "no") : "-",
            log_summary->has_fetch_change ? (log_summary->fetch_changed ? "yes" : "no") : "-") ||
        !machine_journal_append_targets(&builder, &journal_summary) ||
        !machine_journal_append_return_immediate(&builder, &journal_summary) ||
        !machine_journal_append_format(&builder, "\n")) {
        free(builder.data);
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-118: out of memory building dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_journal_build_dump_from_file(const MachineJournalFile *source,
    char **out_text,
    MachineJournalError *error) {
    return machine_journal_dump_file(source, out_text, error);
}

#define MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineJournalError *error) { \
    MachineJournalFile journal_file; \
    int ok; \
    machine_journal_file_init(&journal_file); \
    ok = builder(source, &journal_file, error) && \
        machine_journal_dump_file(&journal_file, out_text, error); \
    machine_journal_file_free(&journal_file); \
    return ok; \
}

MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_log_file,
    MachineLogFile, machine_journal_build_from_machine_log_file)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_log_report,
    MachineLogReport, machine_journal_build_from_machine_log_report)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_timeline_file,
    MachineTimelineFile, machine_journal_build_from_machine_timeline_file)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_timeline_report,
    MachineTimelineReport, machine_journal_build_from_machine_timeline_report)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_history_file,
    MachineHistoryFile, machine_journal_build_from_machine_history_file)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_history_report,
    MachineHistoryReport, machine_journal_build_from_machine_history_report)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_outcome_file,
    MachineOutcomeFile, machine_journal_build_from_machine_outcome_file)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_outcome_report,
    MachineOutcomeReport, machine_journal_build_from_machine_outcome_report)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_event_file,
    MachineEventFile, machine_journal_build_from_machine_event_file)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_event_report,
    MachineEventReport, machine_journal_build_from_machine_event_report)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_trace_file,
    MachineTraceFile, machine_journal_build_from_machine_trace_file)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_trace_report,
    MachineTraceReport, machine_journal_build_from_machine_trace_report)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_delta_file,
    MachineDeltaFile, machine_journal_build_from_machine_delta_file)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_delta_report,
    MachineDeltaReport, machine_journal_build_from_machine_delta_report)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_observe_file,
    MachineObserveFile, machine_journal_build_from_machine_observe_file)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_observe_report,
    MachineObserveReport, machine_journal_build_from_machine_observe_report)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_apply_file,
    MachineApplyFile, machine_journal_build_from_machine_apply_file)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_apply_report,
    MachineApplyReport, machine_journal_build_from_machine_apply_report)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_commit_file,
    MachineCommitFile, machine_journal_build_from_machine_commit_file)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_commit_report,
    MachineCommitReport, machine_journal_build_from_machine_commit_report)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_step_file,
    MachineStepFile, machine_journal_build_from_machine_step_file)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_step_report,
    MachineStepReport, machine_journal_build_from_machine_step_report)
MACHINE_JOURNAL_DEFINE_DUMP_FROM_WRAPPER(machine_journal_build_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_journal_build_from_machine_ir_report)

int machine_journal_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineJournalError *error) {
    MachineJournalFile journal_file;
    int ok;

    machine_journal_file_init(&journal_file);
    ok = machine_journal_build_from_machine_ir_report_with_profile(
            report, profile, &journal_file, error) &&
        machine_journal_dump_file(&journal_file, out_text, error);
    machine_journal_file_free(&journal_file);
    return ok;
}

int machine_journal_report_get_summary(const MachineJournalReport *report,
    size_t *out_mapped_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    return 1;
}

int machine_journal_report_get_overview_artifact(const MachineJournalReport *report,
    MachineJournalReportOverviewArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->log_report = &report->log_report;
    out_artifact->journal_summary = &report->journal_summary;
    return 1;
}

int machine_journal_report_get_file(const MachineJournalReport *report,
    const MachineJournalFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_journal_report_get_log_file(const MachineJournalReport *report,
    const MachineLogFile **out_log_file) {
    if (!report || !out_log_file) {
        return 0;
    }
    *out_log_file = &report->file.log_file;
    return 1;
}

int machine_journal_report_get_log_report(const MachineJournalReport *report,
    const MachineLogReport **out_log_report) {
    if (!report || !out_log_report) {
        return 0;
    }
    *out_log_report = &report->log_report;
    return 1;
}

#define MACHINE_JOURNAL_DEFINE_REPORT_ARTIFACT_GETTER(name, field, type) \
int name(const MachineJournalReport *report, const type **out_summary) { \
    if (!report || !out_summary) { \
        return 0; \
    } \
    *out_summary = &report->field; \
    return 1; \
}

MACHINE_JOURNAL_DEFINE_REPORT_ARTIFACT_GETTER(machine_journal_report_get_header_summary_artifact,
    header_summary, MachineJournalHeaderSummary)
MACHINE_JOURNAL_DEFINE_REPORT_ARTIFACT_GETTER(
    machine_journal_report_get_target_policy_summary_artifact,
    target_policy_summary,
    MachineJournalTargetPolicySummary)
MACHINE_JOURNAL_DEFINE_REPORT_ARTIFACT_GETTER(machine_journal_report_get_journal_summary_artifact,
    journal_summary, MachineJournalSummary)

int machine_journal_report_overview_artifact_get_log_report(
    const MachineJournalReportOverviewArtifact *artifact,
    const MachineLogReport **out_log_report) {
    if (!artifact || !out_log_report) {
        return 0;
    }
    *out_log_report = artifact->log_report;
    return 1;
}

int machine_journal_report_overview_artifact_get_journal_summary_artifact(
    const MachineJournalReportOverviewArtifact *artifact,
    const MachineJournalSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->journal_summary;
    return 1;
}

int machine_journal_dump_report(const MachineJournalReport *report,
    char **out_text,
    MachineJournalError *error) {
    MachineJournalStringBuilder builder;
    const MachineJournalSummary *journal_summary = NULL;
    const MachineLogSummary *log_summary = NULL;

    memset(&builder, 0, sizeof(builder));
    if (!report || !out_text ||
        !machine_journal_report_get_journal_summary_artifact(report, &journal_summary) ||
        !journal_summary) {
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-119: invalid report dump contract");
        return 0;
    }
    log_summary = &journal_summary->log_summary;

    if (!machine_journal_append_format(
            &builder,
            "machine_journal profile=%s log=%s timeline=%s origin-status=%s origin-pc=0x%zx origin-sp=0x%zx origin-segment=%zu mapped_bytes=%zu records=%zu record-index=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_log_resolution_kind_name(report->header_summary.log_resolution_kind),
            machine_timeline_resolution_kind_name(report->header_summary.timeline_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.journal_record_count,
            report->header_summary.journal_record_index) ||
        !machine_journal_append_format(
            &builder,
            "journal: resolution=%s kind=%s log=%s timeline=%s history=%s outcome=%s event=%s trace=%s change-class=%s apply=%s commit=%s writeback=%s mutation=%s effect=%s transition=%s action=%s raw=0x%02x value=0x%02x known=%s name=%s bytes=%zu",
            machine_journal_resolution_kind_name(journal_summary->resolution_kind),
            machine_journal_kind_name(journal_summary->journal_kind),
            machine_log_kind_name(journal_summary->log_kind),
            machine_timeline_kind_name(log_summary->timeline_kind),
            machine_history_kind_name(log_summary->history_kind),
            machine_outcome_kind_name(log_summary->outcome_kind),
            machine_event_kind_name(log_summary->event_kind),
            machine_trace_resolution_kind_name(log_summary->trace_resolution_kind),
            machine_trace_change_class_name(log_summary->trace_change_class),
            machine_apply_resolution_kind_name(log_summary->apply_resolution_kind),
            machine_commit_resolution_kind_name(log_summary->commit_resolution_kind),
            machine_writeback_resolution_kind_name(log_summary->writeback_resolution_kind),
            machine_mutation_resolution_kind_name(log_summary->mutation_resolution_kind),
            machine_mutation_effect_kind_name(log_summary->mutation_effect_kind),
            machine_transition_resolution_kind_name(log_summary->transition_resolution_kind),
            machine_interp_action_kind_name(log_summary->action_kind),
            (unsigned int)log_summary->raw_byte,
            (unsigned int)log_summary->tag_value,
            log_summary->is_known ? "yes" : "no",
            log_summary->tag_name ? log_summary->tag_name : "-",
            log_summary->instruction_byte_count) ||
        !machine_journal_append_payload_bytes(&builder, journal_summary) ||
        !machine_journal_append_immediate_hint(&builder, journal_summary) ||
        !machine_journal_append_format(
            &builder,
            " exact=%s single-record=%s records=%zu record-index=%zu origin-status=%s observed-status=%s status-changed=%s pc-changed=%s stack-changed=%s fetch-changed=%s",
            journal_summary->is_exact_journal ? "yes" : "no",
            journal_summary->is_single_record_journal ? "yes" : "no",
            journal_summary->journal_record_count,
            journal_summary->journal_record_index,
            machine_step_status_name(log_summary->origin_status),
            log_summary->has_observed_state ? machine_step_status_name(log_summary->observed_status) : "-",
            log_summary->has_status_change ? (log_summary->status_changed ? "yes" : "no") : "-",
            log_summary->has_program_counter_change ? (log_summary->program_counter_changed ? "yes" : "no") : "-",
            log_summary->has_stack_pointer_change ? (log_summary->stack_pointer_changed ? "yes" : "no") : "-",
            log_summary->has_fetch_change ? (log_summary->fetch_changed ? "yes" : "no") : "-") ||
        !machine_journal_append_targets(&builder, journal_summary) ||
        !machine_journal_append_return_immediate(&builder, journal_summary) ||
        !machine_journal_append_format(&builder, "\nreport_overview:\n") ||
        !machine_journal_append_format(
            &builder,
            "  origin: log=%s timeline=%s status=%s segment=%zu mapped-bytes=%zu pc=0x%zx sp=0x%zx records=%zu record-index=%zu\n",
            machine_log_resolution_kind_name(report->header_summary.log_resolution_kind),
            machine_timeline_resolution_kind_name(report->header_summary.timeline_resolution_kind),
            machine_step_status_name(report->header_summary.origin_step_status),
            report->header_summary.origin_current_segment_index,
            report->header_summary.mapped_byte_count,
            report->header_summary.origin_program_counter,
            report->header_summary.origin_stack_pointer,
            report->header_summary.journal_record_count,
            report->header_summary.journal_record_index) ||
        !machine_journal_append_format(
            &builder,
            "  policy: profile=%s exact=%s preview=%s single-record=%s\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.surfaces_exact_journal ? "yes" : "no",
            report->target_policy_summary.surfaces_preview_journal ? "yes" : "no",
            report->target_policy_summary.surfaces_single_record_journal ? "yes" : "no") ||
        !machine_journal_append_format(
            &builder,
            "  journal: resolution=%s kind=%s log=%s timeline=%s exact=%s single-record=%s records=%zu record-index=%zu state=%s status=%s pc=%s fetch=%s",
            machine_journal_resolution_kind_name(journal_summary->resolution_kind),
            machine_journal_kind_name(journal_summary->journal_kind),
            machine_log_kind_name(journal_summary->log_kind),
            machine_timeline_kind_name(log_summary->timeline_kind),
            journal_summary->is_exact_journal ? "yes" : "no",
            journal_summary->is_single_record_journal ? "yes" : "no",
            journal_summary->journal_record_count,
            journal_summary->journal_record_index,
            log_summary->has_observed_state ? "yes" : "no",
            log_summary->has_status_change ? (log_summary->status_changed ? "yes" : "no") : "-",
            log_summary->has_program_counter_change ? (log_summary->program_counter_changed ? "yes" : "no") : "-",
            log_summary->has_fetch_change ? (log_summary->fetch_changed ? "yes" : "no") : "-")) {
        free(builder.data);
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-120: out of memory building report dump");
        return 0;
    }
    if (!machine_journal_append_targets(&builder, journal_summary) ||
        !machine_journal_append_return_immediate(&builder, journal_summary) ||
        !machine_journal_append_format(&builder, "\n")) {
        free(builder.data);
        machine_journal_set_error(error, 0, 0, "MACHINE-JOURNAL-121: out of memory terminating report dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int machine_journal_build_report_dump_from_file(const MachineJournalFile *source,
    char **out_text,
    MachineJournalError *error) {
    MachineJournalReport report;
    int ok;

    machine_journal_report_init(&report);
    ok = machine_journal_build_report_from_file(source, &report, error) &&
        machine_journal_dump_report(&report, out_text, error);
    machine_journal_report_free(&report);
    return ok;
}

#define MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(name, input_type, builder) \
int name(const input_type *source, char **out_text, MachineJournalError *error) { \
    MachineJournalReport report; \
    int ok; \
    machine_journal_report_init(&report); \
    ok = builder(source, &report, error) && \
        machine_journal_dump_report(&report, out_text, error); \
    machine_journal_report_free(&report); \
    return ok; \
}

MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_log_file,
    MachineLogFile, machine_journal_build_report_from_machine_log_file)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_log_report,
    MachineLogReport, machine_journal_build_report_from_machine_log_report)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_timeline_file,
    MachineTimelineFile, machine_journal_build_report_from_machine_timeline_file)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_timeline_report,
    MachineTimelineReport, machine_journal_build_report_from_machine_timeline_report)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_history_file,
    MachineHistoryFile, machine_journal_build_report_from_machine_history_file)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_history_report,
    MachineHistoryReport, machine_journal_build_report_from_machine_history_report)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_outcome_file,
    MachineOutcomeFile, machine_journal_build_report_from_machine_outcome_file)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_outcome_report,
    MachineOutcomeReport, machine_journal_build_report_from_machine_outcome_report)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_event_file,
    MachineEventFile, machine_journal_build_report_from_machine_event_file)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_event_report,
    MachineEventReport, machine_journal_build_report_from_machine_event_report)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_trace_file,
    MachineTraceFile, machine_journal_build_report_from_machine_trace_file)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_trace_report,
    MachineTraceReport, machine_journal_build_report_from_machine_trace_report)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_delta_file,
    MachineDeltaFile, machine_journal_build_report_from_machine_delta_file)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_delta_report,
    MachineDeltaReport, machine_journal_build_report_from_machine_delta_report)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_observe_file,
    MachineObserveFile, machine_journal_build_report_from_machine_observe_file)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_observe_report,
    MachineObserveReport, machine_journal_build_report_from_machine_observe_report)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_apply_file,
    MachineApplyFile, machine_journal_build_report_from_machine_apply_file)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_apply_report,
    MachineApplyReport, machine_journal_build_report_from_machine_apply_report)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_commit_file,
    MachineCommitFile, machine_journal_build_report_from_machine_commit_file)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_commit_report,
    MachineCommitReport, machine_journal_build_report_from_machine_commit_report)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_step_file,
    MachineStepFile, machine_journal_build_report_from_machine_step_file)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_step_report,
    MachineStepReport, machine_journal_build_report_from_machine_step_report)
MACHINE_JOURNAL_DEFINE_REPORT_DUMP_FROM_WRAPPER(machine_journal_build_report_dump_from_machine_ir_report,
    MachineIrAllocateRewriteReport, machine_journal_build_report_from_machine_ir_report)

int machine_journal_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineJournalError *error) {
    MachineJournalReport report_file;
    int ok;

    machine_journal_report_init(&report_file);
    ok = machine_journal_build_report_from_machine_ir_report_with_profile(
            report, profile, &report_file, error) &&
        machine_journal_dump_report(&report_file, out_text, error);
    machine_journal_report_free(&report_file);
    return ok;
}
