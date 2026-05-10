#if defined(__INTELLISENSE__) && !defined(MACHINE_RUNTIME_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_RUNTIME_SPLIT_AGGREGATOR)
#define MACHINE_RUNTIME_SPLIT_INTELLISENSE_CONTEXT
#include "machine_runtime.c"

#else

static void machine_runtime_set_error(MachineRuntimeError *error, int line, int column, const char *fmt, ...) {
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

static char *machine_runtime_strdup(const char *text) {
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

static int machine_runtime_append_format(MachineRuntimeStringBuilder *builder, const char *fmt, ...) {
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

static size_t machine_runtime_stack_alignment(MachineElfTargetProfile profile) {
    (void)profile;
    return 0x1000u;
}

static size_t machine_runtime_stack_byte_count(MachineElfTargetProfile profile) {
    (void)profile;
    return 0x1000u;
}

static size_t machine_runtime_stack_gap_byte_count(MachineElfTargetProfile profile) {
    (void)profile;
    return 0x1000u;
}

static size_t machine_runtime_target_word_byte_count(MachineElfTargetProfile profile) {
    (void)profile;
    return sizeof(uint32_t);
}

static size_t machine_runtime_align_up(size_t value, size_t alignment) {
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

static int machine_runtime_segment_covers_virtual_address(const MachineRuntimeSegment *segment,
    size_t virtual_address) {
    size_t delta;

    if (!segment || segment->byte_count == 0u || virtual_address < segment->virtual_address) {
        return 0;
    }
    delta = virtual_address - segment->virtual_address;
    return delta < segment->byte_count;
}

static const char *machine_runtime_segment_kind_name(MachineRuntimeSegmentKind kind) {
    switch (kind) {
        case MACHINE_RUNTIME_SEGMENT_KIND_LOAD:
            return "load";
        case MACHINE_RUNTIME_SEGMENT_KIND_STACK:
            return "stack";
    }
    return "unknown";
}

static int machine_runtime_region_summary_covers_virtual_address(const MachineRuntimeRegionSummary *summary,
    size_t virtual_address) {
    if (!summary || summary->byte_count == 0u || virtual_address < summary->base_virtual_address) {
        return 0;
    }
    return virtual_address < summary->end_virtual_address;
}

static const char *machine_runtime_segment_filter_kind_name(MachineRuntimeReportSegmentFilterKind filter_kind) {
    switch (filter_kind) {
        case MACHINE_RUNTIME_SEGMENT_FILTER_EXECUTABLE:
            return "executable";
        case MACHINE_RUNTIME_SEGMENT_FILTER_NON_EXECUTABLE:
            return "non-executable";
        case MACHINE_RUNTIME_SEGMENT_FILTER_ENTRY:
            return "entry";
        case MACHINE_RUNTIME_SEGMENT_FILTER_STACK:
            return "stack";
    }
    return "unknown";
}

static int machine_runtime_try_add_size(size_t base, size_t delta, size_t *out_sum) {
    if (!out_sum) {
        return 0;
    }
    if (delta > ((size_t)-1) - base) {
        return 0;
    }
    *out_sum = base + delta;
    return 1;
}

static int machine_runtime_file_compute_bounds(const MachineRuntimeFile *runtime_file,
    size_t *out_base_virtual_address,
    size_t *out_end_virtual_address) {
    size_t segment_index;
    size_t base_virtual_address;
    size_t end_virtual_address;

    if (!runtime_file || runtime_file->segment_count == 0u || !runtime_file->segments) {
        return 0;
    }

    base_virtual_address = runtime_file->segments[0].virtual_address;
    if (!machine_runtime_try_add_size(
            runtime_file->segments[0].virtual_address,
            runtime_file->segments[0].byte_count,
            &end_virtual_address)) {
        return 0;
    }
    for (segment_index = 1u; segment_index < runtime_file->segment_count; ++segment_index) {
        const MachineRuntimeSegment *segment = &runtime_file->segments[segment_index];
        size_t segment_end = 0u;

        if (!machine_runtime_try_add_size(segment->virtual_address, segment->byte_count, &segment_end)) {
            return 0;
        }

        if (segment->virtual_address < base_virtual_address) {
            base_virtual_address = segment->virtual_address;
        }
        if (segment_end > end_virtual_address) {
            end_virtual_address = segment_end;
        }
    }

    if (out_base_virtual_address) {
        *out_base_virtual_address = base_virtual_address;
    }
    if (out_end_virtual_address) {
        *out_end_virtual_address = end_virtual_address;
    }
    return 1;
}

void machine_runtime_file_init(MachineRuntimeFile *runtime_file) {
    if (!runtime_file) {
        return;
    }
    machine_load_file_init(&runtime_file->load_file);
    runtime_file->entry_virtual_address = 0u;
    runtime_file->initial_stack_pointer = 0u;
    runtime_file->segments = NULL;
    runtime_file->segment_count = 0u;
    runtime_file->segment_capacity = 0u;
    runtime_file->total_mapped_byte_count = 0u;
    runtime_file->stack_segment_index = 0u;
    runtime_file->has_stack_segment = 0;
}

void machine_runtime_report_init(MachineRuntimeReport *report) {
    if (!report) {
        return;
    }
    machine_runtime_file_init(&report->file);
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    memset(&report->memory_summary, 0, sizeof(report->memory_summary));
    memset(&report->stack_summary, 0, sizeof(report->stack_summary));
    memset(&report->gap_summary, 0, sizeof(report->gap_summary));
    memset(&report->launch_summary, 0, sizeof(report->launch_summary));
    memset(&report->initial_stack_summary, 0, sizeof(report->initial_stack_summary));
    report->segment_summaries = NULL;
    report->segment_summary_count = 0u;
    report->executable_segment_indices = NULL;
    report->executable_segment_count = 0u;
    report->non_executable_segment_indices = NULL;
    report->non_executable_segment_count = 0u;
    report->stack_segment_index = 0u;
    report->has_stack_segment = 0;
}

void machine_runtime_file_free(MachineRuntimeFile *runtime_file) {
    size_t segment_index;

    if (!runtime_file) {
        return;
    }
    for (segment_index = 0u; segment_index < runtime_file->segment_count; ++segment_index) {
        free(runtime_file->segments[segment_index].name);
        free(runtime_file->segments[segment_index].bytes);
    }
    free(runtime_file->segments);
    machine_load_file_free(&runtime_file->load_file);
    machine_runtime_file_init(runtime_file);
}

void machine_runtime_report_free(MachineRuntimeReport *report) {
    if (!report) {
        return;
    }
    machine_runtime_file_free(&report->file);
    free(report->segment_summaries);
    free(report->executable_segment_indices);
    free(report->non_executable_segment_indices);
    machine_runtime_report_init(report);
}

int machine_runtime_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineRuntimeTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->base_virtual_address =
        profile == MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW ? 0x08048000u : 0x1000u;
    out_summary->stack_alignment = machine_runtime_stack_alignment(profile);
    out_summary->stack_byte_count = machine_runtime_stack_byte_count(profile);
    out_summary->stack_gap_byte_count = machine_runtime_stack_gap_byte_count(profile);
    return 1;
}

int machine_runtime_file_get_target_policy_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeTargetPolicySummary *out_summary) {
    if (!runtime_file || !out_summary) {
        return 0;
    }
    return machine_runtime_get_target_policy_summary(
        runtime_file->load_file.exec_file.image_file.target_profile, out_summary);
}

int machine_runtime_verify_file(const MachineRuntimeFile *runtime_file,
    MachineRuntimeError *error) {
    size_t segment_index;
    size_t other_index;
    size_t total_mapped_byte_count = 0u;
    size_t stack_segment_count = 0u;
    const MachineRuntimeSegment *stack_segment = NULL;

    if (!runtime_file) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-100: invalid runtime file");
        return 0;
    }
    if (!machine_load_verify_file(&runtime_file->load_file, NULL)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-101: invalid load input");
        return 0;
    }
    if (runtime_file->segment_count == 0u || !runtime_file->segments) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-102: runtime file must materialize segments");
        return 0;
    }
    if (runtime_file->segment_capacity < runtime_file->segment_count) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-103: runtime segment capacity mismatch");
        return 0;
    }
    if (runtime_file->entry_virtual_address != runtime_file->load_file.entry_virtual_address) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-104: runtime entry must match load entry");
        return 0;
    }

    for (segment_index = 0u; segment_index < runtime_file->segment_count; ++segment_index) {
        const MachineRuntimeSegment *segment = &runtime_file->segments[segment_index];
        size_t segment_end = 0u;

        if (!segment->name) {
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-105: runtime segment missing name");
            return 0;
        }
        if (segment->byte_count > 0u && !segment->bytes) {
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-106: runtime segment missing bytes");
            return 0;
        }
        if (!machine_runtime_try_add_size(segment->virtual_address, segment->byte_count, &segment_end)) {
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-111: overlapping runtime segments");
            return 0;
        }
        if (segment->kind == MACHINE_RUNTIME_SEGMENT_KIND_LOAD) {
            const MachineLoadSegment *load_segment = NULL;

            if (segment->load_segment_index >= runtime_file->load_file.segment_count) {
                machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-107: runtime load segment missing source");
                return 0;
            }
            load_segment = &runtime_file->load_file.segments[segment->load_segment_index];
            if (segment->virtual_address != load_segment->virtual_address ||
                segment->byte_count != load_segment->memory_byte_count ||
                segment->readable != load_segment->readable ||
                segment->writable != load_segment->writable ||
                segment->executable != load_segment->executable) {
                machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-108: runtime load segment diverged from source");
                return 0;
            }
        } else if (segment->kind == MACHINE_RUNTIME_SEGMENT_KIND_STACK) {
            if (segment->load_segment_index != (size_t)-1 ||
                !segment->readable || !segment->writable || segment->executable) {
                machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-109: invalid runtime stack segment");
                return 0;
            }
            ++stack_segment_count;
            stack_segment = segment;
        } else {
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-110: unknown runtime segment kind");
            return 0;
        }

        for (other_index = segment_index + 1u; other_index < runtime_file->segment_count; ++other_index) {
            const MachineRuntimeSegment *other = &runtime_file->segments[other_index];
            size_t other_end = 0u;

            if (!machine_runtime_try_add_size(other->virtual_address, other->byte_count, &other_end)) {
                machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-111: overlapping runtime segments");
                return 0;
            }

            if (segment->virtual_address < other_end && other->virtual_address < segment_end) {
                machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-111: overlapping runtime segments");
                return 0;
            }
        }

        if (total_mapped_byte_count > ((size_t)-1) - segment->byte_count) {
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-112: runtime mapped-byte count mismatch");
            return 0;
        }
        total_mapped_byte_count += segment->byte_count;
    }

    if (total_mapped_byte_count != runtime_file->total_mapped_byte_count) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-112: runtime mapped-byte count mismatch");
        return 0;
    }
    if (stack_segment_count != 1u || !runtime_file->has_stack_segment ||
        runtime_file->stack_segment_index >= runtime_file->segment_count ||
        runtime_file->segments[runtime_file->stack_segment_index].kind != MACHINE_RUNTIME_SEGMENT_KIND_STACK) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-113: runtime file must have exactly one stack segment");
        return 0;
    }
    stack_segment = &runtime_file->segments[runtime_file->stack_segment_index];
    {
        size_t stack_end = 0u;

        if (!stack_segment ||
            !machine_runtime_try_add_size(
                stack_segment->virtual_address, stack_segment->byte_count, &stack_end) ||
            runtime_file->initial_stack_pointer != stack_end) {
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-114: runtime stack pointer must target stack top");
            return 0;
        }
    }
    if (!machine_runtime_file_get_entry_segment(runtime_file, NULL, NULL)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-115: runtime entry must land inside an executable segment");
        return 0;
    }
    return 1;
}

