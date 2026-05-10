#if defined(__INTELLISENSE__) && !defined(MACHINE_LOAD_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LOAD_SPLIT_AGGREGATOR)
#define MACHINE_LOAD_SPLIT_INTELLISENSE_CONTEXT
#include "machine_load.c"

#else

static void machine_load_set_error(MachineLoadError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (!error || !fmt) {
        return;
    }
    error->line = line;
    error->column = column;
    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

static char *machine_load_strdup(const char *text) {
    size_t length;
    char *copy;

    if (!text) {
        return NULL;
    }
    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, length + 1u);
    return copy;
}

static int machine_load_append_format(MachineLoadStringBuilder *builder, const char *fmt, ...) {
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

static size_t machine_load_segment_alignment(MachineElfTargetProfile profile) {
    (void)profile;
    return 0x1000u;
}

static size_t machine_load_align_up(size_t value, size_t alignment) {
    size_t remainder;

    if (alignment == 0u) {
        return value;
    }
    remainder = value % alignment;
    if (remainder == 0u) {
        return value;
    }
    return value + (alignment - remainder);
}

static int machine_load_try_add_size(size_t base, size_t delta, size_t *out_sum) {
    if (!out_sum) {
        return 0;
    }
    if (delta > ((size_t)-1) - base) {
        return 0;
    }
    *out_sum = base + delta;
    return 1;
}

static int machine_load_segment_covers_virtual_address(const MachineLoadSegment *segment,
    size_t virtual_address) {
    size_t delta;

    if (!segment || segment->memory_byte_count == 0u || virtual_address < segment->virtual_address) {
        return 0;
    }
    delta = virtual_address - segment->virtual_address;
    return delta < segment->memory_byte_count;
}

static int machine_load_report_get_segment_filter_storage(const MachineLoadReport *report,
    MachineLoadReportSegmentFilterKind filter_kind,
    const size_t **out_indices,
    size_t *out_count) {
    if (!report) {
        return 0;
    }
    if (out_indices) {
        *out_indices = NULL;
    }
    if (out_count) {
        *out_count = 0u;
    }
    switch (filter_kind) {
        case MACHINE_LOAD_SEGMENT_FILTER_EXECUTABLE:
            if (out_indices) {
                *out_indices = report->executable_segment_indices;
            }
            if (out_count) {
                *out_count = report->executable_segment_count;
            }
            return 1;
        case MACHINE_LOAD_SEGMENT_FILTER_NON_EXECUTABLE:
            if (out_indices) {
                *out_indices = report->non_executable_segment_indices;
            }
            if (out_count) {
                *out_count = report->non_executable_segment_count;
            }
            return 1;
        case MACHINE_LOAD_SEGMENT_FILTER_ENTRY:
            if (out_count) {
                *out_count = report->segment_summary_count > 0u ? 1u : 0u;
            }
            return 1;
    }
    return 0;
}

static const char *machine_load_segment_filter_kind_name(MachineLoadReportSegmentFilterKind filter_kind) {
    switch (filter_kind) {
        case MACHINE_LOAD_SEGMENT_FILTER_EXECUTABLE:
            return "executable";
        case MACHINE_LOAD_SEGMENT_FILTER_NON_EXECUTABLE:
            return "non-executable";
        case MACHINE_LOAD_SEGMENT_FILTER_ENTRY:
            return "entry";
    }
    return "unknown";
}

void machine_load_file_init(MachineLoadFile *load_file) {
    if (!load_file) {
        return;
    }
    machine_exec_file_init(&load_file->exec_file);
    load_file->entry_virtual_address = 0u;
    load_file->segments = NULL;
    load_file->segment_count = 0u;
    load_file->segment_capacity = 0u;
    load_file->total_memory_byte_count = 0u;
}

void machine_load_report_init(MachineLoadReport *report) {
    if (!report) {
        return;
    }
    machine_load_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    memset(&report->memory_summary, 0, sizeof(report->memory_summary));
    report->segment_summaries = NULL;
    report->segment_summary_count = 0u;
    report->executable_segment_indices = NULL;
    report->executable_segment_count = 0u;
    report->non_executable_segment_indices = NULL;
    report->non_executable_segment_count = 0u;
}

void machine_load_file_free(MachineLoadFile *load_file) {
    size_t segment_index;

    if (!load_file) {
        return;
    }
    for (segment_index = 0u; segment_index < load_file->segment_count; ++segment_index) {
        free(load_file->segments[segment_index].name);
        free(load_file->segments[segment_index].bytes);
    }
    free(load_file->segments);
    machine_exec_file_free(&load_file->exec_file);
    machine_load_file_init(load_file);
}

void machine_load_report_free(MachineLoadReport *report) {
    if (!report) {
        return;
    }
    machine_load_file_free(&report->file);
    free(report->segment_summaries);
    free(report->executable_segment_indices);
    free(report->non_executable_segment_indices);
    machine_load_report_init(report);
}

int machine_load_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineLoadTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->base_virtual_address =
        profile == MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ? 0x08048000u : 0x1000u;
    out_summary->segment_alignment = machine_load_segment_alignment(profile);
    return 1;
}

