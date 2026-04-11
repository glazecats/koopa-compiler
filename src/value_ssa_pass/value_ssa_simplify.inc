#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

typedef enum {
    VALUE_SSA_RESOLVE_UNVISITED = 0,
    VALUE_SSA_RESOLVE_VISITING,
    VALUE_SSA_RESOLVE_DONE,
} ValueSsaResolveState;

static ValueSsaPhi *value_ssa_simplify_lookup_phi_definition(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    size_t value_id) {
    size_t block_id;
    size_t phi_index;

    if (!function || !analysis || value_id >= analysis->value_count || !analysis->has_def[value_id]) {
        return NULL;
    }

    phi_index = analysis->def_phi_indices[value_id];
    if (phi_index == (size_t)-1) {
        return NULL;
    }
    block_id = analysis->def_block_ids[value_id];
    if (block_id >= function->block_count || phi_index >= function->blocks[block_id].phi_count) {
        return NULL;
    }

    return &function->blocks[block_id].phis[phi_index];
}

static ValueSsaInstruction *value_ssa_simplify_lookup_instruction_definition(ValueSsaFunction *function,
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

static ValueSsaValueRef value_ssa_simplify_resolve_value_ref(ValueSsaValueRef value,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaResolveState *resolve_states,
    ValueSsaValueRef *resolved_values) {
    size_t input_index;

    if (value.kind != VALUE_SSA_VALUE_ID) {
        return value;
    }

    if (resolve_states[value.value_id] == VALUE_SSA_RESOLVE_DONE) {
        return resolved_values[value.value_id];
    }
    if (resolve_states[value.value_id] == VALUE_SSA_RESOLVE_VISITING) {
        return value;
    }

    resolve_states[value.value_id] = VALUE_SSA_RESOLVE_VISITING;
    resolved_values[value.value_id] = value;

    if (!analysis->has_def[value.value_id]) {
        resolve_states[value.value_id] = VALUE_SSA_RESOLVE_DONE;
        return resolved_values[value.value_id];
    }

    if (analysis->def_instruction_indices[value.value_id] != (size_t)-1) {
        {
            ValueSsaInstruction *instruction =
                value_ssa_simplify_lookup_instruction_definition(function, analysis, value.value_id);

            if (instruction && instruction->kind == VALUE_SSA_INSTR_MOV) {
                resolved_values[value.value_id] = value_ssa_simplify_resolve_value_ref(
                    instruction->as.mov_value,
                    function,
                    analysis,
                    resolve_states,
                    resolved_values);
            }
        }
    } else if (analysis->def_phi_indices[value.value_id] != (size_t)-1) {
        {
            ValueSsaPhi *phi = value_ssa_simplify_lookup_phi_definition(function, analysis, value.value_id);

            if (phi && phi->input_count > 0 && phi->inputs) {
                ValueSsaValueRef candidate = value_ssa_simplify_resolve_value_ref(
                    phi->inputs[0].value,
                    function,
                    analysis,
                    resolve_states,
                    resolved_values);
                int all_same = 1;

                for (input_index = 1; input_index < phi->input_count; ++input_index) {
                    ValueSsaValueRef resolved_input = value_ssa_simplify_resolve_value_ref(
                        phi->inputs[input_index].value,
                        function,
                        analysis,
                        resolve_states,
                        resolved_values);

                    if (!value_ssa_value_refs_equal(candidate, resolved_input)) {
                        all_same = 0;
                        break;
                    }
                }

                if (all_same) {
                    resolved_values[value.value_id] = candidate;
                }
            }
        }
    }

    resolve_states[value.value_id] = VALUE_SSA_RESOLVE_DONE;
    return resolved_values[value.value_id];
}

static int value_ssa_simplify_rewrite_use_site(ValueSsaFunction *function,
    const ValueSsaUseSite *site,
    ValueSsaValueRef replacement,
    ValueSsaError *error) {
    ValueSsaBasicBlock *block;

    if (!function || !site || site->block_id >= function->block_count) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-200: invalid simplify use-site contract");
        return 0;
    }

    block = &function->blocks[site->block_id];
    switch (site->kind) {
    case VALUE_SSA_USE_PHI:
        if (site->phi_index >= block->phi_count ||
            site->operand_index >= block->phis[site->phi_index].input_count) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-201: invalid phi use-site during simplify");
            return 0;
        }
        block->phis[site->phi_index].inputs[site->operand_index].value = replacement;
        return 1;
    case VALUE_SSA_USE_INSTR:
        if (site->instruction_index >= block->instruction_count) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-202: invalid instruction use-site during simplify");
            return 0;
        }
        switch (site->role) {
        case VALUE_SSA_USE_ROLE_MOV_VALUE:
            block->instructions[site->instruction_index].as.mov_value = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_BINARY_LHS:
            block->instructions[site->instruction_index].as.binary.lhs = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_BINARY_RHS:
            block->instructions[site->instruction_index].as.binary.rhs = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_CALL_ARG:
            if (site->operand_index >= block->instructions[site->instruction_index].as.call.arg_count) {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-203: invalid call-arg use-site during simplify");
                return 0;
            }
            block->instructions[site->instruction_index].as.call.args[site->operand_index] = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_STORE_VALUE:
            block->instructions[site->instruction_index].as.store.value = replacement;
            return 1;
        default:
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-204: invalid instruction use-role during simplify");
            return 0;
        }
    case VALUE_SSA_USE_TERM:
        switch (site->role) {
        case VALUE_SSA_USE_ROLE_RETURN_VALUE:
            block->terminator.as.return_value = replacement;
            return 1;
        case VALUE_SSA_USE_ROLE_BRANCH_CONDITION:
            block->terminator.as.branch.condition = replacement;
            return 1;
        default:
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-205: invalid terminator use-role during simplify");
            return 0;
        }
    default:
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-206: unknown use-site kind during simplify");
        return 0;
    }
}

