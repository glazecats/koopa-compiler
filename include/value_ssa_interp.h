#ifndef VALUE_SSA_INTERP_H
#define VALUE_SSA_INTERP_H

#include <stddef.h>

#include "value_ssa.h"

typedef struct {
    int line;
    int column;
    char message[512];
} ValueSsaInterpError;

typedef int (*ValueSsaInterpExternCallFn)(const ValueSsaProgram *program,
    const char *callee_name,
    const long long *args,
    size_t arg_count,
    int has_result,
    long long *out_result,
    long long *global_values,
    size_t global_count,
    void *user_data,
    ValueSsaInterpError *error);

typedef struct {
    ValueSsaInterpExternCallFn extern_call;
    void *extern_call_user_data;
    size_t max_steps;
    size_t max_call_depth;
} ValueSsaInterpOptions;

typedef struct {
    long long return_value;
    long long *global_values;
    size_t global_count;
} ValueSsaInterpResult;

void value_ssa_interp_options_init(ValueSsaInterpOptions *options);
void value_ssa_interp_result_init(ValueSsaInterpResult *result);
void value_ssa_interp_result_free(ValueSsaInterpResult *result);

int value_ssa_interp_execute_function(const ValueSsaProgram *program,
    const char *function_name,
    const long long *args,
    size_t arg_count,
    const ValueSsaInterpOptions *options,
    ValueSsaInterpResult *out_result,
    ValueSsaInterpError *error);

int value_ssa_interp_execute_main(const ValueSsaProgram *program,
    const long long *args,
    size_t arg_count,
    const ValueSsaInterpOptions *options,
    ValueSsaInterpResult *out_result,
    ValueSsaInterpError *error);

#endif
