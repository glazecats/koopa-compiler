#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_INTERP_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_INTERP_SPLIT_AGGREGATOR)
#define VALUE_SSA_INTERP_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_interp.c"
#else

static void value_ssa_interp_set_error(ValueSsaInterpError *error, int line, int column, const char *fmt, ...) {
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

void value_ssa_interp_options_init(ValueSsaInterpOptions *options) {
    if (!options) {
        return;
    }

    options->extern_call = NULL;
    options->extern_call_user_data = NULL;
    options->max_steps = 100000;
    options->max_call_depth = 256;
}

void value_ssa_interp_result_init(ValueSsaInterpResult *result) {
    if (!result) {
        return;
    }

    result->return_value = 0;
    result->global_values = NULL;
    result->global_count = 0;
}

void value_ssa_interp_result_free(ValueSsaInterpResult *result) {
    if (!result) {
        return;
    }

    free(result->global_values);
    result->global_values = NULL;
    result->global_count = 0;
    result->return_value = 0;
}

static const ValueSsaFunction *value_ssa_interp_find_function(const ValueSsaProgram *program, const char *name) {
    size_t function_index;

    if (!program || !name) {
        return NULL;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];

        if (function->name && strcmp(function->name, name) == 0) {
            return function;
        }
    }

    return NULL;
}

static long long value_ssa_interp_normalize_sysy_int_value(long long value) {
    uint32_t bits = (uint32_t)value;
    return (long long)(int32_t)bits;
}

static int value_ssa_interp_eval_value(const ValueSsaInterpFrame *frame,
    ValueSsaValueRef value,
    long long *out_value,
    ValueSsaInterpError *error) {
    if (!frame || !out_value) {
        value_ssa_interp_set_error(error, 0, 0, "VALUE-SSA-INTERP-001: invalid value evaluation contract");
        return 0;
    }

    switch (value.kind) {
    case VALUE_SSA_VALUE_IMMEDIATE:
        *out_value = value_ssa_interp_normalize_sysy_int_value(value.immediate);
        return 1;
    case VALUE_SSA_VALUE_ID:
        if (value.value_id >= frame->function->next_value_id || !frame->has_ssa_value[value.value_id]) {
            value_ssa_interp_set_error(error,
                0,
                0,
                "VALUE-SSA-INTERP-002: ssa.%zu is not available in function '%s'",
                value.value_id,
                frame->function->name ? frame->function->name : "<anon>");
            return 0;
        }

        *out_value = frame->ssa_values[value.value_id];
        return 1;
    default:
        value_ssa_interp_set_error(error, 0, 0, "VALUE-SSA-INTERP-003: unknown value kind %d", (int)value.kind);
        return 0;
    }
}

static int value_ssa_interp_eval_binary(ValueSsaBinaryOp op,
    long long lhs,
    long long rhs,
    long long *out_value,
    ValueSsaInterpError *error) {
    int32_t lhs32;
    int32_t rhs32;

    if (!out_value) {
        value_ssa_interp_set_error(error, 0, 0, "VALUE-SSA-INTERP-004: invalid binary evaluation contract");
        return 0;
    }

    lhs32 = (int32_t)value_ssa_interp_normalize_sysy_int_value(lhs);
    rhs32 = (int32_t)value_ssa_interp_normalize_sysy_int_value(rhs);

    switch (op) {
    case VALUE_SSA_BINARY_ADD:
        *out_value = value_ssa_interp_normalize_sysy_int_value((long long)lhs32 + (long long)rhs32);
        return 1;
    case VALUE_SSA_BINARY_SUB:
        *out_value = value_ssa_interp_normalize_sysy_int_value((long long)lhs32 - (long long)rhs32);
        return 1;
    case VALUE_SSA_BINARY_MUL:
        *out_value = value_ssa_interp_normalize_sysy_int_value((long long)lhs32 * (long long)rhs32);
        return 1;
    case VALUE_SSA_BINARY_DIV:
        if (rhs32 == 0 || (lhs32 == INT32_MIN && rhs32 == -1)) {
            value_ssa_interp_set_error(error,
                0,
                0,
                "VALUE-SSA-INTERP-005: invalid div operands %lld and %lld",
                lhs,
                rhs);
            return 0;
        }
        *out_value = value_ssa_interp_normalize_sysy_int_value((long long)(lhs32 / rhs32));
        return 1;
    case VALUE_SSA_BINARY_MOD:
        if (rhs32 == 0 || (lhs32 == INT32_MIN && rhs32 == -1)) {
            value_ssa_interp_set_error(error,
                0,
                0,
                "VALUE-SSA-INTERP-006: invalid mod operands %lld and %lld",
                lhs,
                rhs);
            return 0;
        }
        *out_value = value_ssa_interp_normalize_sysy_int_value((long long)(lhs32 % rhs32));
        return 1;
    case VALUE_SSA_BINARY_BIT_AND:
        *out_value = value_ssa_interp_normalize_sysy_int_value((long long)(lhs32 & rhs32));
        return 1;
    case VALUE_SSA_BINARY_BIT_XOR:
        *out_value = value_ssa_interp_normalize_sysy_int_value((long long)(lhs32 ^ rhs32));
        return 1;
    case VALUE_SSA_BINARY_BIT_OR:
        *out_value = value_ssa_interp_normalize_sysy_int_value((long long)(lhs32 | rhs32));
        return 1;
    case VALUE_SSA_BINARY_SHIFT_LEFT:
    case VALUE_SSA_BINARY_SHIFT_RIGHT: {
        uint32_t shifted;
        size_t bit_width = sizeof(uint32_t) * CHAR_BIT;

        if (rhs32 < 0 || (uint32_t)rhs32 >= bit_width ||
            (op == VALUE_SSA_BINARY_SHIFT_LEFT && lhs32 < 0)) {
            value_ssa_interp_set_error(error,
                0,
                0,
                "VALUE-SSA-INTERP-007: invalid shift operands %lld and %lld",
                lhs,
                rhs);
            return 0;
        }

        if (op == VALUE_SSA_BINARY_SHIFT_LEFT) {
            shifted = ((uint32_t)lhs32 << (uint32_t)rhs32);
            *out_value = value_ssa_interp_normalize_sysy_int_value((long long)shifted);
        } else {
            *out_value = value_ssa_interp_normalize_sysy_int_value((long long)(lhs32 >> rhs32));
        }
        return 1;
    }
    case VALUE_SSA_BINARY_EQ:
        *out_value = lhs32 == rhs32;
        return 1;
    case VALUE_SSA_BINARY_NE:
        *out_value = lhs32 != rhs32;
        return 1;
    case VALUE_SSA_BINARY_LT:
        *out_value = lhs32 < rhs32;
        return 1;
    case VALUE_SSA_BINARY_LE:
        *out_value = lhs32 <= rhs32;
        return 1;
    case VALUE_SSA_BINARY_GT:
        *out_value = lhs32 > rhs32;
        return 1;
    case VALUE_SSA_BINARY_GE:
        *out_value = lhs32 >= rhs32;
        return 1;
    default:
        value_ssa_interp_set_error(error, 0, 0, "VALUE-SSA-INTERP-009: unknown binary op %d", (int)op);
        return 0;
    }
}

#endif