int value_ssa_simplify_trivial_values(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-122: invalid simplify contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis def_use;
        ValueSsaResolveState *resolve_states = NULL;
        ValueSsaValueRef *resolved_values = NULL;
        size_t value_id;

        if (!function->has_body || function->next_value_id == 0) {
            continue;
        }

        value_ssa_def_use_analysis_init(&def_use);
        resolve_states = (ValueSsaResolveState *)calloc(function->next_value_id, sizeof(ValueSsaResolveState));
        resolved_values = (ValueSsaValueRef *)calloc(function->next_value_id, sizeof(ValueSsaValueRef));
        if (!resolve_states || !resolved_values) {
            free(resolve_states);
            free(resolved_values);
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-123: out of memory while simplifying function '%s'",
                function->name ? function->name : "<anon>");
            return 0;
        }
        if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            free(resolve_states);
            free(resolved_values);
            return 0;
        }

        for (value_id = 0; value_id < function->next_value_id; ++value_id) {
            ValueSsaValueRef seed = value_ssa_value_id(value_id);

            resolved_values[value_id] = seed;
        }
        for (value_id = 0; value_id < function->next_value_id; ++value_id) {
            (void)value_ssa_simplify_resolve_value_ref(value_ssa_value_id(value_id),
                function,
                &def_use,
                resolve_states,
                resolved_values);
        }

        for (value_id = 0; value_id < function->next_value_id; ++value_id) {
            ValueSsaValueRef replacement = resolved_values[value_id];
            size_t use_index;

            if (replacement.kind == VALUE_SSA_VALUE_ID && replacement.value_id == value_id) {
                continue;
            }

            for (use_index = 0; use_index < def_use.use_counts[value_id]; ++use_index) {
                const ValueSsaUseSite *site = &def_use.use_sites[def_use.use_offsets[value_id] + use_index];

                if (!value_ssa_simplify_rewrite_use_site(function, site, replacement, error)) {
                    value_ssa_def_use_analysis_free(&def_use);
                    free(resolve_states);
                    free(resolved_values);
                    return 0;
                }
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        free(resolve_states);
        free(resolved_values);
    }

    return 1;
}

#endif
