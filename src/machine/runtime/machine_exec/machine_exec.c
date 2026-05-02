#include "machine/exec.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineExecStringBuilder;

static void machine_exec_set_error(MachineExecError *error, int line, int column, const char *fmt, ...);
static char *machine_exec_strdup(const char *text);
static int machine_exec_append_format(MachineExecStringBuilder *builder, const char *fmt, ...);
static int machine_exec_is_executable_segment_name(const char *segment_name);
static int machine_exec_segment_covers_virtual_address(const MachineExecSegment *segment,
    size_t virtual_address);
static int machine_exec_report_get_segment_filter_storage(const MachineExecReport *report,
    MachineExecReportSegmentFilterKind filter_kind,
    const size_t **out_indices,
    size_t *out_count);
static const char *machine_exec_segment_filter_kind_name(MachineExecReportSegmentFilterKind filter_kind);

#define MACHINE_EXEC_SPLIT_AGGREGATOR
#include "machine_exec_core.inc"
#include "machine_exec_query.inc"
#include "machine_exec_build.inc"
#include "machine_exec_dump.inc"
#include "machine_exec_report.inc"
