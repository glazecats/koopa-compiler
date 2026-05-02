#include "machine/load.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineLoadStringBuilder;

static void machine_load_set_error(MachineLoadError *error, int line, int column, const char *fmt, ...);
static char *machine_load_strdup(const char *text);
static int machine_load_append_format(MachineLoadStringBuilder *builder, const char *fmt, ...);
static size_t machine_load_segment_alignment(MachineElfTargetProfile profile);
static size_t machine_load_align_up(size_t value, size_t alignment);
static int machine_load_segment_covers_virtual_address(const MachineLoadSegment *segment,
    size_t virtual_address);
static int machine_load_report_get_segment_filter_storage(const MachineLoadReport *report,
    MachineLoadReportSegmentFilterKind filter_kind,
    const size_t **out_indices,
    size_t *out_count);
static const char *machine_load_segment_filter_kind_name(MachineLoadReportSegmentFilterKind filter_kind);


#define MACHINE_LOAD_SPLIT_AGGREGATOR
#include "machine_load_core.inc"
#include "machine_load_query.inc"
#include "machine_load_build.inc"
#include "machine_load_dump.inc"
#include "machine_load_report.inc"
