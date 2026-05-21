#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "../value_ssa_pass/value_ssa_pass.c"
#else

static int value_ssa_perf_binary_is_add_one_of_value(const ValueSsaInstruction *instruction,
    size_t value_id) {
    if (!instruction || instruction->kind != VALUE_SSA_INSTR_BINARY ||
        instruction->as.binary.op != VALUE_SSA_BINARY_ADD) {
        return 0;
    }

    return (instruction->as.binary.lhs.kind == VALUE_SSA_VALUE_ID &&
            instruction->as.binary.lhs.value_id == value_id &&
            instruction->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
            instruction->as.binary.rhs.immediate == 1) ||
        (instruction->as.binary.rhs.kind == VALUE_SSA_VALUE_ID &&
            instruction->as.binary.rhs.value_id == value_id &&
            instruction->as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
            instruction->as.binary.lhs.immediate == 1);
}

static int value_ssa_perf_mark_non_negative_zero_based_inductions(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    unsigned char *non_negative_values) {
    size_t block_index;

    if (!function || !analysis || !non_negative_values) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t phi_index;

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            const ValueSsaPhi *phi = &block->phis[phi_index];
            size_t input_index;
            size_t increment_value_id = (size_t)-1;
            int saw_zero_seed = 0;

            if (phi->result_id >= function->next_value_id || phi->input_count != 2u) {
                continue;
            }

            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                if (phi->inputs[input_index].value.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                    phi->inputs[input_index].value.immediate == 0) {
                    saw_zero_seed = 1;
                } else if (phi->inputs[input_index].value.kind == VALUE_SSA_VALUE_ID) {
                    increment_value_id = phi->inputs[input_index].value.value_id;
                }
            }

            if (!saw_zero_seed || increment_value_id == (size_t)-1 ||
                increment_value_id >= analysis->value_count || !analysis->has_def[increment_value_id]) {
                continue;
            }

            {
                ValueSsaInstruction *increment_definition = value_ssa_pass_lookup_instruction_definition(
                    function, analysis, increment_value_id);

                if (!increment_definition ||
                    !value_ssa_perf_binary_is_add_one_of_value(increment_definition, phi->result_id)) {
                    continue;
                }
            }

            non_negative_values[phi->result_id] = 1u;
            non_negative_values[increment_value_id] = 1u;
        }
    }

    return 1;
}

static int value_ssa_perf_strength_reduce_non_negative_induction_divmods(ValueSsaProgram *program,
    ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-313: invalid perf induction div/mod reduction contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis def_use;
        unsigned char *non_negative_values = NULL;
        size_t block_index;
        int changed_function = 0;

        if (!function->has_body || function->next_value_id == 0u) {
            continue;
        }

        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }

        non_negative_values = (unsigned char *)calloc(function->next_value_id, sizeof(unsigned char));
        if (!non_negative_values) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-314: out of memory preparing perf induction div/mod reduction");
            return 0;
        }
        if (!value_ssa_perf_mark_non_negative_zero_based_inductions(function, &def_use, non_negative_values)) {
            free(non_negative_values);
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-315: failed collecting non-negative induction values");
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                long long divisor;
                unsigned shift = 0u;
                unsigned long long bits;

                if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
                    instruction->as.binary.lhs.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.binary.lhs.value_id >= function->next_value_id ||
                    !non_negative_values[instruction->as.binary.lhs.value_id] ||
                    instruction->as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE) {
                    continue;
                }

                if (instruction->as.binary.op != VALUE_SSA_BINARY_DIV &&
                    instruction->as.binary.op != VALUE_SSA_BINARY_MOD) {
                    continue;
                }

                divisor = instruction->as.binary.rhs.immediate;
                if (divisor <= 0) {
                    continue;
                }

                bits = (unsigned long long)divisor;
                if ((bits & (bits - 1u)) != 0u) {
                    continue;
                }
                while ((1ull << shift) != bits) {
                    ++shift;
                    if (shift >= 31u) {
                        break;
                    }
                }
                if ((1ull << shift) != bits) {
                    continue;
                }

                if (instruction->as.binary.op == VALUE_SSA_BINARY_DIV) {
                    instruction->as.binary.op = VALUE_SSA_BINARY_SHIFT_RIGHT;
                    instruction->as.binary.rhs = value_ssa_value_immediate((long long)shift);
                } else {
                    instruction->as.binary.op = VALUE_SSA_BINARY_BIT_AND;
                    instruction->as.binary.rhs = value_ssa_value_immediate((long long)(bits - 1u));
                }
                changed_function = 1;
            }
        }

        free(non_negative_values);
        value_ssa_def_use_analysis_free(&def_use);

        if (changed_function &&
            (!value_ssa_simplify_trivial_values(program, error) ||
                !value_ssa_eliminate_dead_value_defs(program, error))) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

typedef struct {
    size_t header_id;
    size_t body_id;
    size_t preheader_id;
    size_t exit_id;
} ValueSsaPerfSimpleLoop;

typedef enum {
    VALUE_SSA_PERF_ADDRESS_ROOT_UNKNOWN = 0,
    VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT,
    VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT,
    VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER,
} ValueSsaPerfAddressRootKind;

typedef struct {
    ValueSsaPerfAddressRootKind kind;
    size_t id;
} ValueSsaPerfAddressRoot;

