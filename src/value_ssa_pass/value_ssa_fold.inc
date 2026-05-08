#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

typedef enum {
    VALUE_SSA_FOLD_RESOLVE_UNVISITED = 0,
    VALUE_SSA_FOLD_RESOLVE_VISITING,
    VALUE_SSA_FOLD_RESOLVE_DONE,
} ValueSsaFoldResolveState;

static ValueSsaInstruction *value_ssa_fold_lookup_instruction_definition(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    size_t value_id) {
    size_t block_id;
    size_t instruction_index;

    if (!function || !analysis || value_id >= analysis->value_count || !analysis->has_def[value_id]) {
        return NULL;
    }

    instruction_index = analysis->def_instruction_indices[value_id];
    if (instruction_index == (size_t)-1) {
        return NULL;
    }
    block_id = analysis->def_block_ids[value_id];
    if (block_id >= function->block_count || instruction_index >= function->blocks[block_id].instruction_count) {
        return NULL;
    }

    return &function->blocks[block_id].instructions[instruction_index];
}

static int value_ssa_fold_try_resolve_immediate(ValueSsaValueRef value,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaFoldResolveState *states,
    unsigned char *has_values,
    long long *resolved_values,
    long long *out_value) {
    ValueSsaInstruction *instruction;
    long long resolved_value = 0;

    if (!out_value) {
        return 0;
    }
    if (value.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        *out_value = value.immediate;
        return 1;
    }
    if (value.kind != VALUE_SSA_VALUE_ID ||
        !function || !analysis || !states || !has_values || !resolved_values ||
        value.value_id >= analysis->value_count) {
        return 0;
    }

    if (states[value.value_id] == VALUE_SSA_FOLD_RESOLVE_DONE) {
        if (!has_values[value.value_id]) {
            return 0;
        }
        *out_value = resolved_values[value.value_id];
        return 1;
    }
    if (states[value.value_id] == VALUE_SSA_FOLD_RESOLVE_VISITING) {
        return 0;
    }

    states[value.value_id] = VALUE_SSA_FOLD_RESOLVE_VISITING;
    instruction = value_ssa_fold_lookup_instruction_definition(function, analysis, value.value_id);
    if (instruction &&
        instruction->kind == VALUE_SSA_INSTR_MOV &&
        value_ssa_fold_try_resolve_immediate(
            instruction->as.mov_value,
            function,
            analysis,
            states,
            has_values,
            resolved_values,
            &resolved_value)) {
        has_values[value.value_id] = 1u;
        resolved_values[value.value_id] = resolved_value;
    }
    states[value.value_id] = VALUE_SSA_FOLD_RESOLVE_DONE;

    if (!has_values[value.value_id]) {
        return 0;
    }
    *out_value = resolved_values[value.value_id];
    return 1;
}

static int value_ssa_try_fold_binary_immediates(ValueSsaBinaryOp op,
    long long lhs,
    long long rhs,
    long long *out_value) {
    int32_t lhs32;
    int32_t rhs32;
    long long result = 0;

    if (!out_value) {
        return 0;
    }

    lhs32 = (int32_t)value_ssa_normalize_sysy_int_value(lhs);
    rhs32 = (int32_t)value_ssa_normalize_sysy_int_value(rhs);

    switch (op) {
    case VALUE_SSA_BINARY_ADD:
        result = value_ssa_normalize_sysy_int_value((long long)lhs32 + (long long)rhs32);
        break;
    case VALUE_SSA_BINARY_SUB:
        result = value_ssa_normalize_sysy_int_value((long long)lhs32 - (long long)rhs32);
        break;
    case VALUE_SSA_BINARY_MUL:
        result = value_ssa_normalize_sysy_int_value((long long)lhs32 * (long long)rhs32);
        break;
    case VALUE_SSA_BINARY_BIT_AND:
        result = value_ssa_normalize_sysy_int_value((long long)(lhs32 & rhs32));
        break;
    case VALUE_SSA_BINARY_BIT_XOR:
        result = value_ssa_normalize_sysy_int_value((long long)(lhs32 ^ rhs32));
        break;
    case VALUE_SSA_BINARY_BIT_OR:
        result = value_ssa_normalize_sysy_int_value((long long)(lhs32 | rhs32));
        break;
    case VALUE_SSA_BINARY_EQ:
        result = lhs32 == rhs32 ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_NE:
        result = lhs32 != rhs32 ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_LT:
        result = lhs32 < rhs32 ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_LE:
        result = lhs32 <= rhs32 ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_GT:
        result = lhs32 > rhs32 ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_GE:
        result = lhs32 >= rhs32 ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_DIV:
    case VALUE_SSA_BINARY_MOD:
    case VALUE_SSA_BINARY_SHIFT_LEFT:
    case VALUE_SSA_BINARY_SHIFT_RIGHT:
    default:
        return 0;
    }

    *out_value = result;
    return 1;
}

int value_ssa_fold_constants(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-207: invalid constant-fold contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis analysis;
        ValueSsaFoldResolveState *states = NULL;
        unsigned char *has_values = NULL;
        long long *resolved_values = NULL;
        size_t block_index;

        if (!function->has_body) {
            continue;
        }

        value_ssa_def_use_analysis_init(&analysis);
        if (function->next_value_id > 0u) {
            states = (ValueSsaFoldResolveState *)calloc(function->next_value_id, sizeof(ValueSsaFoldResolveState));
            has_values = (unsigned char *)calloc(function->next_value_id, sizeof(unsigned char));
            resolved_values = (long long *)calloc(function->next_value_id, sizeof(long long));
        }
        if ((function->next_value_id > 0u && (!states || !has_values || !resolved_values)) ||
            !value_ssa_compute_def_use_analysis(function, &analysis, error)) {
            value_ssa_def_use_analysis_free(&analysis);
            free(states);
            free(has_values);
            free(resolved_values);
            if (!error || !error->message[0]) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-207: out of memory preparing constant-fold analysis");
            }
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                long long lhs_immediate;
                long long rhs_immediate;
                long long folded_value;

                if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
                    !value_ssa_fold_try_resolve_immediate(
                        instruction->as.binary.lhs,
                        function,
                        &analysis,
                        states,
                        has_values,
                        resolved_values,
                        &lhs_immediate) ||
                    !value_ssa_fold_try_resolve_immediate(
                        instruction->as.binary.rhs,
                        function,
                        &analysis,
                        states,
                        has_values,
                        resolved_values,
                        &rhs_immediate) ||
                    !value_ssa_try_fold_binary_immediates(instruction->as.binary.op,
                        lhs_immediate,
                        rhs_immediate,
                        &folded_value)) {
                    continue;
                }

                instruction->kind = VALUE_SSA_INSTR_MOV;
                instruction->as.mov_value = value_ssa_value_immediate(folded_value);
                has_values[instruction->result.value_id] = 1u;
                resolved_values[instruction->result.value_id] = folded_value;
            }
        }

        value_ssa_def_use_analysis_free(&analysis);
        free(states);
        free(has_values);
        free(resolved_values);
    }

    return value_ssa_verify_program(program, error);
}

#endif
