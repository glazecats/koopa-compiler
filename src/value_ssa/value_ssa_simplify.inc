#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa.c"
#else

typedef enum {
    VALUE_SSA_DEF_NONE = 0,
    VALUE_SSA_DEF_PHI,
    VALUE_SSA_DEF_INSTRUCTION,
} ValueSsaDefinitionKind;

typedef enum {
    VALUE_SSA_RESOLVE_UNVISITED = 0,
    VALUE_SSA_RESOLVE_VISITING,
    VALUE_SSA_RESOLVE_DONE,
} ValueSsaResolveState;

static ValueSsaValueRef value_ssa_simplify_resolve_value_ref(ValueSsaValueRef value,
    const ValueSsaDefinitionKind *definition_kinds,
    ValueSsaPhi *const *phi_definitions,
    ValueSsaInstruction *const *instruction_definitions,
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

    switch (definition_kinds[value.value_id]) {
    case VALUE_SSA_DEF_INSTRUCTION:
        {
            ValueSsaInstruction *instruction = instruction_definitions[value.value_id];

            if (instruction && instruction->kind == VALUE_SSA_INSTR_MOV) {
                resolved_values[value.value_id] = value_ssa_simplify_resolve_value_ref(
                    instruction->as.mov_value,
                    definition_kinds,
                    phi_definitions,
                    instruction_definitions,
                    resolve_states,
                    resolved_values);
            }
        }
        break;
    case VALUE_SSA_DEF_PHI:
        {
            ValueSsaPhi *phi = phi_definitions[value.value_id];

            if (phi && phi->input_count > 0 && phi->inputs) {
                ValueSsaValueRef candidate = value_ssa_simplify_resolve_value_ref(
                    phi->inputs[0].value,
                    definition_kinds,
                    phi_definitions,
                    instruction_definitions,
                    resolve_states,
                    resolved_values);
                int all_same = 1;

                for (input_index = 1; input_index < phi->input_count; ++input_index) {
                    ValueSsaValueRef resolved_input = value_ssa_simplify_resolve_value_ref(
                        phi->inputs[input_index].value,
                        definition_kinds,
                        phi_definitions,
                        instruction_definitions,
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
        break;
    case VALUE_SSA_DEF_NONE:
    default:
        break;
    }

    resolve_states[value.value_id] = VALUE_SSA_RESOLVE_DONE;
    return resolved_values[value.value_id];
}

int value_ssa_simplify_trivial_values(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-122: invalid simplify contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefinitionKind *definition_kinds = NULL;
        ValueSsaPhi **phi_definitions = NULL;
        ValueSsaInstruction **instruction_definitions = NULL;
        ValueSsaResolveState *resolve_states = NULL;
        ValueSsaValueRef *resolved_values = NULL;
        size_t block_index;
        size_t value_id;

        if (!function->has_body || function->next_value_id == 0) {
            continue;
        }

        definition_kinds = (ValueSsaDefinitionKind *)calloc(function->next_value_id, sizeof(ValueSsaDefinitionKind));
        phi_definitions = (ValueSsaPhi **)calloc(function->next_value_id, sizeof(ValueSsaPhi *));
        instruction_definitions =
            (ValueSsaInstruction **)calloc(function->next_value_id, sizeof(ValueSsaInstruction *));
        resolve_states = (ValueSsaResolveState *)calloc(function->next_value_id, sizeof(ValueSsaResolveState));
        resolved_values = (ValueSsaValueRef *)calloc(function->next_value_id, sizeof(ValueSsaValueRef));
        if (!definition_kinds || !phi_definitions || !instruction_definitions || !resolve_states || !resolved_values) {
            free(definition_kinds);
            free(phi_definitions);
            free(instruction_definitions);
            free(resolve_states);
            free(resolved_values);
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-123: out of memory while simplifying function '%s'",
                function->name ? function->name : "<anon>");
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t phi_index;
            size_t instruction_index;

            for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                ValueSsaPhi *phi = &block->phis[phi_index];

                definition_kinds[phi->result_id] = VALUE_SSA_DEF_PHI;
                phi_definitions[phi->result_id] = phi;
            }
            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->has_result && instruction->result.kind == VALUE_SSA_VALUE_ID) {
                    definition_kinds[instruction->result.value_id] = VALUE_SSA_DEF_INSTRUCTION;
                    instruction_definitions[instruction->result.value_id] = instruction;
                }
            }
        }

        for (value_id = 0; value_id < function->next_value_id; ++value_id) {
            ValueSsaValueRef seed = value_ssa_value_id(value_id);

            resolved_values[value_id] = seed;
        }
        for (value_id = 0; value_id < function->next_value_id; ++value_id) {
            (void)value_ssa_simplify_resolve_value_ref(value_ssa_value_id(value_id),
                definition_kinds,
                phi_definitions,
                instruction_definitions,
                resolve_states,
                resolved_values);
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t phi_index;
            size_t instruction_index;

            for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                ValueSsaPhi *phi = &block->phis[phi_index];
                size_t input_index;

                for (input_index = 0; input_index < phi->input_count; ++input_index) {
                    phi->inputs[input_index].value = value_ssa_simplify_resolve_value_ref(
                        phi->inputs[input_index].value,
                        definition_kinds,
                        phi_definitions,
                        instruction_definitions,
                        resolve_states,
                        resolved_values);
                }
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                size_t arg_index;

                switch (instruction->kind) {
                case VALUE_SSA_INSTR_MOV:
                    instruction->as.mov_value = value_ssa_simplify_resolve_value_ref(
                        instruction->as.mov_value,
                        definition_kinds,
                        phi_definitions,
                        instruction_definitions,
                        resolve_states,
                        resolved_values);
                    break;
                case VALUE_SSA_INSTR_BINARY:
                    instruction->as.binary.lhs = value_ssa_simplify_resolve_value_ref(
                        instruction->as.binary.lhs,
                        definition_kinds,
                        phi_definitions,
                        instruction_definitions,
                        resolve_states,
                        resolved_values);
                    instruction->as.binary.rhs = value_ssa_simplify_resolve_value_ref(
                        instruction->as.binary.rhs,
                        definition_kinds,
                        phi_definitions,
                        instruction_definitions,
                        resolve_states,
                        resolved_values);
                    break;
                case VALUE_SSA_INSTR_CALL:
                    for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                        instruction->as.call.args[arg_index] = value_ssa_simplify_resolve_value_ref(
                            instruction->as.call.args[arg_index],
                            definition_kinds,
                            phi_definitions,
                            instruction_definitions,
                            resolve_states,
                            resolved_values);
                    }
                    break;
                case VALUE_SSA_INSTR_STORE_LOCAL:
                case VALUE_SSA_INSTR_STORE_GLOBAL:
                    instruction->as.store.value = value_ssa_simplify_resolve_value_ref(
                        instruction->as.store.value,
                        definition_kinds,
                        phi_definitions,
                        instruction_definitions,
                        resolve_states,
                        resolved_values);
                    break;
                case VALUE_SSA_INSTR_LOAD_LOCAL:
                case VALUE_SSA_INSTR_LOAD_GLOBAL:
                    break;
                default:
                    break;
                }
            }

            switch (block->terminator.kind) {
            case VALUE_SSA_TERM_RETURN:
                block->terminator.as.return_value = value_ssa_simplify_resolve_value_ref(
                    block->terminator.as.return_value,
                    definition_kinds,
                    phi_definitions,
                    instruction_definitions,
                    resolve_states,
                    resolved_values);
                break;
            case VALUE_SSA_TERM_BRANCH:
                block->terminator.as.branch.condition = value_ssa_simplify_resolve_value_ref(
                    block->terminator.as.branch.condition,
                    definition_kinds,
                    phi_definitions,
                    instruction_definitions,
                    resolve_states,
                    resolved_values);
                break;
            case VALUE_SSA_TERM_JUMP:
            default:
                break;
            }
        }

        free(definition_kinds);
        free(phi_definitions);
        free(instruction_definitions);
        free(resolve_states);
        free(resolved_values);
    }

    return 1;
}

#endif