int machine_runtime_clone_file(const MachineRuntimeFile *source,
    MachineRuntimeFile *out_clone,
    MachineRuntimeError *error) {
    size_t segment_index;

    if (!source || !out_clone) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-116: invalid runtime clone contract");
        return 0;
    }
    if (!machine_runtime_verify_file(source, error)) {
        return 0;
    }

    machine_runtime_file_free(out_clone);
    if (!machine_load_clone_file(&source->load_file, &out_clone->load_file, NULL)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-117: failed cloning load input");
        machine_runtime_file_free(out_clone);
        return 0;
    }

    out_clone->entry_virtual_address = source->entry_virtual_address;
    out_clone->initial_stack_pointer = source->initial_stack_pointer;
    out_clone->segment_count = source->segment_count;
    out_clone->segment_capacity = source->segment_count;
    out_clone->total_mapped_byte_count = source->total_mapped_byte_count;
    out_clone->stack_segment_index = source->stack_segment_index;
    out_clone->has_stack_segment = source->has_stack_segment;
    out_clone->segments = source->segment_count > 0u
        ? (MachineRuntimeSegment *)calloc(source->segment_count, sizeof(MachineRuntimeSegment))
        : NULL;
    if (source->segment_count > 0u && !out_clone->segments) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-118: out of memory cloning runtime segments");
        machine_runtime_file_free(out_clone);
        return 0;
    }

    for (segment_index = 0u; segment_index < source->segment_count; ++segment_index) {
        const MachineRuntimeSegment *src = &source->segments[segment_index];
        MachineRuntimeSegment *dst = &out_clone->segments[segment_index];

        dst->name = machine_runtime_strdup(src->name ? src->name : "");
        dst->kind = src->kind;
        dst->load_segment_index = src->load_segment_index;
        dst->virtual_address = src->virtual_address;
        dst->byte_count = src->byte_count;
        dst->readable = src->readable;
        dst->writable = src->writable;
        dst->executable = src->executable;
        dst->bytes = src->byte_count > 0u ? (unsigned char *)malloc(src->byte_count) : NULL;
        if (!dst->name || (src->byte_count > 0u && !dst->bytes)) {
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-119: out of memory cloning runtime segment payload");
            machine_runtime_file_free(out_clone);
            return 0;
        }
        if (src->byte_count > 0u) {
            memcpy(dst->bytes, src->bytes, src->byte_count);
        }
    }

    if (!machine_runtime_verify_file(out_clone, error)) {
        machine_runtime_file_free(out_clone);
        return 0;
    }
    return 1;
}


#endif