int machine_load_file_get_target_policy_summary(const MachineLoadFile *load_file,
    MachineLoadTargetPolicySummary *out_summary) {
    if (!load_file || !out_summary) {
        return 0;
    }
    return machine_load_get_target_policy_summary(load_file->exec_file.image_file.target_profile, out_summary);
}

int machine_load_clone_file(const MachineLoadFile *source,
    MachineLoadFile *out_clone,
    MachineLoadError *error) {
    size_t segment_index;

    if (!source || !out_clone) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-100: invalid load clone contract");
        return 0;
    }
    if (!machine_load_verify_file(source, error)) {
        return 0;
    }

    machine_load_file_free(out_clone);
    if (!machine_exec_clone_file(&source->exec_file, &out_clone->exec_file, NULL)) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-101: failed cloning embedded exec file");
        machine_load_file_free(out_clone);
        return 0;
    }
    out_clone->entry_virtual_address = source->entry_virtual_address;
    out_clone->segment_count = source->segment_count;
    out_clone->segment_capacity = source->segment_count;
    out_clone->total_memory_byte_count = source->total_memory_byte_count;
    out_clone->segments = source->segment_count > 0u
        ? (MachineLoadSegment *)calloc(source->segment_count, sizeof(MachineLoadSegment))
        : NULL;
    if (source->segment_count > 0u && !out_clone->segments) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-102: out of memory cloning load segments");
        machine_load_file_free(out_clone);
        return 0;
    }
    for (segment_index = 0u; segment_index < source->segment_count; ++segment_index) {
        const MachineLoadSegment *src = &source->segments[segment_index];
        MachineLoadSegment *dst = &out_clone->segments[segment_index];

        dst->name = machine_load_strdup(src->name ? src->name : "");
        dst->exec_segment_index = src->exec_segment_index;
        dst->virtual_address = src->virtual_address;
        dst->file_byte_count = src->file_byte_count;
        dst->memory_byte_count = src->memory_byte_count;
        dst->readable = src->readable;
        dst->writable = src->writable;
        dst->executable = src->executable;
        dst->bytes = src->memory_byte_count > 0u ? (unsigned char *)malloc(src->memory_byte_count) : NULL;
        if (!dst->name || (src->memory_byte_count > 0u && !dst->bytes)) {
            machine_load_set_error(error, 0, 0, "MACHINE-LOAD-103: out of memory cloning load segment");
            machine_load_file_free(out_clone);
            return 0;
        }
        if (src->memory_byte_count > 0u) {
            memcpy(dst->bytes, src->bytes, src->memory_byte_count);
        }
    }
    if (!machine_load_verify_file(out_clone, error)) {
        machine_load_file_free(out_clone);
        return 0;
    }
    return 1;
}


#endif
