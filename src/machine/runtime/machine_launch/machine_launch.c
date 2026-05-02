#include "machine/launch.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} MachineLaunchStringBuilder;

static void machine_launch_set_error(MachineLaunchError *error, int line, int column, const char *fmt, ...);
static char *machine_launch_strdup(const char *text);
static int machine_launch_append_format(MachineLaunchStringBuilder *builder, const char *fmt, ...);
static const char *machine_launch_program_counter_register_name(MachineElfTargetProfile profile);
static const char *machine_launch_stack_pointer_register_name(MachineElfTargetProfile profile);


#define MACHINE_LAUNCH_SPLIT_AGGREGATOR
#include "machine_launch_core.inc"
#include "machine_launch_query.inc"
#include "machine_launch_build.inc"
#include "machine_launch_dump.inc"
#include "machine_launch_report.inc"
