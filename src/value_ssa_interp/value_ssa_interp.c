#include "value_ssa_interp.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const ValueSsaProgram *program;
    const ValueSsaInterpOptions *options;
    long long *global_values;
    size_t step_count;
    size_t call_depth;
} ValueSsaInterpContext;

typedef struct {
    const ValueSsaFunction *function;
    long long *local_values;
    unsigned char *has_local_value;
    long long *ssa_values;
    unsigned char *has_ssa_value;
} ValueSsaInterpFrame;

static void value_ssa_interp_set_error(ValueSsaInterpError *error, int line, int column, const char *fmt, ...);
static const ValueSsaFunction *value_ssa_interp_find_function(const ValueSsaProgram *program, const char *name);
static int value_ssa_interp_eval_value(const ValueSsaInterpFrame *frame,
    ValueSsaValueRef value,
    long long *out_value,
    ValueSsaInterpError *error);
static int value_ssa_interp_eval_binary(ValueSsaBinaryOp op,
    long long lhs,
    long long rhs,
    long long *out_value,
    ValueSsaInterpError *error);
static int value_ssa_interp_execute_function_internal(ValueSsaInterpContext *context,
    const ValueSsaFunction *function,
    const long long *args,
    size_t arg_count,
    long long *out_return_value,
    ValueSsaInterpError *error);

#define VALUE_SSA_INTERP_SPLIT_AGGREGATOR
#include "value_ssa_interp_state.inc"
#include "value_ssa_interp_exec.inc"
