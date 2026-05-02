#if defined(__INTELLISENSE__) && !defined(MACHINE_EXEC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_EXEC_SPLIT_AGGREGATOR)
#define MACHINE_EXEC_SPLIT_INTELLISENSE_CONTEXT
#include "machine_exec.c"

#else

static void machine_exec_set_error(MachineExecError *error, int line, int column, const char *fmt, ...) {
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

static char *machine_exec_strdup(const char *text) {
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

static int machine_exec_append_format(MachineExecStringBuilder *builder, const char *fmt, ...) {
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

static int machine_exec_is_executable_segment_name(const char *segment_name) {
    if (!segment_name) {
        return 0;
    }
    return strcmp(segment_name, ".text") == 0 || strcmp(segment_name, ".cold") == 0;
}

static int machine_exec_segment_covers_virtual_address(const MachineExecSegment *segment,
    size_t virtual_address) {
    size_t delta;

    if (!segment || segment->byte_count == 0u || virtual_address < segment->virtual_address) {
        return 0;
    }
    delta = virtual_address - segment->virtual_address;
    return delta < segment->byte_count;
}

static int machine_exec_report_get_segment_filter_storage(const MachineExecReport *report,
    MachineExecReportSegmentFilterKind filter_kind,
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
        case MACHINE_EXEC_SEGMENT_FILTER_EXECUTABLE:
            if (out_indices) {
                *out_indices = report->executable_segment_indices;
            }
            if (out_count) {
                *out_count = report->executable_segment_count;
            }
            return 1;
        case MACHINE_EXEC_SEGMENT_FILTER_NON_EXECUTABLE:
            if (out_indices) {
                *out_indices = report->non_executable_segment_indices;
            }
            if (out_count) {
                *out_count = report->non_executable_segment_count;
            }
            return 1;
        case MACHINE_EXEC_SEGMENT_FILTER_ENTRY:
            if (out_indices) {
                *out_indices = report->segment_summary_count > 0u ? report->executable_segment_indices : NULL;
            }
            if (out_count) {
                *out_count = 1u;
            }
            return 1;
    }
    return 0;
}

static const char *machine_exec_segment_filter_kind_name(MachineExecReportSegmentFilterKind filter_kind) {
    switch (filter_kind) {
        case MACHINE_EXEC_SEGMENT_FILTER_EXECUTABLE:
            return "executable";
        case MACHINE_EXEC_SEGMENT_FILTER_NON_EXECUTABLE:
            return "non-executable";
        case MACHINE_EXEC_SEGMENT_FILTER_ENTRY:
            return "entry";
    }
    return "unknown";
}

void machine_exec_file_init(MachineExecFile *exec_file) {
    if (!exec_file) {
        return;
    }
    machine_image_file_init(&exec_file->image_file);
    exec_file->entry_virtual_address = 0u;
    exec_file->segments = NULL;
    exec_file->segment_count = 0u;
    exec_file->segment_capacity = 0u;
}

void machine_exec_report_init(MachineExecReport *report) {
    if (!report) {
        return;
    }
    machine_exec_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    report->segment_summaries = NULL;
    report->segment_summary_count = 0u;
    report->executable_segment_indices = NULL;
    report->executable_segment_count = 0u;
    report->non_executable_segment_indices = NULL;
    report->non_executable_segment_count = 0u;
}

void machine_exec_file_free(MachineExecFile *exec_file) {
    size_t segment_index;

    if (!exec_file) {
        return;
    }
    for (segment_index = 0u; segment_index < exec_file->segment_count; ++segment_index) {
        free(exec_file->segments[segment_index].name);
    }
    free(exec_file->segments);
    machine_image_file_free(&exec_file->image_file);
    machine_exec_file_init(exec_file);
}

void machine_exec_report_free(MachineExecReport *report) {
    if (!report) {
        return;
    }
    machine_exec_file_free(&report->file);
    free(report->segment_summaries);
    free(report->executable_segment_indices);
    free(report->non_executable_segment_indices);
    machine_exec_report_init(report);
}

int machine_exec_clone_file(const MachineExecFile *source,
    MachineExecFile *out_clone,
    MachineExecError *error) {
    size_t segment_index;
    MachineImageError image_error;

    memset(&image_error, 0, sizeof(image_error));
    if (!source || !out_clone) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-124: invalid exec clone contract");
        return 0;
    }
    if (!machine_exec_verify_file(source, error)) {
        return 0;
    }

    machine_exec_file_free(out_clone);
    if (!machine_image_clone_file(&source->image_file, &out_clone->image_file, &image_error)) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-125: failed cloning exec image file: %s", image_error.message);
        machine_exec_file_free(out_clone);
        return 0;
    }
    out_clone->entry_virtual_address = source->entry_virtual_address;
    out_clone->segment_count = source->segment_count;
    out_clone->segment_capacity = source->segment_count;
    out_clone->segments = source->segment_count > 0u
        ? (MachineExecSegment *)calloc(source->segment_count, sizeof(MachineExecSegment))
        : NULL;
    if (source->segment_count > 0u && !out_clone->segments) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-126: out of memory cloning exec segments");
        machine_exec_file_free(out_clone);
        return 0;
    }
    for (segment_index = 0u; segment_index < source->segment_count; ++segment_index) {
        const MachineExecSegment *src = &source->segments[segment_index];
        MachineExecSegment *dst = &out_clone->segments[segment_index];

        dst->name = machine_exec_strdup(src->name ? src->name : "");
        dst->image_segment_index = src->image_segment_index;
        dst->virtual_address = src->virtual_address;
        dst->byte_count = src->byte_count;
        dst->readable = src->readable;
        dst->writable = src->writable;
        dst->executable = src->executable;
        if (!dst->name) {
            machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-127: out of memory cloning exec segment name");
            machine_exec_file_free(out_clone);
            return 0;
        }
    }
    if (!machine_exec_verify_file(out_clone, error)) {
        machine_exec_file_free(out_clone);
        return 0;
    }
    return 1;
}

#endif
