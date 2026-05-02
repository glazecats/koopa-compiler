#include "machine/runtime.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineRuntimeStringBuilder;

static void machine_runtime_set_error(MachineRuntimeError *error, int line, int column, const char *fmt, ...);
static char *machine_runtime_strdup(const char *text);
static int machine_runtime_append_format(MachineRuntimeStringBuilder *builder, const char *fmt, ...);
static size_t machine_runtime_stack_alignment(MachineElfTargetProfile profile);
static size_t machine_runtime_stack_byte_count(MachineElfTargetProfile profile);
static size_t machine_runtime_stack_gap_byte_count(MachineElfTargetProfile profile);
static size_t machine_runtime_align_up(size_t value, size_t alignment);
static int machine_runtime_segment_covers_virtual_address(const MachineRuntimeSegment *segment,
    size_t virtual_address);
static const char *machine_runtime_segment_kind_name(MachineRuntimeSegmentKind kind);
static const char *machine_runtime_segment_filter_kind_name(MachineRuntimeReportSegmentFilterKind filter_kind);
static int machine_runtime_file_compute_bounds(const MachineRuntimeFile *runtime_file,
    size_t *out_base_virtual_address,
    size_t *out_end_virtual_address);


#define MACHINE_RUNTIME_SPLIT_AGGREGATOR
#include "machine_runtime_core.inc"
#include "machine_runtime_query.inc"
#include "machine_runtime_build.inc"
#include "machine_runtime_dump.inc"
#include "machine_runtime_report.inc"