static int value_ssa_perf_extract_global_arg_root(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    size_t *out_global_id,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!function || !analysis || !out_global_id || depth > 8u || value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        if (definition->as.addr_slot.kind != VALUE_SSA_SLOT_GLOBAL) {
            return 0;
        }
        *out_global_id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_perf_extract_global_arg_root(
            function, analysis, definition->as.mov_value, out_global_id, depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_perf_collect_non_alias_parameter_pointer_pairs(const ValueSsaProgram *program,
    const ValueSsaFunction *callee,
    unsigned char *pair_matrix) {
    size_t param_count;
    size_t lhs_index;
    size_t rhs_index;
    size_t caller_index;
    int saw_callsite = 0;

    if (!program || !callee || !pair_matrix) {
        return 0;
    }

    param_count = callee->parameter_count;
    for (lhs_index = 0; lhs_index < param_count; ++lhs_index) {
        for (rhs_index = 0; rhs_index < param_count; ++rhs_index) {
            pair_matrix[lhs_index * param_count + rhs_index] =
                (unsigned char)(lhs_index != rhs_index &&
                    lhs_index < callee->local_count &&
                    rhs_index < callee->local_count &&
                    callee->locals[lhs_index].is_parameter &&
                    callee->locals[rhs_index].is_parameter &&
                    callee->locals[lhs_index].array_rank > 0u &&
                    callee->locals[rhs_index].array_rank > 0u);
        }
    }

    for (caller_index = 0; caller_index < program->function_count; ++caller_index) {
        const ValueSsaFunction *caller = &program->functions[caller_index];
        ValueSsaDefUseAnalysis def_use;
        size_t block_index;

        if (!caller->has_body) {
            continue;
        }

        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_def_use_analysis(caller, &def_use, NULL)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }

        for (block_index = 0; block_index < caller->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &caller->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                size_t lhs_global_id;
                size_t rhs_global_id;

                if (instruction->kind != VALUE_SSA_INSTR_CALL ||
                    !instruction->as.call.callee_name ||
                    !callee->name ||
                    strcmp(instruction->as.call.callee_name, callee->name) != 0 ||
                    instruction->as.call.arg_count != param_count) {
                    continue;
                }

                saw_callsite = 1;
                for (lhs_index = 0; lhs_index < param_count; ++lhs_index) {
                    for (rhs_index = lhs_index + 1u; rhs_index < param_count; ++rhs_index) {
                        if (!pair_matrix[lhs_index * param_count + rhs_index]) {
                            continue;
                        }

                        if (!value_ssa_perf_extract_global_arg_root(
                                caller, &def_use, instruction->as.call.args[lhs_index], &lhs_global_id, 0u) ||
                            !value_ssa_perf_extract_global_arg_root(
                                caller, &def_use, instruction->as.call.args[rhs_index], &rhs_global_id, 0u) ||
                            lhs_global_id == rhs_global_id) {
                            pair_matrix[lhs_index * param_count + rhs_index] = 0u;
                            pair_matrix[rhs_index * param_count + lhs_index] = 0u;
                        }
                    }
                }
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
    }

    if (!saw_callsite) {
        memset(pair_matrix, 0, param_count * param_count * sizeof(unsigned char));
    }

    return 1;
}

static int value_ssa_perf_parameter_pointer_pair_proven_non_alias(const ValueSsaFunction *function,
    const unsigned char *pair_matrix,
    size_t lhs_id,
    size_t rhs_id) {
    size_t param_count;

    if (!function || !pair_matrix || lhs_id == rhs_id) {
        return 0;
    }

    param_count = function->parameter_count;
    if (lhs_id >= param_count || rhs_id >= param_count) {
        return 0;
    }

    return pair_matrix[lhs_id * param_count + rhs_id] != 0u;
}

static int value_ssa_perf_same_pure_address_value(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    unsigned depth) {
    ValueSsaInstruction *lhs_def;
    ValueSsaInstruction *rhs_def;

    if (depth > 8u) {
        return 0;
    }
    if (value_ssa_value_refs_equal(lhs, rhs)) {
        return 1;
    }
    if (!function || !analysis || lhs.kind != VALUE_SSA_VALUE_ID || rhs.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    lhs_def = value_ssa_pass_lookup_instruction_definition(function, analysis, lhs.value_id);
    rhs_def = value_ssa_pass_lookup_instruction_definition(function, analysis, rhs.value_id);
    if (!lhs_def || !rhs_def || lhs_def->kind != rhs_def->kind) {
        return 0;
    }

    switch (lhs_def->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return lhs_def->as.addr_slot.kind == rhs_def->as.addr_slot.kind &&
            lhs_def->as.addr_slot.id == rhs_def->as.addr_slot.id;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        return lhs_def->as.load_slot.kind == rhs_def->as.load_slot.kind &&
            lhs_def->as.load_slot.id == rhs_def->as.load_slot.id;
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        return lhs_def->as.load_slot.kind == rhs_def->as.load_slot.kind &&
            lhs_def->as.load_slot.id == rhs_def->as.load_slot.id;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_perf_same_pure_address_value(
            function, analysis, lhs_def->as.mov_value, rhs_def->as.mov_value, depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        if (lhs_def->as.binary.op != rhs_def->as.binary.op) {
            return 0;
        }
        if (value_ssa_perf_same_pure_address_value(
                function, analysis, lhs_def->as.binary.lhs, rhs_def->as.binary.lhs, depth + 1u) &&
            value_ssa_perf_same_pure_address_value(
                function, analysis, lhs_def->as.binary.rhs, rhs_def->as.binary.rhs, depth + 1u)) {
            return 1;
        }
        return lhs_def->as.binary.op == VALUE_SSA_BINARY_ADD &&
            value_ssa_perf_same_pure_address_value(
                function, analysis, lhs_def->as.binary.lhs, rhs_def->as.binary.rhs, depth + 1u) &&
            value_ssa_perf_same_pure_address_value(
                function, analysis, lhs_def->as.binary.rhs, rhs_def->as.binary.lhs, depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_perf_find_simple_single_body_loop(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t header_id,
    ValueSsaPerfSimpleLoop *out_loop) {
    const ValueSsaBasicBlock *header;
    size_t predecessor_index;
    size_t predecessor_a = (size_t)-1;
    size_t predecessor_b = (size_t)-1;
    size_t body_id = (size_t)-1;
    size_t preheader_id = (size_t)-1;

    if (!function || !analysis || !out_loop || header_id >= function->block_count) {
        return 0;
    }

    header = &function->blocks[header_id];
    if (header->phi_count != 0u ||
        !header->has_terminator ||
        header->terminator.kind != VALUE_SSA_TERM_BRANCH ||
        analysis->predecessor_counts[header_id] != 2u) {
        return 0;
    }

    for (predecessor_index = 0; predecessor_index < function->block_count; ++predecessor_index) {
        if (!analysis->predecessor_matrix[predecessor_index * function->block_count + header_id]) {
            continue;
        }
        if (predecessor_a == (size_t)-1) {
            predecessor_a = predecessor_index;
        } else if (predecessor_b == (size_t)-1) {
            predecessor_b = predecessor_index;
        } else {
            return 0;
        }
    }

    if (predecessor_a == (size_t)-1 || predecessor_b == (size_t)-1) {
        return 0;
    }

    if (function->blocks[predecessor_a].has_terminator &&
        function->blocks[predecessor_a].terminator.kind == VALUE_SSA_TERM_JUMP &&
        function->blocks[predecessor_a].terminator.as.jump_target == header_id &&
        analysis->dominates[header_id * function->block_count + predecessor_a]) {
        body_id = predecessor_a;
        preheader_id = predecessor_b;
    } else if (function->blocks[predecessor_b].has_terminator &&
               function->blocks[predecessor_b].terminator.kind == VALUE_SSA_TERM_JUMP &&
               function->blocks[predecessor_b].terminator.as.jump_target == header_id &&
               analysis->dominates[header_id * function->block_count + predecessor_b]) {
        body_id = predecessor_b;
        preheader_id = predecessor_a;
    } else {
        return 0;
    }

    if (body_id >= function->block_count || preheader_id >= function->block_count) {
        return 0;
    }

    if (header->terminator.as.branch.then_target == body_id) {
        out_loop->exit_id = header->terminator.as.branch.else_target;
    } else if (header->terminator.as.branch.else_target == body_id) {
        out_loop->exit_id = header->terminator.as.branch.then_target;
    } else {
        return 0;
    }
    if (function->blocks[body_id].phi_count != 0u ||
        function->blocks[preheader_id].phi_count != 0u ||
        !function->blocks[body_id].has_terminator ||
        function->blocks[body_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[body_id].terminator.as.jump_target != header_id ||
        !function->blocks[preheader_id].has_terminator ||
        function->blocks[preheader_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[preheader_id].terminator.as.jump_target != header_id) {
        return 0;
    }

    out_loop->header_id = header_id;
    out_loop->body_id = body_id;
    out_loop->preheader_id = preheader_id;
    return 1;
}

static int value_ssa_perf_find_phi_header_single_body_loop(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t header_id,
    ValueSsaPerfSimpleLoop *out_loop) {
    const ValueSsaBasicBlock *header;
    size_t predecessor_index;
    size_t predecessor_a = (size_t)-1;
    size_t predecessor_b = (size_t)-1;
    size_t body_id = (size_t)-1;
    size_t preheader_id = (size_t)-1;

    if (!function || !analysis || !out_loop || header_id >= function->block_count) {
        return 0;
    }

    header = &function->blocks[header_id];
    if (!header->has_terminator ||
        header->terminator.kind != VALUE_SSA_TERM_BRANCH ||
        analysis->predecessor_counts[header_id] != 2u) {
        return 0;
    }

    for (predecessor_index = 0; predecessor_index < function->block_count; ++predecessor_index) {
        if (!analysis->predecessor_matrix[predecessor_index * function->block_count + header_id]) {
            continue;
        }
        if (predecessor_a == (size_t)-1) {
            predecessor_a = predecessor_index;
        } else if (predecessor_b == (size_t)-1) {
            predecessor_b = predecessor_index;
        } else {
            return 0;
        }
    }

    if (predecessor_a == (size_t)-1 || predecessor_b == (size_t)-1) {
        return 0;
    }

    if (function->blocks[predecessor_a].has_terminator &&
        function->blocks[predecessor_a].terminator.kind == VALUE_SSA_TERM_JUMP &&
        function->blocks[predecessor_a].terminator.as.jump_target == header_id &&
        analysis->dominates[header_id * function->block_count + predecessor_a]) {
        body_id = predecessor_a;
        preheader_id = predecessor_b;
    } else if (function->blocks[predecessor_b].has_terminator &&
               function->blocks[predecessor_b].terminator.kind == VALUE_SSA_TERM_JUMP &&
               function->blocks[predecessor_b].terminator.as.jump_target == header_id &&
               analysis->dominates[header_id * function->block_count + predecessor_b]) {
        body_id = predecessor_b;
        preheader_id = predecessor_a;
    } else {
        return 0;
    }

    if (body_id >= function->block_count || preheader_id >= function->block_count) {
        return 0;
    }

    if (header->terminator.as.branch.then_target == body_id) {
        out_loop->exit_id = header->terminator.as.branch.else_target;
    } else if (header->terminator.as.branch.else_target == body_id) {
        out_loop->exit_id = header->terminator.as.branch.then_target;
    } else {
        return 0;
    }
    if (function->blocks[body_id].phi_count != 0u ||
        function->blocks[preheader_id].phi_count != 0u ||
        !function->blocks[body_id].has_terminator ||
        function->blocks[body_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[body_id].terminator.as.jump_target != header_id ||
        !function->blocks[preheader_id].has_terminator ||
        function->blocks[preheader_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[preheader_id].terminator.as.jump_target != header_id) {
        return 0;
    }

    out_loop->header_id = header_id;
    out_loop->body_id = body_id;
    out_loop->preheader_id = preheader_id;
    return 1;
}

static int value_ssa_perf_extract_address_root(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    ValueSsaPerfAddressRoot *out_root,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!out_root) {
        return 0;
    }

    out_root->kind = VALUE_SSA_PERF_ADDRESS_ROOT_UNKNOWN;
    out_root->id = 0u;
    if (!function || !analysis || depth > 8u || value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
        out_root->kind = VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT;
        out_root->id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        out_root->kind = VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT;
        out_root->id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        if (definition->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            definition->as.load_slot.id < function->local_count &&
            function->locals[definition->as.load_slot.id].is_parameter &&
            function->locals[definition->as.load_slot.id].array_rank > 0u) {
            out_root->kind = VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER;
            out_root->id = definition->as.load_slot.id;
            return 1;
        }
        return 0;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_perf_extract_address_root(
            function, analysis, definition->as.mov_value, out_root, depth + 1u);
    case VALUE_SSA_INSTR_BINARY: {
        ValueSsaPerfAddressRoot lhs_root;
        ValueSsaPerfAddressRoot rhs_root;
        int has_lhs_root;
        int has_rhs_root;

        if (definition->as.binary.op != VALUE_SSA_BINARY_ADD) {
            return 0;
        }

        has_lhs_root = value_ssa_perf_extract_address_root(
            function, analysis, definition->as.binary.lhs, &lhs_root, depth + 1u);
        has_rhs_root = value_ssa_perf_extract_address_root(
            function, analysis, definition->as.binary.rhs, &rhs_root, depth + 1u);
        if (has_lhs_root == has_rhs_root) {
            return 0;
        }
        *out_root = has_lhs_root ? lhs_root : rhs_root;
        return 1;
    }
    default:
        return 0;
    }
}

static int value_ssa_perf_address_roots_proven_non_alias(const ValueSsaFunction *function,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfAddressRoot *lhs,
    const ValueSsaPerfAddressRoot *rhs) {
    if (!lhs || !rhs ||
        lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_UNKNOWN ||
        rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_UNKNOWN) {
        return 0;
    }

    if (lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT &&
        rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if (lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT &&
        rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if ((lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT && rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT) ||
        (lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT && rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }
    if ((lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT && rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER) ||
        (lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER && rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }
    if (lhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER &&
        rhs->kind == VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER) {
        return value_ssa_perf_parameter_pointer_pair_proven_non_alias(
            function,
            parameter_pointer_non_alias_pairs,
            lhs->id,
            rhs->id);
    }

    return 0;
}

static int value_ssa_perf_store_cannot_alias_loaded_address(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *parameter_pointer_non_alias_pairs,
    const ValueSsaPerfAddressRoot *load_root,
    const ValueSsaInstruction *store_instruction) {
    ValueSsaPerfAddressRoot store_root;

    if (!function || !analysis || !load_root || !store_instruction) {
        return 0;
    }

    switch (store_instruction->kind) {
    case VALUE_SSA_INSTR_STORE_LOCAL:
        if (load_root->kind != VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT ||
            store_instruction->as.store.slot.kind != VALUE_SSA_SLOT_LOCAL ||
            store_instruction->as.store.slot.id >= function->local_count) {
            return load_root->kind != VALUE_SSA_PERF_ADDRESS_ROOT_PARAMETER_POINTER;
        }
        if (store_instruction->as.store.slot.id == load_root->id) {
            return 0;
        }
        return function->locals[store_instruction->as.store.slot.id].array_rank == 0u;
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        if (store_instruction->as.store.slot.kind != VALUE_SSA_SLOT_GLOBAL) {
            return 0;
        }
        if (load_root->kind == VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT) {
            return store_instruction->as.store.slot.id != load_root->id;
        }
        return load_root->kind == VALUE_SSA_PERF_ADDRESS_ROOT_LOCAL_SLOT;
    case VALUE_SSA_INSTR_STORE_INDIRECT:
        if (!value_ssa_perf_extract_address_root(
                function, analysis, store_instruction->as.store_indirect.addr, &store_root, 0u)) {
            return 0;
        }
        return value_ssa_perf_address_roots_proven_non_alias(
            function, parameter_pointer_non_alias_pairs, load_root, &store_root);
    default:
        return 1;
    }
}

static int value_ssa_perf_function_has_calls(const ValueSsaFunction *function) {
    size_t block_index;

    if (!function) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            if (block->instructions[instruction_index].kind == VALUE_SSA_INSTR_CALL) {
                return 1;
            }
        }
    }

    return 0;
}

static int value_ssa_perf_function_preserves_global_slot(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    size_t slot_id) {
    ValueSsaPerfAddressRoot load_root;
    size_t block_index;

    if (!function || !analysis) {
        return 0;
    }

    load_root.kind = VALUE_SSA_PERF_ADDRESS_ROOT_GLOBAL_SLOT;
    load_root.id = slot_id;
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_CALL) {
                return 0;
            }
            if (!value_ssa_perf_store_cannot_alias_loaded_address(
                    function, analysis, NULL, &load_root, instruction)) {
                return 0;
            }
        }
    }

    return 1;
}

static int value_ssa_perf_find_simple_branch_body_loop(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t header_id,
    ValueSsaPerfSimpleLoop *out_loop) {
    const ValueSsaBasicBlock *header;
    size_t predecessor_index;
    size_t predecessor_a = (size_t)-1;
    size_t predecessor_b = (size_t)-1;
    size_t body_id = (size_t)-1;
    size_t preheader_id = (size_t)-1;
    size_t candidate_then;
    size_t candidate_else;

    if (!function || !analysis || !out_loop || header_id >= function->block_count) {
        return 0;
    }

    header = &function->blocks[header_id];
    if (header->phi_count != 0u ||
        !header->has_terminator ||
        header->terminator.kind != VALUE_SSA_TERM_BRANCH ||
        analysis->predecessor_counts[header_id] != 2u) {
        return 0;
    }

    for (predecessor_index = 0; predecessor_index < function->block_count; ++predecessor_index) {
        if (!analysis->predecessor_matrix[predecessor_index * function->block_count + header_id]) {
            continue;
        }
        if (predecessor_a == (size_t)-1) {
            predecessor_a = predecessor_index;
        } else if (predecessor_b == (size_t)-1) {
            predecessor_b = predecessor_index;
        } else {
            return 0;
        }
    }

    if (predecessor_a == (size_t)-1 || predecessor_b == (size_t)-1) {
        return 0;
    }

    if (function->blocks[predecessor_a].has_terminator &&
        function->blocks[predecessor_a].terminator.kind == VALUE_SSA_TERM_JUMP &&
        function->blocks[predecessor_a].terminator.as.jump_target == header_id &&
        analysis->dominates[header_id * function->block_count + predecessor_a]) {
        body_id = predecessor_a;
        preheader_id = predecessor_b;
    } else if (function->blocks[predecessor_b].has_terminator &&
               function->blocks[predecessor_b].terminator.kind == VALUE_SSA_TERM_JUMP &&
               function->blocks[predecessor_b].terminator.as.jump_target == header_id &&
               analysis->dominates[header_id * function->block_count + predecessor_b]) {
        body_id = predecessor_b;
        preheader_id = predecessor_a;
    } else {
        return 0;
    }

    if (body_id >= function->block_count || preheader_id >= function->block_count) {
        return 0;
    }
    if (!function->blocks[body_id].has_terminator ||
        function->blocks[body_id].terminator.kind != VALUE_SSA_TERM_BRANCH ||
        function->blocks[body_id].phi_count != 0u ||
        function->blocks[preheader_id].phi_count != 0u ||
        !function->blocks[preheader_id].has_terminator ||
        function->blocks[preheader_id].terminator.kind != VALUE_SSA_TERM_JUMP ||
        function->blocks[preheader_id].terminator.as.jump_target != header_id) {
        return 0;
    }

    candidate_then = function->blocks[body_id].terminator.as.branch.then_target;
    candidate_else = function->blocks[body_id].terminator.as.branch.else_target;
    if (candidate_then == header_id && candidate_else != header_id) {
        out_loop->exit_id = candidate_else;
    } else if (candidate_else == header_id && candidate_then != header_id) {
        out_loop->exit_id = candidate_then;
    } else {
        return 0;
    }
    if (out_loop->exit_id >= function->block_count) {
        return 0;
    }

    out_loop->header_id = header_id;
    out_loop->body_id = body_id;
    out_loop->preheader_id = preheader_id;
    return 1;
}


static int value_ssa_perf_loop_preserves_local_slot(const ValueSsaFunction *function,
    const ValueSsaPerfSimpleLoop *loop,
    size_t slot_id) {
    size_t block_ids[2];
    size_t block_pos;

    if (!function || !loop || slot_id >= function->local_count) {
        return 0;
    }

    block_ids[0] = loop->header_id;
    block_ids[1] = loop->body_id;
    for (block_pos = 0; block_pos < 2u; ++block_pos) {
        const ValueSsaBasicBlock *block = &function->blocks[block_ids[block_pos]];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL &&
                instruction->as.store.slot.kind == VALUE_SSA_SLOT_LOCAL &&
                instruction->as.store.slot.id == slot_id) {
                return 0;
            }
        }
    }

    return 1;
}

static int value_ssa_perf_loop_preserves_global_slot(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaPerfSimpleLoop *loop,
    const unsigned char *readonly_globals,
    size_t slot_id) {
    (void)function;
    (void)loop;

    return program && readonly_globals && slot_id < program->global_count && readonly_globals[slot_id];
}

static int value_ssa_perf_value_is_loop_invariant(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!program || !function || !analysis || !readonly_globals || !loop) {
        return 0;
    }
    if (value.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }
    if (depth > 12u) {
        return 0;
    }
    if (value.value_id >= analysis->value_count || !analysis->has_def[value.value_id]) {
        return 0;
    }
    if (analysis->def_block_ids[value.value_id] != loop->header_id &&
        analysis->def_block_ids[value.value_id] != loop->body_id) {
        return 1;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }
    if (definition->kind == VALUE_SSA_INSTR_CALL) {
        return 0;
    }
    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return 1;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        return definition->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            definition->as.load_slot.id < function->local_count &&
            value_ssa_perf_loop_preserves_local_slot(function, loop, definition->as.load_slot.id);
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        return definition->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
            definition->as.load_slot.id < program->global_count &&
            value_ssa_perf_loop_preserves_global_slot(
                program, function, loop, readonly_globals, definition->as.load_slot.id);
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_perf_value_is_loop_invariant(
            program,
            function,
            analysis,
            readonly_globals,
            loop,
            definition->as.mov_value,
            depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        return value_ssa_perf_value_is_loop_invariant(
                   program,
                   function,
                   analysis,
                   readonly_globals,
                   loop,
                   definition->as.binary.lhs,
                   depth + 1u) &&
            value_ssa_perf_value_is_loop_invariant(
                   program,
                   function,
                   analysis,
                   readonly_globals,
                   loop,
                   definition->as.binary.rhs,
                   depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_perf_value_is_loop_invariant_for_indirect_addr(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *address_taken_locals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!program || !function || !analysis || !readonly_globals || !loop ||
        (!address_taken_locals && function->local_count > 0u)) {
        return 0;
    }
    if (value.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }
    if (depth > 12u) {
        return 0;
    }
    if (value.value_id >= analysis->value_count || !analysis->has_def[value.value_id]) {
        return 0;
    }
    if (analysis->def_block_ids[value.value_id] != loop->header_id &&
        analysis->def_block_ids[value.value_id] != loop->body_id) {
        return 1;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }
    if (definition->kind == VALUE_SSA_INSTR_CALL) {
        return 0;
    }
    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return 1;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        if (definition->as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
            definition->as.load_slot.id >= function->local_count ||
            !value_ssa_perf_loop_preserves_local_slot(function, loop, definition->as.load_slot.id)) {
            return 0;
        }
        if (function->locals[definition->as.load_slot.id].is_parameter &&
            function->locals[definition->as.load_slot.id].array_rank > 0u) {
            return 1;
        }
        return function->locals[definition->as.load_slot.id].array_rank == 0u &&
            !address_taken_locals[definition->as.load_slot.id];
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        return definition->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
            definition->as.load_slot.id < program->global_count &&
            value_ssa_perf_loop_preserves_global_slot(
                program, function, loop, readonly_globals, definition->as.load_slot.id);
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
            program,
            function,
            analysis,
            readonly_globals,
            address_taken_locals,
            loop,
            definition->as.mov_value,
            depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        return value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
                   program,
                   function,
                   analysis,
                   readonly_globals,
                   address_taken_locals,
                   loop,
                   definition->as.binary.lhs,
                   depth + 1u) &&
            value_ssa_perf_value_is_loop_invariant_for_indirect_addr(
                   program,
                   function,
                   analysis,
                   readonly_globals,
                   address_taken_locals,
                   loop,
                   definition->as.binary.rhs,
                   depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_perf_value_is_defined_inside_loop(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value) {
    size_t def_block_id;

    if (!function || !analysis || !loop || value.kind != VALUE_SSA_VALUE_ID || value.value_id >= analysis->value_count) {
        return 0;
    }
    if (!analysis->has_def[value.value_id]) {
        return 0;
    }

    def_block_id = analysis->def_block_ids[value.value_id];
    return def_block_id == loop->header_id || def_block_id == loop->body_id;
}

static int value_ssa_perf_clone_loop_invariant_value_into_preheader(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned char *has_mapping,
    ValueSsaValueRef *mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaValueRef *out_value,
    ValueSsaError *error);

typedef struct {
    size_t phi_result_id;
    size_t step_result_id;
    ValueSsaValueRef seed_value;
    long long step_immediate;
} ValueSsaPerfSimpleInduction;

static int value_ssa_perf_try_strength_reduce_induction_address_expr(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaPerfSimpleLoop *loop,
    const unsigned char *has_induction,
    const ValueSsaPerfSimpleInduction *inductions,
    ValueSsaValueRef value,
    size_t *out_phi_id,
    long long *out_scale,
    long long *out_offset,
    unsigned depth);

static int value_ssa_perf_match_simple_add_immediate_step(const ValueSsaInstruction *instruction,
    size_t base_value_id,
    long long *out_step_immediate) {
    if (!instruction || instruction->kind != VALUE_SSA_INSTR_BINARY ||
        instruction->as.binary.op != VALUE_SSA_BINARY_ADD || !out_step_immediate) {
        return 0;
    }

    if (instruction->as.binary.lhs.kind == VALUE_SSA_VALUE_ID &&
        instruction->as.binary.lhs.value_id == base_value_id &&
        instruction->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        *out_step_immediate = instruction->as.binary.rhs.immediate;
        return 1;
    }
    if (instruction->as.binary.rhs.kind == VALUE_SSA_VALUE_ID &&
        instruction->as.binary.rhs.value_id == base_value_id &&
        instruction->as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        *out_step_immediate = instruction->as.binary.lhs.immediate;
        return 1;
    }

    return 0;
}

static int value_ssa_perf_shift_result_is_worth_carried_recurrence(const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef result) {
    size_t use_index;
    size_t useful_address_uses = 0u;

    if (!analysis || result.kind != VALUE_SSA_VALUE_ID || result.value_id >= analysis->value_count) {
        return 0;
    }

    if (!analysis->use_counts || analysis->use_counts[result.value_id] < 3u) {
        return 0;
    }
    if (!analysis->use_sites || !analysis->use_offsets) {
        return 1;
    }

    for (use_index = 0; use_index < analysis->use_counts[result.value_id]; ++use_index) {
        const ValueSsaUseSite *site =
            &analysis->use_sites[analysis->use_offsets[result.value_id] + use_index];

        if (site->role == VALUE_SSA_USE_ROLE_LOAD_INDIRECT_ADDR ||
            site->role == VALUE_SSA_USE_ROLE_STORE_INDIRECT_ADDR) {
            ++useful_address_uses;
            continue;
        }

        if ((site->role == VALUE_SSA_USE_ROLE_BINARY_LHS ||
                site->role == VALUE_SSA_USE_ROLE_BINARY_RHS) &&
            site->instruction_index != (size_t)-1) {
            ++useful_address_uses;
        }
    }

    return useful_address_uses >= 2u;
}

static int value_ssa_perf_value_has_multiple_body_binary_uses(const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaValueRef value) {
    size_t use_index;
    size_t useful_uses = 0u;

    if (!analysis || !loop || value.kind != VALUE_SSA_VALUE_ID || value.value_id >= analysis->value_count ||
        !analysis->use_counts || !analysis->use_sites || !analysis->use_offsets) {
        return 0;
    }

    for (use_index = 0; use_index < analysis->use_counts[value.value_id]; ++use_index) {
        const ValueSsaUseSite *site = &analysis->use_sites[analysis->use_offsets[value.value_id] + use_index];

        if (site->block_id != loop->body_id) {
            continue;
        }
        if (site->role != VALUE_SSA_USE_ROLE_BINARY_LHS &&
            site->role != VALUE_SSA_USE_ROLE_BINARY_RHS) {
            continue;
        }
        ++useful_uses;
        if (useful_uses >= 2u) {
            return 1;
        }
    }

    return 0;
}

static int value_ssa_perf_body_has_later_related_induction_address(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaPerfSimpleLoop *loop,
    const unsigned char *has_induction,
    const ValueSsaPerfSimpleInduction *inductions,
    const ValueSsaBasicBlock *body,
    size_t start_instruction_index,
    size_t phi_id,
    long long scale) {
    size_t instruction_index;

    if (!function || !analysis || !loop || !has_induction || !inductions || !body) {
        return 0;
    }

    for (instruction_index = start_instruction_index + 1u; instruction_index < body->instruction_count; ++instruction_index) {
        const ValueSsaInstruction *instruction = &body->instructions[instruction_index];
        size_t other_phi_id;
        long long other_scale;
        long long other_offset;

        if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
            instruction->as.binary.op != VALUE_SSA_BINARY_ADD) {
            continue;
        }
        if (!value_ssa_perf_try_strength_reduce_induction_address_expr(
                (ValueSsaFunction *)function,
                analysis,
                loop,
                has_induction,
                inductions,
                instruction->result,
                &other_phi_id,
                &other_scale,
                &other_offset,
                0u)) {
            continue;
        }
        if (other_phi_id == phi_id && other_scale == scale) {
            return 1;
        }
    }

    return 0;
}

static int value_ssa_perf_collect_simple_inductions_for_loop(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaPerfSimpleLoop *loop,
    ValueSsaPerfSimpleInduction *inductions,
    unsigned char *has_induction) {
    const ValueSsaBasicBlock *header;
    size_t phi_index;

    if (!function || !analysis || !loop || !inductions || !has_induction ||
        loop->header_id >= function->block_count) {
        return 0;
    }

    header = &function->blocks[loop->header_id];

    for (phi_index = 0; phi_index < header->phi_count; ++phi_index) {
        const ValueSsaPhi *phi = &header->phis[phi_index];
        size_t input_index;
        size_t step_value_id = (size_t)-1;
        ValueSsaValueRef seed_value;
        long long step_immediate = 0;
        int saw_seed = 0;
        ValueSsaInstruction *step_definition = NULL;

        memset(&seed_value, 0, sizeof(seed_value));
        if (phi->result_id >= function->next_value_id || phi->input_count != 2u) {
            continue;
        }

        for (input_index = 0; input_index < phi->input_count; ++input_index) {
            if (phi->inputs[input_index].predecessor_block_id == loop->preheader_id) {
                seed_value = phi->inputs[input_index].value;
                saw_seed = 1;
            } else if (phi->inputs[input_index].predecessor_block_id == loop->body_id &&
                       phi->inputs[input_index].value.kind == VALUE_SSA_VALUE_ID) {
                step_value_id = phi->inputs[input_index].value.value_id;
            }
        }

        if (!saw_seed || step_value_id == (size_t)-1 ||
            step_value_id >= analysis->value_count || !analysis->has_def[step_value_id]) {
            continue;
        }

        step_definition = value_ssa_pass_lookup_instruction_definition(function, analysis, step_value_id);
        if (!step_definition ||
            !value_ssa_perf_match_simple_add_immediate_step(
                step_definition,
                phi->result_id,
                &step_immediate)) {
            continue;
        }

        has_induction[phi->result_id] = 1u;
        inductions[phi->result_id].phi_result_id = phi->result_id;
        inductions[phi->result_id].step_result_id = step_value_id;
        inductions[phi->result_id].seed_value = seed_value;
        inductions[phi->result_id].step_immediate = step_immediate;
    }

    return 1;
}

static int value_ssa_perf_try_strength_reduce_induction_address_expr(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaPerfSimpleLoop *loop,
    const unsigned char *has_induction,
    const ValueSsaPerfSimpleInduction *inductions,
    ValueSsaValueRef value,
    size_t *out_phi_id,
    long long *out_scale,
    long long *out_offset,
    unsigned depth) {
    ValueSsaInstruction *definition;
    size_t inner_phi_id;
    long long inner_scale;
    long long inner_offset;

    (void)inductions;

    if (!function || !analysis || !loop || !has_induction || !inductions ||
        !out_phi_id || !out_scale || !out_offset || depth > 12u) {
        return 0;
    }

    if (value.kind == VALUE_SSA_VALUE_ID &&
        value.value_id < function->next_value_id &&
        has_induction[value.value_id] &&
        value_ssa_perf_value_is_defined_inside_loop(function, analysis, loop, value)) {
        *out_phi_id = value.value_id;
        *out_scale = 1;
        *out_offset = 0;
        return 1;
    }

    if (value.kind != VALUE_SSA_VALUE_ID ||
        value.value_id >= analysis->value_count || !analysis->has_def[value.value_id] ||
        !value_ssa_perf_value_is_defined_inside_loop(function, analysis, loop, value)) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }

    if (definition->kind == VALUE_SSA_INSTR_MOV) {
        return value_ssa_perf_try_strength_reduce_induction_address_expr(
            function,
            analysis,
            loop,
            has_induction,
            inductions,
            definition->as.mov_value,
            out_phi_id,
            out_scale,
            out_offset,
            depth + 1u);
    }

    if (definition->kind != VALUE_SSA_INSTR_BINARY) {
        return 0;
    }

    if (definition->as.binary.op == VALUE_SSA_BINARY_SHIFT_LEFT) {
        if (!value_ssa_perf_try_strength_reduce_induction_address_expr(
                function,
                analysis,
                loop,
                has_induction,
                inductions,
                definition->as.binary.lhs,
                &inner_phi_id,
                &inner_scale,
                &inner_offset,
                depth + 1u) ||
            definition->as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE ||
            definition->as.binary.rhs.immediate < 0 ||
            definition->as.binary.rhs.immediate >= 31) {
            return 0;
        }

        *out_phi_id = inner_phi_id;
        *out_scale = inner_scale << definition->as.binary.rhs.immediate;
        *out_offset = inner_offset << definition->as.binary.rhs.immediate;
        return 1;
    }

    if (definition->as.binary.op != VALUE_SSA_BINARY_ADD) {
        return 0;
    }

    if (value_ssa_perf_try_strength_reduce_induction_address_expr(
            function,
            analysis,
            loop,
            has_induction,
            inductions,
            definition->as.binary.lhs,
            &inner_phi_id,
            &inner_scale,
            &inner_offset,
            depth + 1u) &&
        definition->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        *out_phi_id = inner_phi_id;
        *out_scale = inner_scale;
        *out_offset = inner_offset + definition->as.binary.rhs.immediate;
        return 1;
    }

    if (value_ssa_perf_try_strength_reduce_induction_address_expr(
            function,
            analysis,
            loop,
            has_induction,
            inductions,
            definition->as.binary.rhs,
            &inner_phi_id,
            &inner_scale,
            &inner_offset,
            depth + 1u) &&
        definition->as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        *out_phi_id = inner_phi_id;
        *out_scale = inner_scale;
        *out_offset = inner_offset + definition->as.binary.lhs.immediate;
        return 1;
    }

    return 0;
}

static int value_ssa_perf_strength_reduce_simple_induction_address_exprs(ValueSsaProgram *program,
    ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-329: invalid induction address strength-reduction contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis def_use;
        size_t header_id;
        int changed_function = 0;

        if (!function->has_body || function->next_value_id == 0u) {
            continue;
        }

        value_ssa_cfg_analysis_init(&cfg);
        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_cfg_analysis(function, &cfg, error) ||
            !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }

        for (header_id = 0; header_id < function->block_count; ++header_id) {
            ValueSsaPerfSimpleLoop loop;
            ValueSsaBasicBlock *preheader = NULL;
            ValueSsaBasicBlock *header;
            ValueSsaBasicBlock *body = NULL;
            ValueSsaBasicBlock rebuilt_preheader;
            ValueSsaBasicBlock rebuilt_header;
            ValueSsaBasicBlock rebuilt_body;
            unsigned char *has_induction = NULL;
            ValueSsaPerfSimpleInduction *inductions = NULL;
            size_t candidate_instruction_index = (size_t)-1;
            size_t candidate_phi_id = (size_t)-1;
            long long candidate_scale = 0;
            long long candidate_offset = 0;
            size_t candidate_result_id = (size_t)-1;
            int have_candidate = 0;
            size_t new_phi_value_id = (size_t)-1;
            size_t new_step_value_id = (size_t)-1;
            size_t init_value_id = (size_t)-1;
            size_t instruction_index;
            size_t phi_index;
            size_t step_value_id;
            long long step_immediate;
            long long carried_step;
            ValueSsaInstruction *step_def = NULL;
            int ok = 1;

            if (!value_ssa_perf_find_phi_header_single_body_loop(function, &cfg, header_id, &loop)) {
                continue;
            }

            preheader = &function->blocks[loop.preheader_id];
            header = &function->blocks[loop.header_id];
            body = &function->blocks[loop.body_id];

            has_induction = (unsigned char *)calloc(function->next_value_id, sizeof(unsigned char));
            inductions = (ValueSsaPerfSimpleInduction *)calloc(
                function->next_value_id,
                sizeof(ValueSsaPerfSimpleInduction));
            if (!has_induction || !inductions) {
                free(inductions);
                free(has_induction);
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-330: out of memory preparing induction address reduction");
                return 0;
            }

            if (!value_ssa_perf_collect_simple_inductions_for_loop(function, &def_use, &loop, inductions, has_induction)) {
                free(inductions);
                free(has_induction);
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-331: failed collecting simple inductions");
                return 0;
            }

            for (instruction_index = 0; instruction_index < body->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &body->instructions[instruction_index];
                size_t phi_id;
                long long scale;
                long long offset;

                if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
                    instruction->as.binary.op != VALUE_SSA_BINARY_ADD) {
                    continue;
                }

                if (!value_ssa_perf_try_strength_reduce_induction_address_expr(
                        function,
                        &def_use,
                        &loop,
                        has_induction,
                        inductions,
                        instruction->result,
                        &phi_id,
                        &scale,
                        &offset,
                        0u) ||
                    phi_id >= function->next_value_id ||
                    !has_induction[phi_id] ||
                    !value_ssa_perf_body_has_later_related_induction_address(
                        function,
                        &def_use,
                        &loop,
                        has_induction,
                        inductions,
                        body,
                        instruction_index,
                        phi_id,
                        scale)) {
                    continue;
                }

                if (scale == 0) {
                    continue;
                }

                if (!have_candidate ||
                    phi_id != candidate_phi_id ||
                    scale != candidate_scale ||
                    offset < candidate_offset ||
                    (offset == candidate_offset && instruction_index < candidate_instruction_index)) {
                    candidate_instruction_index = instruction_index;
                    candidate_phi_id = phi_id;
                    candidate_scale = scale;
                    candidate_offset = offset;
                    candidate_result_id = instruction->result.value_id;
                    have_candidate = 1;
                }
            }

            if (!have_candidate || candidate_instruction_index == (size_t)-1) {
                free(inductions);
                free(has_induction);
                continue;
            }

            step_value_id = inductions[candidate_phi_id].step_result_id;
            step_immediate = inductions[candidate_phi_id].step_immediate;
            carried_step = candidate_scale * step_immediate;
            if (carried_step == 0) {
                free(inductions);
                free(has_induction);
                continue;
            }

            step_def = value_ssa_pass_lookup_instruction_definition(function, &def_use, step_value_id);
            if (!step_def || !value_ssa_perf_match_simple_add_immediate_step(step_def, candidate_phi_id, &step_immediate)) {
                free(inductions);
                free(has_induction);
                continue;
            }

            new_phi_value_id = value_ssa_function_allocate_value(function);
            new_step_value_id = value_ssa_function_allocate_value(function);
            if (inductions[candidate_phi_id].seed_value.kind == VALUE_SSA_VALUE_ID) {
                init_value_id = value_ssa_function_allocate_value(function);
            }
            if (new_phi_value_id == (size_t)-1 ||
                new_step_value_id == (size_t)-1 ||
                (inductions[candidate_phi_id].seed_value.kind == VALUE_SSA_VALUE_ID &&
                    init_value_id == (size_t)-1)) {
                free(inductions);
                free(has_induction);
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-336: failed allocating induction address values");
                return 0;
            }

            memset(&rebuilt_preheader, 0, sizeof(rebuilt_preheader));
            memset(&rebuilt_header, 0, sizeof(rebuilt_header));
            memset(&rebuilt_body, 0, sizeof(rebuilt_body));
            rebuilt_preheader.id = preheader->id;
            rebuilt_header.id = header->id;
            rebuilt_body.id = body->id;

            for (instruction_index = 0; instruction_index < preheader->instruction_count; ++instruction_index) {
                if (!value_ssa_block_append_instruction(&rebuilt_preheader, &preheader->instructions[instruction_index], error)) {
                    ok = 0;
                    break;
                }
            }
            if (ok && inductions[candidate_phi_id].seed_value.kind == VALUE_SSA_VALUE_ID) {
                ValueSsaInstruction init_instruction;

                memset(&init_instruction, 0, sizeof(init_instruction));
                init_instruction.kind = VALUE_SSA_INSTR_BINARY;
                init_instruction.has_result = 1;
                init_instruction.result = value_ssa_value_id(init_value_id);
                init_instruction.as.binary.op = VALUE_SSA_BINARY_MUL;
                init_instruction.as.binary.lhs = inductions[candidate_phi_id].seed_value;
                init_instruction.as.binary.rhs = value_ssa_value_immediate(candidate_scale);
                if (!value_ssa_block_append_instruction(&rebuilt_preheader, &init_instruction, error)) {
                    ok = 0;
                }
                if (ok && candidate_offset != 0) {
                    size_t offset_value_id = value_ssa_function_allocate_value(function);
                    if (offset_value_id == (size_t)-1) {
                        ok = 0;
                    } else {
                        memset(&init_instruction, 0, sizeof(init_instruction));
                        init_instruction.kind = VALUE_SSA_INSTR_BINARY;
                        init_instruction.has_result = 1;
                        init_instruction.result = value_ssa_value_id(offset_value_id);
                        init_instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
                        init_instruction.as.binary.lhs = value_ssa_value_id(init_value_id);
                        init_instruction.as.binary.rhs = value_ssa_value_immediate(candidate_offset);
                        if (!value_ssa_block_append_instruction(&rebuilt_preheader, &init_instruction, error)) {
                            ok = 0;
                        } else {
                            init_value_id = offset_value_id;
                        }
                    }
                }
            }

            for (phi_index = 0; ok && phi_index < header->phi_count; ++phi_index) {
                if (!value_ssa_block_append_phi(
                        &rebuilt_header,
                        header->phis[phi_index].result_id,
                        header->phis[phi_index].inputs,
                        header->phis[phi_index].input_count,
                        error)) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                ValueSsaPhiInput new_phi_inputs[2];

                new_phi_inputs[0].predecessor_block_id = loop.preheader_id;
                if (inductions[candidate_phi_id].seed_value.kind == VALUE_SSA_VALUE_IMMEDIATE) {
                    new_phi_inputs[0].value = value_ssa_value_immediate(
                        inductions[candidate_phi_id].seed_value.immediate * candidate_scale + candidate_offset);
                } else {
                    new_phi_inputs[0].value = value_ssa_value_id(init_value_id);
                }
                new_phi_inputs[1].predecessor_block_id = loop.body_id;
                new_phi_inputs[1].value = value_ssa_value_id(new_step_value_id);
                if (!value_ssa_block_append_phi(&rebuilt_header, new_phi_value_id, new_phi_inputs, 2u, error)) {
                    ok = 0;
                }
            }

            for (instruction_index = 0; ok && instruction_index < header->instruction_count; ++instruction_index) {
                if (!value_ssa_block_append_instruction(&rebuilt_header, &header->instructions[instruction_index], error)) {
                    ok = 0;
                    break;
                }
            }

            for (instruction_index = 0; ok && instruction_index < body->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *source = &body->instructions[instruction_index];
                ValueSsaInstruction replacement;

                if (instruction_index == candidate_instruction_index) {
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = value_ssa_value_id(candidate_result_id);
                    replacement.as.mov_value = value_ssa_value_id(new_phi_value_id);
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    continue;
                }
                if (source->kind == VALUE_SSA_INSTR_BINARY &&
                    source->as.binary.op == VALUE_SSA_BINARY_ADD) {
                    size_t other_phi_id;
                    long long other_scale;
                    long long other_offset;

                    if (value_ssa_perf_try_strength_reduce_induction_address_expr(
                            function,
                            &def_use,
                            &loop,
                            has_induction,
                            inductions,
                            source->result,
                            &other_phi_id,
                            &other_scale,
                            &other_offset,
                            0u) &&
                        other_phi_id == candidate_phi_id &&
                        other_scale == candidate_scale) {
                        memset(&replacement, 0, sizeof(replacement));
                        replacement.kind = other_offset == candidate_offset
                            ? VALUE_SSA_INSTR_MOV
                            : VALUE_SSA_INSTR_BINARY;
                        replacement.has_result = 1;
                        replacement.result = source->result;
                        if (replacement.kind == VALUE_SSA_INSTR_MOV) {
                            replacement.as.mov_value = value_ssa_value_id(new_phi_value_id);
                        } else {
                            replacement.as.binary.op = VALUE_SSA_BINARY_ADD;
                            replacement.as.binary.lhs = value_ssa_value_id(new_phi_value_id);
                            replacement.as.binary.rhs =
                                value_ssa_value_immediate(other_offset - candidate_offset);
                        }
                        if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                            ok = 0;
                            break;
                        }
                        continue;
                    }
                }
                if (!value_ssa_block_append_instruction(&rebuilt_body, source, error)) {
                    ok = 0;
                    break;
                }
            }

            if (ok) {
                ValueSsaInstruction new_step_instruction;

                memset(&new_step_instruction, 0, sizeof(new_step_instruction));
                new_step_instruction.kind = VALUE_SSA_INSTR_BINARY;
                new_step_instruction.has_result = 1;
                new_step_instruction.result = value_ssa_value_id(new_step_value_id);
                new_step_instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
                new_step_instruction.as.binary.lhs = value_ssa_value_id(new_phi_value_id);
                new_step_instruction.as.binary.rhs = value_ssa_value_immediate(carried_step);
                if (!value_ssa_block_append_instruction(&rebuilt_body, &new_step_instruction, error)) {
                    ok = 0;
                }
            }

            if (ok) {
                rebuilt_preheader.has_terminator = preheader->has_terminator;
                rebuilt_preheader.terminator = preheader->terminator;
                rebuilt_header.has_terminator = header->has_terminator;
                rebuilt_header.terminator = header->terminator;
                rebuilt_body.has_terminator = body->has_terminator;
                rebuilt_body.terminator = body->terminator;

                value_ssa_basic_block_free(preheader);
                *preheader = rebuilt_preheader;
                memset(&rebuilt_preheader, 0, sizeof(rebuilt_preheader));

                value_ssa_basic_block_free(header);
                *header = rebuilt_header;
                memset(&rebuilt_header, 0, sizeof(rebuilt_header));

                value_ssa_basic_block_free(body);
                *body = rebuilt_body;
                memset(&rebuilt_body, 0, sizeof(rebuilt_body));
                changed_function = 1;
            }

            free(inductions);
            free(has_induction);
            value_ssa_basic_block_free(&rebuilt_preheader);
            value_ssa_basic_block_free(&rebuilt_header);
            value_ssa_basic_block_free(&rebuilt_body);
            if (!ok) {
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                return 0;
            }
            if (changed_function) {
                break;
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_cfg_analysis_free(&cfg);

        if (changed_function &&
            (!value_ssa_simplify_trivial_values(program, error) ||
                !value_ssa_eliminate_dead_value_defs(program, error) ||
                !value_ssa_simplify_cfg(program, error))) {
            return 0;
        }
        if (changed_function) {
            --function_index;
        }
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_perf_strength_reduce_simple_induction_shifts(ValueSsaProgram *program,
    ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-332: invalid induction shift strength-reduction contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis def_use;
        size_t header_id;
        int changed_function = 0;

        if (!function->has_body || function->next_value_id == 0u) {
            continue;
        }

        value_ssa_cfg_analysis_init(&cfg);
        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_cfg_analysis(function, &cfg, error) ||
            !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }

        for (header_id = 0; header_id < function->block_count; ++header_id) {
            ValueSsaPerfSimpleLoop loop;
            ValueSsaBasicBlock *preheader;
            ValueSsaBasicBlock *header;
            ValueSsaBasicBlock *body;
            ValueSsaBasicBlock rebuilt_preheader;
            ValueSsaBasicBlock rebuilt_header;
            ValueSsaBasicBlock rebuilt_body;
            unsigned char *has_induction = NULL;
            ValueSsaPerfSimpleInduction *inductions = NULL;
            size_t candidate_block_id = (size_t)-1;
            size_t candidate_instruction_index = (size_t)-1;
            size_t candidate_phi_id = (size_t)-1;
            long long candidate_shift = 0;
            size_t new_phi_value_id = (size_t)-1;
            size_t new_step_value_id = (size_t)-1;
            size_t init_value_id = (size_t)-1;
            long long seed_immediate_value = 0;
            long long step_value = 0;
            size_t phi_index;
            size_t instruction_index;
            int ok = 1;

            if (!value_ssa_perf_find_phi_header_single_body_loop(function, &cfg, header_id, &loop)) {
                continue;
            }

            preheader = &function->blocks[loop.preheader_id];
            header = &function->blocks[loop.header_id];
            body = &function->blocks[loop.body_id];

            has_induction = (unsigned char *)calloc(function->next_value_id, sizeof(unsigned char));
            inductions = (ValueSsaPerfSimpleInduction *)calloc(
                function->next_value_id,
                sizeof(ValueSsaPerfSimpleInduction));
            if (!has_induction || !inductions) {
                free(inductions);
                free(has_induction);
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-333: out of memory preparing induction shift reduction");
                return 0;
            }
            if (!value_ssa_perf_collect_simple_inductions_for_loop(function, &def_use, &loop, inductions, has_induction)) {
                free(inductions);
                free(has_induction);
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-334: failed collecting induction shift candidates");
                return 0;
            }

            for (instruction_index = 0; instruction_index < header->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &header->instructions[instruction_index];
                if (instruction->kind == VALUE_SSA_INSTR_BINARY &&
                    instruction->as.binary.op == VALUE_SSA_BINARY_SHIFT_LEFT &&
                    value_ssa_perf_shift_result_is_worth_carried_recurrence(&def_use, instruction->result) &&
                    instruction->as.binary.lhs.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.binary.lhs.value_id < function->next_value_id &&
                    has_induction[instruction->as.binary.lhs.value_id] &&
                    instruction->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                    instruction->as.binary.rhs.immediate >= 0 &&
                    instruction->as.binary.rhs.immediate < 31) {
                    candidate_block_id = loop.header_id;
                    candidate_instruction_index = instruction_index;
                    candidate_phi_id = instruction->as.binary.lhs.value_id;
                    candidate_shift = instruction->as.binary.rhs.immediate;
                    break;
                }
            }

            for (instruction_index = 0;
                 candidate_block_id == (size_t)-1 && instruction_index < body->instruction_count;
                 ++instruction_index) {
                ValueSsaInstruction *instruction = &body->instructions[instruction_index];
                if (instruction->kind == VALUE_SSA_INSTR_BINARY &&
                    instruction->as.binary.op == VALUE_SSA_BINARY_SHIFT_LEFT &&
                    value_ssa_perf_shift_result_is_worth_carried_recurrence(&def_use, instruction->result) &&
                    instruction->as.binary.lhs.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.binary.lhs.value_id < function->next_value_id &&
                    has_induction[instruction->as.binary.lhs.value_id] &&
                    instruction->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                    instruction->as.binary.rhs.immediate >= 0 &&
                    instruction->as.binary.rhs.immediate < 31) {
                    candidate_block_id = loop.body_id;
                    candidate_instruction_index = instruction_index;
                    candidate_phi_id = instruction->as.binary.lhs.value_id;
                    candidate_shift = instruction->as.binary.rhs.immediate;
                    break;
                }
            }

            if (candidate_block_id == (size_t)-1) {
                free(inductions);
                free(has_induction);
                continue;
            }

            new_phi_value_id = value_ssa_function_allocate_value(function);
            new_step_value_id = value_ssa_function_allocate_value(function);
            if (inductions[candidate_phi_id].seed_value.kind == VALUE_SSA_VALUE_ID) {
                init_value_id = value_ssa_function_allocate_value(function);
            }
            if (new_step_value_id == (size_t)-1 ||
                (inductions[candidate_phi_id].seed_value.kind == VALUE_SSA_VALUE_ID &&
                    init_value_id == (size_t)-1)) {
                free(inductions);
                free(has_induction);
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-335: failed allocating induction shift values");
                return 0;
            }

            if (inductions[candidate_phi_id].seed_value.kind == VALUE_SSA_VALUE_IMMEDIATE) {
                seed_immediate_value = inductions[candidate_phi_id].seed_value.immediate << candidate_shift;
            }
            step_value = inductions[candidate_phi_id].step_immediate << candidate_shift;

            memset(&rebuilt_preheader, 0, sizeof(rebuilt_preheader));
            memset(&rebuilt_header, 0, sizeof(rebuilt_header));
            memset(&rebuilt_body, 0, sizeof(rebuilt_body));
            rebuilt_preheader.id = preheader->id;
            rebuilt_header.id = header->id;
            rebuilt_body.id = body->id;

            for (instruction_index = 0; instruction_index < preheader->instruction_count; ++instruction_index) {
                if (!value_ssa_block_append_instruction(&rebuilt_preheader, &preheader->instructions[instruction_index], error)) {
                    ok = 0;
                    break;
                }
            }
            if (ok && inductions[candidate_phi_id].seed_value.kind == VALUE_SSA_VALUE_ID) {
                ValueSsaInstruction init_instruction;

                memset(&init_instruction, 0, sizeof(init_instruction));
                init_instruction.kind = VALUE_SSA_INSTR_BINARY;
                init_instruction.has_result = 1;
                init_instruction.result = value_ssa_value_id(init_value_id);
                init_instruction.as.binary.op = VALUE_SSA_BINARY_SHIFT_LEFT;
                init_instruction.as.binary.lhs = inductions[candidate_phi_id].seed_value;
                init_instruction.as.binary.rhs = value_ssa_value_immediate(candidate_shift);
                if (!value_ssa_block_append_instruction(&rebuilt_preheader, &init_instruction, error)) {
                    ok = 0;
                }
            }
            for (phi_index = 0; ok && phi_index < header->phi_count; ++phi_index) {
                if (!value_ssa_block_append_phi(
                        &rebuilt_header,
                        header->phis[phi_index].result_id,
                        header->phis[phi_index].inputs,
                        header->phis[phi_index].input_count,
                        error)) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                ValueSsaPhiInput phi_inputs[2];

                phi_inputs[0].predecessor_block_id = loop.preheader_id;
                phi_inputs[0].value = inductions[candidate_phi_id].seed_value.kind == VALUE_SSA_VALUE_IMMEDIATE
                    ? value_ssa_value_immediate(seed_immediate_value)
                    : value_ssa_value_id(init_value_id);
                phi_inputs[1].predecessor_block_id = loop.body_id;
                phi_inputs[1].value = value_ssa_value_id(new_step_value_id);
                if (!value_ssa_block_append_phi(&rebuilt_header, new_phi_value_id, phi_inputs, 2u, error)) {
                    ok = 0;
                }
            }

            for (instruction_index = 0; ok && instruction_index < header->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &header->instructions[instruction_index];
                ValueSsaInstruction replacement;

                if (candidate_block_id == loop.header_id && instruction_index == candidate_instruction_index) {
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = value_ssa_value_id(new_phi_value_id);
                    if (!value_ssa_block_append_instruction(&rebuilt_header, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    continue;
                }
                if (!value_ssa_block_append_instruction(&rebuilt_header, instruction, error)) {
                    ok = 0;
                    break;
                }
            }

            for (instruction_index = 0; ok && instruction_index < body->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &body->instructions[instruction_index];
                ValueSsaInstruction replacement;

                if (candidate_block_id == loop.body_id && instruction_index == candidate_instruction_index) {
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = value_ssa_value_id(new_phi_value_id);
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    continue;
                }
                if (!value_ssa_block_append_instruction(&rebuilt_body, instruction, error)) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                ValueSsaInstruction step_instruction;

                memset(&step_instruction, 0, sizeof(step_instruction));
                step_instruction.kind = VALUE_SSA_INSTR_BINARY;
                step_instruction.has_result = 1;
                step_instruction.result = value_ssa_value_id(new_step_value_id);
                step_instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
                step_instruction.as.binary.lhs = value_ssa_value_id(new_phi_value_id);
                step_instruction.as.binary.rhs = value_ssa_value_immediate(step_value);
                if (!value_ssa_block_append_instruction(&rebuilt_body, &step_instruction, error)) {
                    ok = 0;
                }
            }

            if (ok) {
                rebuilt_preheader.has_terminator = preheader->has_terminator;
                rebuilt_preheader.terminator = preheader->terminator;
                rebuilt_header.has_terminator = header->has_terminator;
                rebuilt_header.terminator = header->terminator;
                rebuilt_body.has_terminator = body->has_terminator;
                rebuilt_body.terminator = body->terminator;

                value_ssa_basic_block_free(header);
                *header = rebuilt_header;
                memset(&rebuilt_header, 0, sizeof(rebuilt_header));

                value_ssa_basic_block_free(body);
                *body = rebuilt_body;
                memset(&rebuilt_body, 0, sizeof(rebuilt_body));

                value_ssa_basic_block_free(preheader);
                *preheader = rebuilt_preheader;
                memset(&rebuilt_preheader, 0, sizeof(rebuilt_preheader));

                changed_function = 1;
            }

            free(inductions);
            free(has_induction);
            value_ssa_basic_block_free(&rebuilt_preheader);
            value_ssa_basic_block_free(&rebuilt_header);
            value_ssa_basic_block_free(&rebuilt_body);
            if (!ok) {
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                return 0;
            }
            if (changed_function) {
                break;
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_cfg_analysis_free(&cfg);

        if (changed_function &&
            (!value_ssa_simplify_trivial_values(program, error) ||
                !value_ssa_eliminate_dead_value_defs(program, error) ||
                !value_ssa_simplify_cfg(program, error))) {
            return 0;
        }
        if (changed_function) {
            --function_index;
        }
    }

    return value_ssa_verify_program(program, error);
}


#endif
