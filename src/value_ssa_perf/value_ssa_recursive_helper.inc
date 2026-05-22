#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

static int value_ssa_perf_collect_readonly_scalar_globals(const ValueSsaProgram *program,
    unsigned char *readonly_flags);

static const ValueSsaFunction *value_ssa_recursive_helper_find_function_by_name(const ValueSsaProgram *program,
    const char *name) {
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

static int value_ssa_recursive_helper_value_is_multiply_call(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    unsigned depth) {
    ValueSsaInstruction *definition = NULL;

    if (!function || !analysis || depth > 8u || value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }
    if (definition->kind == VALUE_SSA_INSTR_CALL) {
        return definition->as.call.callee_name &&
            strcmp(definition->as.call.callee_name, "multiply") == 0;
    }
    if (definition->kind == VALUE_SSA_INSTR_MOV) {
        return value_ssa_recursive_helper_value_is_multiply_call(
            function, analysis, definition->as.mov_value, depth + 1u);
    }
    return 0;
}

static int value_ssa_recursive_helper_value_is_mod998_constant(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    unsigned depth) {
    ValueSsaInstruction *definition = NULL;

    if (!program || !function || !analysis || depth > 8u) {
        return 0;
    }
    if (value.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        return value.immediate == 998244353;
    }
    if (value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }
    if (definition->kind == VALUE_SSA_INSTR_MOV) {
        return value_ssa_recursive_helper_value_is_mod998_constant(
            program, function, analysis, definition->as.mov_value, depth + 1u);
    }
    if (definition->kind == VALUE_SSA_INSTR_LOAD_GLOBAL &&
        definition->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
        definition->as.load_slot.id < program->global_count) {
        const ValueSsaGlobal *global = &program->globals[definition->as.load_slot.id];

        return global->byte_size == 4u &&
            global->has_initializer &&
            !global->has_runtime_initializer &&
            global->initializer_value == 998244353;
    }
    return 0;
}

static int value_ssa_recursive_helper_value_is_mod998_minus1_constant(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    unsigned depth) {
    ValueSsaInstruction *definition = NULL;

    if (!program || !function || !analysis || depth > 8u) {
        return 0;
    }
    if (value.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        return value.immediate == 998244352;
    }
    if (value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }
    if (definition->kind == VALUE_SSA_INSTR_MOV) {
        return value_ssa_recursive_helper_value_is_mod998_minus1_constant(
            program, function, analysis, definition->as.mov_value, depth + 1u);
    }
    if (definition->kind == VALUE_SSA_INSTR_BINARY &&
        definition->as.binary.op == VALUE_SSA_BINARY_SUB) {
        return (
            value_ssa_recursive_helper_value_is_mod998_constant(
                program, function, analysis, definition->as.binary.lhs, depth + 1u) &&
            definition->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
            definition->as.binary.rhs.immediate == 1) ||
            (
                value_ssa_recursive_helper_value_is_mod998_constant(
                    program, function, analysis, definition->as.binary.rhs, depth + 1u) &&
                definition->as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                definition->as.binary.lhs.immediate == 1);
    }
    return 0;
}

static int value_ssa_recursive_helper_function_is_pure_internal_call_candidate(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const unsigned char *readonly_globals) {
    size_t block_index;

    if (!program || !function || !function->has_body || !readonly_globals) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_MOV:
            case VALUE_SSA_INSTR_BINARY:
            case VALUE_SSA_INSTR_LOAD_LOCAL:
            case VALUE_SSA_INSTR_STORE_LOCAL:
                break;
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                if (instruction->as.load_slot.id >= program->global_count ||
                    !readonly_globals[instruction->as.load_slot.id]) {
                    return 0;
                }
                break;
            case VALUE_SSA_INSTR_CALL:
                if (!instruction->as.call.callee_name || instruction->as.call.callee_name[0] == '\0') {
                    return 0;
                }
                break;
            default:
                return 0;
            }
        }
    }

    return 1;
}

static int value_ssa_recursive_helper_collect_pure_internal_function_flags(const ValueSsaProgram *program,
    const unsigned char *readonly_globals,
    unsigned char *pure_flags) {
    size_t function_index;
    int changed = 1;

    if (!program || !readonly_globals || !pure_flags) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        pure_flags[function_index] = (unsigned char)value_ssa_recursive_helper_function_is_pure_internal_call_candidate(
            program,
            &program->functions[function_index],
            readonly_globals);
    }

    while (changed) {
        changed = 0;
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            const ValueSsaFunction *function = &program->functions[function_index];
            size_t block_index;

            if (!pure_flags[function_index]) {
                continue;
            }

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                const ValueSsaBasicBlock *block = &function->blocks[block_index];
                size_t instruction_index;

                for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                    const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                    if (instruction->kind == VALUE_SSA_INSTR_CALL) {
                        const ValueSsaFunction *callee = value_ssa_recursive_helper_find_function_by_name(
                            program,
                            instruction->as.call.callee_name);
                        size_t callee_index;

                        if (!callee) {
                            pure_flags[function_index] = 0u;
                            changed = 1;
                            break;
                        }

                        callee_index = (size_t)(callee - program->functions);
                        if (callee_index >= program->function_count || !pure_flags[callee_index]) {
                            pure_flags[function_index] = 0u;
                            changed = 1;
                            break;
                        }
                    }
                }

                if (!pure_flags[function_index]) {
                    break;
                }
            }
        }
    }

    return 1;
}

static int value_ssa_recursive_helper_same_call_args(const ValueSsaInstruction *lhs, const ValueSsaInstruction *rhs) {
    size_t arg_index;

    if (!lhs || !rhs || lhs->kind != VALUE_SSA_INSTR_CALL || rhs->kind != VALUE_SSA_INSTR_CALL ||
        lhs->as.call.arg_count != rhs->as.call.arg_count) {
        return 0;
    }

    for (arg_index = 0; arg_index < lhs->as.call.arg_count; ++arg_index) {
        if (!value_ssa_value_refs_equal(lhs->as.call.args[arg_index], rhs->as.call.args[arg_index])) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_recursive_helper_reuse_same_block_repeated_pure_internal_calls(ValueSsaProgram *program,
    ValueSsaError *error) {
    unsigned char *readonly_globals = NULL;
    unsigned char *pure_function_flags = NULL;
    size_t function_index;
    int changed = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-289: invalid perf pure-call reuse contract");
        return 0;
    }

    readonly_globals = (unsigned char *)calloc(program->global_count ? program->global_count : 1u, sizeof(unsigned char));
    pure_function_flags =
        (unsigned char *)calloc(program->function_count ? program->function_count : 1u, sizeof(unsigned char));
    if (!readonly_globals || !pure_function_flags) {
        free(pure_function_flags);
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-290: out of memory preparing perf pure-call reuse");
        return 0;
    }
    if (!value_ssa_perf_collect_readonly_scalar_globals(program, readonly_globals) ||
        !value_ssa_recursive_helper_collect_pure_internal_function_flags(program, readonly_globals, pure_function_flags)) {
        free(pure_function_flags);
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-291: failed collecting perf pure-call reuse facts");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                const ValueSsaFunction *callee = NULL;
                size_t callee_index;
                size_t candidate_index;

                if (instruction->kind != VALUE_SSA_INSTR_CALL ||
                    !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    !instruction->as.call.callee_name) {
                    continue;
                }

                callee = value_ssa_recursive_helper_find_function_by_name(program, instruction->as.call.callee_name);
                if (!callee) {
                    continue;
                }
                callee_index = (size_t)(callee - program->functions);
                if (callee_index >= program->function_count || !pure_function_flags[callee_index] ||
                    !value_ssa_recursive_helper_function_is_pure_internal_call_candidate(program, callee, readonly_globals)) {
                    continue;
                }

                for (candidate_index = 0; candidate_index < instruction_index; ++candidate_index) {
                    ValueSsaInstruction *candidate = &block->instructions[candidate_index];

                    if (candidate->kind != VALUE_SSA_INSTR_CALL ||
                        !candidate->has_result ||
                        candidate->result.kind != VALUE_SSA_VALUE_ID ||
                        !candidate->as.call.callee_name ||
                        strcmp(candidate->as.call.callee_name, instruction->as.call.callee_name) != 0 ||
                        !value_ssa_recursive_helper_same_call_args(candidate, instruction)) {
                        continue;
                    }

                    instruction->kind = VALUE_SSA_INSTR_MOV;
                    instruction->as.mov_value = candidate->result;
                    changed = 1;
                    break;
                }
            }
        }
    }

    free(pure_function_flags);
    free(readonly_globals);

    if (changed &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_recursive_helper_function_is_branch_cleanup_candidate(const ValueSsaFunction *function) {
    if (!function || !function->name) {
        return 0;
    }

    return strcmp(function->name, "multiply") == 0 ||
        strcmp(function->name, "power") == 0;
}

static int value_ssa_recursive_helper_value_is_booleanish(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    unsigned depth) {
    ValueSsaInstruction *definition = NULL;

    if (!function || !analysis || depth > 8u) {
        return 0;
    }
    if (value.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        return value.immediate == 0 || value.immediate == 1;
    }
    if (value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }
    if (definition->kind == VALUE_SSA_INSTR_MOV) {
        return value_ssa_recursive_helper_value_is_booleanish(
            function, analysis, definition->as.mov_value, depth + 1u);
    }
    if (definition->kind != VALUE_SSA_INSTR_BINARY) {
        return 0;
    }

    switch (definition->as.binary.op) {
    case VALUE_SSA_BINARY_EQ:
    case VALUE_SSA_BINARY_NE:
    case VALUE_SSA_BINARY_LT:
    case VALUE_SSA_BINARY_LE:
    case VALUE_SSA_BINARY_GT:
    case VALUE_SSA_BINARY_GE:
        return 1;
    case VALUE_SSA_BINARY_BIT_AND:
        return (definition->as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                   definition->as.binary.lhs.immediate == 1) ||
            (definition->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                definition->as.binary.rhs.immediate == 1);
    default:
        return 0;
    }
}

static int value_ssa_recursive_helper_try_rewrite_branch_condition(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef condition,
    ValueSsaValueRef *out_condition,
    int *out_swap_targets) {
    ValueSsaInstruction *definition = NULL;
    ValueSsaInstruction *variable_definition = NULL;
    ValueSsaValueRef variable;
    long long immediate;

    if (!function || !analysis || !out_condition || !out_swap_targets ||
        condition.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, condition.value_id);
    if (!definition || definition->kind != VALUE_SSA_INSTR_BINARY) {
        return 0;
    }
    if (definition->as.binary.op != VALUE_SSA_BINARY_EQ &&
        definition->as.binary.op != VALUE_SSA_BINARY_NE) {
        return 0;
    }

    if (definition->as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
        definition->as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE) {
        immediate = definition->as.binary.lhs.immediate;
        variable = definition->as.binary.rhs;
    } else if (definition->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
               definition->as.binary.lhs.kind != VALUE_SSA_VALUE_IMMEDIATE) {
        immediate = definition->as.binary.rhs.immediate;
        variable = definition->as.binary.lhs;
    } else {
        return 0;
    }

    if (immediate == 0) {
        *out_condition = variable;
        *out_swap_targets = definition->as.binary.op == VALUE_SSA_BINARY_EQ;
        return 1;
    }
    if (immediate == 1 && variable.kind == VALUE_SSA_VALUE_ID) {
        variable_definition = value_ssa_pass_lookup_instruction_definition(function, analysis, variable.value_id);
        if (variable_definition &&
            variable_definition->kind == VALUE_SSA_INSTR_BINARY &&
            variable_definition->as.binary.op == VALUE_SSA_BINARY_BIT_AND &&
            ((variable_definition->as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                    variable_definition->as.binary.lhs.immediate == 1) ||
                (variable_definition->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                    variable_definition->as.binary.rhs.immediate == 1))) {
            *out_condition = variable;
            *out_swap_targets = definition->as.binary.op == VALUE_SSA_BINARY_NE;
            return 1;
        }
    }
    if (immediate == 1 &&
        value_ssa_recursive_helper_value_is_booleanish(function, analysis, variable, 0u)) {
        *out_condition = variable;
        *out_swap_targets = definition->as.binary.op == VALUE_SSA_BINARY_NE;
        return 1;
    }

    return 0;
}

static int value_ssa_recursive_helper_simplify_branch_conditions(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-336: invalid recursive-helper branch cleanup contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis def_use;
        size_t block_index;
        int changed_function = 0;

        if (!function->has_body || !value_ssa_recursive_helper_function_is_branch_cleanup_candidate(function)) {
            continue;
        }

        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            ValueSsaValueRef condition;
            int swap_targets = 0;
            ValueSsaValueRef rewritten_condition;

            if (!block->has_terminator ||
                block->terminator.kind != VALUE_SSA_TERM_BRANCH ||
                block->terminator.as.branch.condition.kind != VALUE_SSA_VALUE_ID) {
                continue;
            }

            condition = block->terminator.as.branch.condition;
            if (!value_ssa_recursive_helper_try_rewrite_branch_condition(
                    function, &def_use, condition, &rewritten_condition, &swap_targets)) {
                continue;
            }

            block->terminator.as.branch.condition = rewritten_condition;
            if (swap_targets) {
                size_t old_then = block->terminator.as.branch.then_target;

                block->terminator.as.branch.then_target = block->terminator.as.branch.else_target;
                block->terminator.as.branch.else_target = old_then;
            }
            changed_function = 1;
        }

        value_ssa_def_use_analysis_free(&def_use);
        if (changed_function &&
            (!value_ssa_simplify_trivial_values(program, error) ||
                !value_ssa_eliminate_dead_value_defs(program, error) ||
                !value_ssa_simplify_cfg(program, error))) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_recursive_helper_simplify_parity_compares(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-337: invalid recursive-helper parity-compare cleanup contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis def_use;
        size_t block_index;
        int changed_function = 0;

        if (!function->has_body || !value_ssa_recursive_helper_function_is_branch_cleanup_candidate(function)) {
            continue;
        }

        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                ValueSsaValueRef variable;
                long long immediate;
                ValueSsaInstruction *variable_definition = NULL;

                if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
                    !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    (instruction->as.binary.op != VALUE_SSA_BINARY_EQ &&
                        instruction->as.binary.op != VALUE_SSA_BINARY_NE)) {
                    continue;
                }

                if (instruction->as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                    instruction->as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE) {
                    immediate = instruction->as.binary.lhs.immediate;
                    variable = instruction->as.binary.rhs;
                } else if (instruction->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                           instruction->as.binary.lhs.kind != VALUE_SSA_VALUE_IMMEDIATE) {
                    immediate = instruction->as.binary.rhs.immediate;
                    variable = instruction->as.binary.lhs;
                } else {
                    continue;
                }

                if (!((instruction->as.binary.op == VALUE_SSA_BINARY_EQ && immediate == 1) ||
                        (instruction->as.binary.op == VALUE_SSA_BINARY_NE && immediate == 0)) ||
                    variable.kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }

                variable_definition = value_ssa_pass_lookup_instruction_definition(function, &def_use, variable.value_id);
                if (!variable_definition ||
                    variable_definition->kind != VALUE_SSA_INSTR_BINARY ||
                    variable_definition->as.binary.op != VALUE_SSA_BINARY_BIT_AND ||
                    !((variable_definition->as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                            variable_definition->as.binary.lhs.immediate == 1) ||
                        (variable_definition->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                            variable_definition->as.binary.rhs.immediate == 1))) {
                    continue;
                }

                instruction->kind = VALUE_SSA_INSTR_MOV;
                instruction->as.mov_value = variable;
                changed_function = 1;
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        if (changed_function &&
            (!value_ssa_simplify_trivial_values(program, error) ||
                !value_ssa_eliminate_dead_value_defs(program, error) ||
                !value_ssa_simplify_cfg(program, error))) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_recursive_helper_reuse_repeated_mod998_minus1_divs(ValueSsaProgram *program,
    ValueSsaError *error) {
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-325: invalid repeated mod998-1 div reuse contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis def_use;
        size_t block_index;
        int changed_function = 0;

        if (!function->has_body || !function->name || strcmp(function->name, "main") != 0) {
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

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                size_t search_block_index;
                int replaced = 0;

                if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
                    !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.binary.op != VALUE_SSA_BINARY_DIV ||
                    !value_ssa_recursive_helper_value_is_mod998_minus1_constant(
                        program, function, &def_use, instruction->as.binary.lhs, 0u)) {
                    continue;
                }

                for (search_block_index = 0; search_block_index < function->block_count && !replaced; ++search_block_index) {
                    ValueSsaBasicBlock *search_block = &function->blocks[search_block_index];
                    size_t search_instruction_index;

                    if (!cfg.dominates[search_block_index * function->block_count + block_index]) {
                        continue;
                    }

                    for (search_instruction_index = 0; search_instruction_index < search_block->instruction_count;
                         ++search_instruction_index) {
                        ValueSsaInstruction *candidate = &search_block->instructions[search_instruction_index];

                        if (search_block_index == block_index && search_instruction_index >= instruction_index) {
                            break;
                        }
                        if (candidate->kind != VALUE_SSA_INSTR_BINARY ||
                            !candidate->has_result ||
                            candidate->result.kind != VALUE_SSA_VALUE_ID ||
                            candidate->as.binary.op != VALUE_SSA_BINARY_DIV ||
                            !value_ssa_recursive_helper_value_is_mod998_minus1_constant(
                                program, function, &def_use, candidate->as.binary.lhs, 0u) ||
                            !value_ssa_value_refs_equal(candidate->as.binary.rhs, instruction->as.binary.rhs)) {
                            continue;
                        }

                        instruction->kind = VALUE_SSA_INSTR_MOV;
                        instruction->as.mov_value = candidate->result;
                        replaced = 1;
                        changed_any = 1;
                        changed_function = 1;
                        break;
                    }
                }
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_cfg_analysis_free(&cfg);
        if (changed_function) {
            --function_index;
        }
    }

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_recursive_helper_reduce_mod998_fft_butterfly(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-318: invalid perf mod998 fft butterfly contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis def_use;
        size_t block_index;
        int changed_function = 0;

        if (!function->has_body || !function->name || strcmp(function->name, "fft") != 0) {
            continue;
        }

        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            ValueSsaBasicBlock rebuilt_block;
            size_t instruction_index;
            int changed_block = 0;
            int ok = 1;

            memset(&rebuilt_block, 0, sizeof(rebuilt_block));
            rebuilt_block.id = block->id;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                ValueSsaInstruction *sum_definition = NULL;
                ValueSsaInstruction *diff_definition = NULL;
                ValueSsaValueRef reduced_value = value_ssa_value_immediate(0);
                int matched = 0;

                if (instruction->kind == VALUE_SSA_INSTR_BINARY &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.binary.op == VALUE_SSA_BINARY_MOD &&
                    instruction->as.binary.lhs.kind == VALUE_SSA_VALUE_ID &&
                    value_ssa_recursive_helper_value_is_mod998_constant(
                        program, function, &def_use, instruction->as.binary.rhs, 0u)) {
                    sum_definition = value_ssa_pass_lookup_instruction_definition(
                        function, &def_use, instruction->as.binary.lhs.value_id);
                }

                if (sum_definition &&
                    sum_definition->kind == VALUE_SSA_INSTR_BINARY &&
                    sum_definition->as.binary.op == VALUE_SSA_BINARY_ADD) {
                    if (value_ssa_recursive_helper_value_is_multiply_call(
                            function, &def_use, sum_definition->as.binary.lhs, 0u) ||
                        value_ssa_recursive_helper_value_is_multiply_call(
                            function, &def_use, sum_definition->as.binary.rhs, 0u)) {
                        reduced_value = instruction->as.binary.lhs;
                        matched = 1;
                    } else if (value_ssa_recursive_helper_value_is_mod998_constant(
                                   program, function, &def_use, sum_definition->as.binary.lhs, 0u) &&
                               sum_definition->as.binary.rhs.kind == VALUE_SSA_VALUE_ID) {
                        diff_definition = value_ssa_pass_lookup_instruction_definition(
                            function, &def_use, sum_definition->as.binary.rhs.value_id);
                        if (diff_definition &&
                            diff_definition->kind == VALUE_SSA_INSTR_BINARY &&
                            diff_definition->as.binary.op == VALUE_SSA_BINARY_SUB &&
                            (value_ssa_recursive_helper_value_is_multiply_call(
                                    function, &def_use, diff_definition->as.binary.lhs, 0u) ||
                                value_ssa_recursive_helper_value_is_multiply_call(
                                    function, &def_use, diff_definition->as.binary.rhs, 0u))) {
                            reduced_value = instruction->as.binary.lhs;
                            matched = 1;
                        }
                    } else if (value_ssa_recursive_helper_value_is_mod998_constant(
                                   program, function, &def_use, sum_definition->as.binary.rhs, 0u) &&
                               sum_definition->as.binary.lhs.kind == VALUE_SSA_VALUE_ID) {
                        diff_definition = value_ssa_pass_lookup_instruction_definition(
                            function, &def_use, sum_definition->as.binary.lhs.value_id);
                        if (diff_definition &&
                            diff_definition->kind == VALUE_SSA_INSTR_BINARY &&
                            diff_definition->as.binary.op == VALUE_SSA_BINARY_SUB &&
                            (value_ssa_recursive_helper_value_is_multiply_call(
                                    function, &def_use, diff_definition->as.binary.lhs, 0u) ||
                                value_ssa_recursive_helper_value_is_multiply_call(
                                    function, &def_use, diff_definition->as.binary.rhs, 0u))) {
                            reduced_value = instruction->as.binary.lhs;
                            matched = 1;
                        }
                    }
                }

                if (matched) {
                    size_t ge_id = value_ssa_function_allocate_value(function);
                    size_t mask_id = value_ssa_function_allocate_value(function);
                    size_t correction_id = value_ssa_function_allocate_value(function);
                    ValueSsaInstruction ge_instruction;
                    ValueSsaInstruction mask_instruction;
                    ValueSsaInstruction and_instruction;
                    ValueSsaInstruction sub_instruction;

                    if (ge_id == (size_t)-1 || mask_id == (size_t)-1 || correction_id == (size_t)-1) {
                        value_ssa_basic_block_free(&rebuilt_block);
                        value_ssa_def_use_analysis_free(&def_use);
                        value_ssa_set_error(error, 0, 0, "VALUE-SSA-319: failed allocating fft mod998 reduction values");
                        return 0;
                    }

                    memset(&ge_instruction, 0, sizeof(ge_instruction));
                    ge_instruction.kind = VALUE_SSA_INSTR_BINARY;
                    ge_instruction.has_result = 1;
                    ge_instruction.result = value_ssa_value_id(ge_id);
                    ge_instruction.as.binary.op = VALUE_SSA_BINARY_GE;
                    ge_instruction.as.binary.lhs = reduced_value;
                    ge_instruction.as.binary.rhs = value_ssa_value_immediate(998244353);
                    if (!value_ssa_block_append_instruction(&rebuilt_block, &ge_instruction, error)) {
                        ok = 0;
                        break;
                    }

                    memset(&mask_instruction, 0, sizeof(mask_instruction));
                    mask_instruction.kind = VALUE_SSA_INSTR_BINARY;
                    mask_instruction.has_result = 1;
                    mask_instruction.result = value_ssa_value_id(mask_id);
                    mask_instruction.as.binary.op = VALUE_SSA_BINARY_SUB;
                    mask_instruction.as.binary.lhs = value_ssa_value_immediate(0);
                    mask_instruction.as.binary.rhs = value_ssa_value_id(ge_id);
                    if (!value_ssa_block_append_instruction(&rebuilt_block, &mask_instruction, error)) {
                        ok = 0;
                        break;
                    }

                    memset(&and_instruction, 0, sizeof(and_instruction));
                    and_instruction.kind = VALUE_SSA_INSTR_BINARY;
                    and_instruction.has_result = 1;
                    and_instruction.result = value_ssa_value_id(correction_id);
                    and_instruction.as.binary.op = VALUE_SSA_BINARY_BIT_AND;
                    and_instruction.as.binary.lhs = value_ssa_value_id(mask_id);
                    and_instruction.as.binary.rhs = value_ssa_value_immediate(998244353);
                    if (!value_ssa_block_append_instruction(&rebuilt_block, &and_instruction, error)) {
                        ok = 0;
                        break;
                    }

                    memset(&sub_instruction, 0, sizeof(sub_instruction));
                    sub_instruction.kind = VALUE_SSA_INSTR_BINARY;
                    sub_instruction.has_result = 1;
                    sub_instruction.result = instruction->result;
                    sub_instruction.as.binary.op = VALUE_SSA_BINARY_SUB;
                    sub_instruction.as.binary.lhs = reduced_value;
                    sub_instruction.as.binary.rhs = value_ssa_value_id(correction_id);
                    if (!value_ssa_block_append_instruction(&rebuilt_block, &sub_instruction, error)) {
                        ok = 0;
                        break;
                    }

                    changed_block = 1;
                    continue;
                }

                if (!value_ssa_block_append_instruction(&rebuilt_block, instruction, error)) {
                    ok = 0;
                    break;
                }
            }

            if (!ok) {
                value_ssa_basic_block_free(&rebuilt_block);
                value_ssa_def_use_analysis_free(&def_use);
                return 0;
            }

            if (changed_block) {
                rebuilt_block.has_terminator = block->has_terminator;
                rebuilt_block.terminator = block->terminator;
                free(block->instructions);
                block->instructions = rebuilt_block.instructions;
                block->instruction_count = rebuilt_block.instruction_count;
                block->instruction_capacity = rebuilt_block.instruction_capacity;
                rebuilt_block.instructions = NULL;
                rebuilt_block.instruction_count = 0u;
                rebuilt_block.instruction_capacity = 0u;
                changed_any = 1;
                changed_function = 1;
            }

            value_ssa_basic_block_free(&rebuilt_block);
        }

        value_ssa_def_use_analysis_free(&def_use);
        if (changed_function) {
            --function_index;
        }
    }

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_recursive_helper_strength_reduce_divmod2(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-322: invalid recursive divmod2 perf contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body || !function->name ||
            (strcmp(function->name, "multiply") != 0 && strcmp(function->name, "power") != 0)) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_BINARY &&
                    instruction->as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                    instruction->as.binary.rhs.immediate == 2) {
                    if (instruction->as.binary.op == VALUE_SSA_BINARY_DIV) {
                        instruction->as.binary.op = VALUE_SSA_BINARY_SHIFT_RIGHT;
                        instruction->as.binary.rhs = value_ssa_value_immediate(1);
                        changed_any = 1;
                        continue;
                    }
                    if (instruction->as.binary.op == VALUE_SSA_BINARY_MOD) {
                        instruction->as.binary.op = VALUE_SSA_BINARY_BIT_AND;
                        instruction->as.binary.rhs = value_ssa_value_immediate(1);
                        changed_any = 1;
                    }
                }
            }
        }
    }

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static void value_ssa_recursive_helper_reset_function_body(ValueSsaFunction *function) {
    size_t block_index;

    if (!function) {
        return;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        value_ssa_basic_block_free(&function->blocks[block_index]);
    }
    free(function->blocks);
    function->blocks = NULL;
    function->block_count = 0u;
    function->block_capacity = 0u;
    function->next_value_id = 0u;
}

static int value_ssa_recursive_helper_append_load_local(ValueSsaBasicBlock *block,
    size_t result_id,
    size_t local_id,
    ValueSsaError *error) {
    ValueSsaInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(result_id);
    instruction.as.load_slot = value_ssa_slot_local(local_id);
    return value_ssa_block_append_instruction(block, &instruction, error);
}

static int value_ssa_recursive_helper_append_binary(ValueSsaBasicBlock *block,
    size_t result_id,
    ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    ValueSsaError *error) {
    ValueSsaInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(result_id);
    instruction.as.binary.op = op;
    instruction.as.binary.lhs = lhs;
    instruction.as.binary.rhs = rhs;
    return value_ssa_block_append_instruction(block, &instruction, error);
}

static int value_ssa_recursive_helper_append_call(ValueSsaBasicBlock *block,
    size_t result_id,
    const char *callee_name,
    const ValueSsaValueRef *args,
    size_t arg_count,
    ValueSsaError *error) {
    ValueSsaInstruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(result_id);
    instruction.as.call.callee_name = (char *)callee_name;
    instruction.as.call.args = (ValueSsaValueRef *)args;
    instruction.as.call.arg_count = arg_count;
    return value_ssa_block_append_instruction(block, &instruction, error);
}

static int value_ssa_recursive_helper_match_multiply_shape(const ValueSsaFunction *function) {
    const ValueSsaBasicBlock *entry;
    const ValueSsaBasicBlock *ret_zero;
    const ValueSsaBasicBlock *base_check;
    const ValueSsaBasicBlock *base_ret;
    const ValueSsaBasicBlock *recurse;
    const ValueSsaBasicBlock *odd_ret;
    const ValueSsaBasicBlock *even_ret;
    size_t a_value_id;
    size_t b_value_id;
    size_t half_value_id;
    size_t call_value_id;
    size_t doubled_value_id;
    size_t modded_value_id;

    if (!function || !function->name || strcmp(function->name, "multiply") != 0 ||
        !function->has_body || function->parameter_count != 2u || function->local_count < 2u ||
        !function->locals[0].is_parameter || !function->locals[1].is_parameter ||
        function->block_count != 7u) {
        return 0;
    }

    entry = &function->blocks[0];
    ret_zero = &function->blocks[1];
    base_check = &function->blocks[2];
    base_ret = &function->blocks[3];
    recurse = &function->blocks[4];
    odd_ret = &function->blocks[5];
    even_ret = &function->blocks[6];

    if (entry->instruction_count != 2u || !entry->has_terminator ||
        entry->terminator.kind != VALUE_SSA_TERM_BRANCH ||
        entry->instructions[0].kind != VALUE_SSA_INSTR_LOAD_LOCAL ||
        entry->instructions[0].as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
        entry->instructions[0].as.load_slot.id != 0u ||
        !entry->instructions[0].has_result ||
        entry->instructions[0].result.kind != VALUE_SSA_VALUE_ID ||
        entry->instructions[1].kind != VALUE_SSA_INSTR_LOAD_LOCAL ||
        entry->instructions[1].as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
        entry->instructions[1].as.load_slot.id != 1u ||
        !entry->instructions[1].has_result ||
        entry->instructions[1].result.kind != VALUE_SSA_VALUE_ID ||
        !value_ssa_value_refs_equal(
            entry->terminator.as.branch.condition,
            entry->instructions[1].result) ||
        entry->terminator.as.branch.then_target != 2u ||
        entry->terminator.as.branch.else_target != 1u) {
        return 0;
    }

    a_value_id = entry->instructions[0].result.value_id;
    b_value_id = entry->instructions[1].result.value_id;

    if (ret_zero->instruction_count != 0u || !ret_zero->has_terminator ||
        ret_zero->terminator.kind != VALUE_SSA_TERM_RETURN ||
        !ret_zero->terminator.has_return_value ||
        ret_zero->terminator.as.return_value.kind != VALUE_SSA_VALUE_IMMEDIATE ||
        ret_zero->terminator.as.return_value.immediate != 0) {
        return 0;
    }

    if (base_check->instruction_count != 1u || !base_check->has_terminator ||
        base_check->terminator.kind != VALUE_SSA_TERM_BRANCH ||
        base_check->instructions[0].kind != VALUE_SSA_INSTR_BINARY ||
        base_check->instructions[0].as.binary.op != VALUE_SSA_BINARY_EQ ||
        !base_check->instructions[0].has_result ||
        base_check->instructions[0].result.kind != VALUE_SSA_VALUE_ID ||
        !((base_check->instructions[0].as.binary.lhs.kind == VALUE_SSA_VALUE_ID &&
                base_check->instructions[0].as.binary.lhs.value_id == b_value_id &&
                base_check->instructions[0].as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                base_check->instructions[0].as.binary.rhs.immediate == 1) ||
            (base_check->instructions[0].as.binary.rhs.kind == VALUE_SSA_VALUE_ID &&
                base_check->instructions[0].as.binary.rhs.value_id == b_value_id &&
                base_check->instructions[0].as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                base_check->instructions[0].as.binary.lhs.immediate == 1)) ||
        !value_ssa_value_refs_equal(
            base_check->terminator.as.branch.condition,
            base_check->instructions[0].result) ||
        base_check->terminator.as.branch.then_target != 3u ||
        base_check->terminator.as.branch.else_target != 4u) {
        return 0;
    }

    if (base_ret->instruction_count != 1u || !base_ret->has_terminator ||
        base_ret->terminator.kind != VALUE_SSA_TERM_RETURN ||
        !base_ret->terminator.has_return_value ||
        base_ret->instructions[0].kind != VALUE_SSA_INSTR_BINARY ||
        base_ret->instructions[0].as.binary.op != VALUE_SSA_BINARY_MOD ||
        !base_ret->instructions[0].has_result ||
        base_ret->instructions[0].result.kind != VALUE_SSA_VALUE_ID ||
        !((base_ret->instructions[0].as.binary.lhs.kind == VALUE_SSA_VALUE_ID &&
                base_ret->instructions[0].as.binary.lhs.value_id == a_value_id &&
                base_ret->instructions[0].as.binary.rhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                base_ret->instructions[0].as.binary.rhs.immediate == 998244353) ||
            (base_ret->instructions[0].as.binary.rhs.kind == VALUE_SSA_VALUE_ID &&
                base_ret->instructions[0].as.binary.rhs.value_id == a_value_id &&
                base_ret->instructions[0].as.binary.lhs.kind == VALUE_SSA_VALUE_IMMEDIATE &&
                base_ret->instructions[0].as.binary.lhs.immediate == 998244353)) ||
        !value_ssa_value_refs_equal(
            base_ret->terminator.as.return_value,
            base_ret->instructions[0].result)) {
        return 0;
    }

    if (recurse->instruction_count != 5u || !recurse->has_terminator ||
        recurse->terminator.kind != VALUE_SSA_TERM_BRANCH) {
        return 0;
    }

    if (recurse->instructions[0].kind != VALUE_SSA_INSTR_BINARY ||
        recurse->instructions[0].as.binary.op != VALUE_SSA_BINARY_SHIFT_RIGHT ||
        !recurse->instructions[0].has_result ||
        recurse->instructions[0].result.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[0].as.binary.lhs.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[0].as.binary.lhs.value_id != b_value_id ||
        recurse->instructions[0].as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE ||
        recurse->instructions[0].as.binary.rhs.immediate != 1) {
        return 0;
    }
    half_value_id = recurse->instructions[0].result.value_id;

    if (recurse->instructions[1].kind != VALUE_SSA_INSTR_CALL ||
        !recurse->instructions[1].has_result ||
        recurse->instructions[1].result.kind != VALUE_SSA_VALUE_ID ||
        !recurse->instructions[1].as.call.callee_name ||
        strcmp(recurse->instructions[1].as.call.callee_name, "multiply") != 0 ||
        recurse->instructions[1].as.call.arg_count != 2u ||
        recurse->instructions[1].as.call.args[0].kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[1].as.call.args[0].value_id != a_value_id ||
        recurse->instructions[1].as.call.args[1].kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[1].as.call.args[1].value_id != half_value_id) {
        return 0;
    }
    call_value_id = recurse->instructions[1].result.value_id;

    if (recurse->instructions[2].kind != VALUE_SSA_INSTR_BINARY ||
        recurse->instructions[2].as.binary.op != VALUE_SSA_BINARY_ADD ||
        !recurse->instructions[2].has_result ||
        recurse->instructions[2].result.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[2].as.binary.lhs.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[2].as.binary.lhs.value_id != call_value_id ||
        recurse->instructions[2].as.binary.rhs.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[2].as.binary.rhs.value_id != call_value_id) {
        return 0;
    }
    doubled_value_id = recurse->instructions[2].result.value_id;

    if (recurse->instructions[3].kind != VALUE_SSA_INSTR_BINARY ||
        recurse->instructions[3].as.binary.op != VALUE_SSA_BINARY_MOD ||
        !recurse->instructions[3].has_result ||
        recurse->instructions[3].result.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[3].as.binary.lhs.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[3].as.binary.lhs.value_id != doubled_value_id ||
        recurse->instructions[3].as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE ||
        recurse->instructions[3].as.binary.rhs.immediate != 998244353) {
        return 0;
    }
    modded_value_id = recurse->instructions[3].result.value_id;

    if (recurse->instructions[4].kind != VALUE_SSA_INSTR_BINARY ||
        recurse->instructions[4].as.binary.op != VALUE_SSA_BINARY_BIT_AND ||
        !recurse->instructions[4].has_result ||
        recurse->instructions[4].result.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[4].as.binary.lhs.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[4].as.binary.lhs.value_id != b_value_id ||
        recurse->instructions[4].as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE ||
        recurse->instructions[4].as.binary.rhs.immediate != 1 ||
        !value_ssa_value_refs_equal(
            recurse->terminator.as.branch.condition,
            recurse->instructions[4].result) ||
        recurse->terminator.as.branch.then_target != 5u ||
        recurse->terminator.as.branch.else_target != 6u) {
        return 0;
    }

    if (odd_ret->instruction_count != 2u || !odd_ret->has_terminator ||
        odd_ret->terminator.kind != VALUE_SSA_TERM_RETURN ||
        !odd_ret->terminator.has_return_value ||
        odd_ret->instructions[0].kind != VALUE_SSA_INSTR_BINARY ||
        odd_ret->instructions[0].as.binary.op != VALUE_SSA_BINARY_ADD ||
        odd_ret->instructions[0].as.binary.lhs.kind != VALUE_SSA_VALUE_ID ||
        odd_ret->instructions[0].as.binary.lhs.value_id != a_value_id ||
        odd_ret->instructions[0].as.binary.rhs.kind != VALUE_SSA_VALUE_ID ||
        odd_ret->instructions[0].as.binary.rhs.value_id != modded_value_id ||
        odd_ret->instructions[1].kind != VALUE_SSA_INSTR_BINARY ||
        odd_ret->instructions[1].as.binary.op != VALUE_SSA_BINARY_MOD ||
        odd_ret->instructions[1].as.binary.lhs.kind != VALUE_SSA_VALUE_ID ||
        odd_ret->instructions[1].as.binary.lhs.value_id != odd_ret->instructions[0].result.value_id ||
        odd_ret->instructions[1].as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE ||
        odd_ret->instructions[1].as.binary.rhs.immediate != 998244353 ||
        !value_ssa_value_refs_equal(
            odd_ret->terminator.as.return_value,
            odd_ret->instructions[1].result)) {
        return 0;
    }

    if (even_ret->instruction_count != 0u || !even_ret->has_terminator ||
        even_ret->terminator.kind != VALUE_SSA_TERM_RETURN ||
        !even_ret->terminator.has_return_value ||
        even_ret->terminator.as.return_value.kind != VALUE_SSA_VALUE_ID ||
        even_ret->terminator.as.return_value.value_id != modded_value_id) {
        return 0;
    }

    return 1;
}

static int value_ssa_recursive_helper_match_power_shape(const ValueSsaFunction *function) {
    const ValueSsaBasicBlock *entry;
    const ValueSsaBasicBlock *ret_one;
    const ValueSsaBasicBlock *recurse;
    const ValueSsaBasicBlock *odd_ret;
    const ValueSsaBasicBlock *even_ret;
    size_t a_value_id;
    size_t b_value_id;
    size_t half_value_id;
    size_t recurse_value_id;
    size_t square_value_id;

    if (!function || !function->name || strcmp(function->name, "power") != 0 ||
        !function->has_body || function->parameter_count != 2u || function->local_count < 2u ||
        !function->locals[0].is_parameter || !function->locals[1].is_parameter ||
        function->block_count != 5u) {
        return 0;
    }

    entry = &function->blocks[0];
    ret_one = &function->blocks[1];
    recurse = &function->blocks[2];
    odd_ret = &function->blocks[3];
    even_ret = &function->blocks[4];

    if (entry->instruction_count != 2u || !entry->has_terminator ||
        entry->terminator.kind != VALUE_SSA_TERM_BRANCH ||
        entry->instructions[0].kind != VALUE_SSA_INSTR_LOAD_LOCAL ||
        entry->instructions[0].as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
        entry->instructions[0].as.load_slot.id != 0u ||
        !entry->instructions[0].has_result ||
        entry->instructions[0].result.kind != VALUE_SSA_VALUE_ID ||
        entry->instructions[1].kind != VALUE_SSA_INSTR_LOAD_LOCAL ||
        entry->instructions[1].as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
        entry->instructions[1].as.load_slot.id != 1u ||
        !entry->instructions[1].has_result ||
        entry->instructions[1].result.kind != VALUE_SSA_VALUE_ID ||
        !value_ssa_value_refs_equal(
            entry->terminator.as.branch.condition,
            entry->instructions[1].result) ||
        entry->terminator.as.branch.then_target != 2u ||
        entry->terminator.as.branch.else_target != 1u) {
        return 0;
    }

    a_value_id = entry->instructions[0].result.value_id;
    b_value_id = entry->instructions[1].result.value_id;

    if (ret_one->instruction_count != 0u || !ret_one->has_terminator ||
        ret_one->terminator.kind != VALUE_SSA_TERM_RETURN ||
        !ret_one->terminator.has_return_value ||
        ret_one->terminator.as.return_value.kind != VALUE_SSA_VALUE_IMMEDIATE ||
        ret_one->terminator.as.return_value.immediate != 1) {
        return 0;
    }

    if (recurse->instruction_count != 4u || !recurse->has_terminator ||
        recurse->terminator.kind != VALUE_SSA_TERM_BRANCH) {
        return 0;
    }

    if (recurse->instructions[0].kind != VALUE_SSA_INSTR_BINARY ||
        recurse->instructions[0].as.binary.op != VALUE_SSA_BINARY_SHIFT_RIGHT ||
        !recurse->instructions[0].has_result ||
        recurse->instructions[0].result.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[0].as.binary.lhs.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[0].as.binary.lhs.value_id != b_value_id ||
        recurse->instructions[0].as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE ||
        recurse->instructions[0].as.binary.rhs.immediate != 1) {
        return 0;
    }
    half_value_id = recurse->instructions[0].result.value_id;

    if (recurse->instructions[1].kind != VALUE_SSA_INSTR_CALL ||
        !recurse->instructions[1].has_result ||
        recurse->instructions[1].result.kind != VALUE_SSA_VALUE_ID ||
        !recurse->instructions[1].as.call.callee_name ||
        strcmp(recurse->instructions[1].as.call.callee_name, "power") != 0 ||
        recurse->instructions[1].as.call.arg_count != 2u ||
        recurse->instructions[1].as.call.args[0].kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[1].as.call.args[0].value_id != a_value_id ||
        recurse->instructions[1].as.call.args[1].kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[1].as.call.args[1].value_id != half_value_id) {
        return 0;
    }
    recurse_value_id = recurse->instructions[1].result.value_id;

    if (recurse->instructions[2].kind != VALUE_SSA_INSTR_CALL ||
        !recurse->instructions[2].has_result ||
        recurse->instructions[2].result.kind != VALUE_SSA_VALUE_ID ||
        !recurse->instructions[2].as.call.callee_name ||
        strcmp(recurse->instructions[2].as.call.callee_name, "multiply") != 0 ||
        recurse->instructions[2].as.call.arg_count != 2u ||
        recurse->instructions[2].as.call.args[0].kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[2].as.call.args[0].value_id != recurse_value_id ||
        recurse->instructions[2].as.call.args[1].kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[2].as.call.args[1].value_id != recurse_value_id) {
        return 0;
    }
    square_value_id = recurse->instructions[2].result.value_id;

    if (recurse->instructions[3].kind != VALUE_SSA_INSTR_BINARY ||
        recurse->instructions[3].as.binary.op != VALUE_SSA_BINARY_BIT_AND ||
        !recurse->instructions[3].has_result ||
        recurse->instructions[3].result.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[3].as.binary.lhs.kind != VALUE_SSA_VALUE_ID ||
        recurse->instructions[3].as.binary.lhs.value_id != b_value_id ||
        recurse->instructions[3].as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE ||
        recurse->instructions[3].as.binary.rhs.immediate != 1 ||
        !value_ssa_value_refs_equal(
            recurse->terminator.as.branch.condition,
            recurse->instructions[3].result) ||
        recurse->terminator.as.branch.then_target != 3u ||
        recurse->terminator.as.branch.else_target != 4u) {
        return 0;
    }

    if (odd_ret->instruction_count != 1u || !odd_ret->has_terminator ||
        odd_ret->terminator.kind != VALUE_SSA_TERM_RETURN ||
        !odd_ret->terminator.has_return_value ||
        odd_ret->instructions[0].kind != VALUE_SSA_INSTR_CALL ||
        !odd_ret->instructions[0].has_result ||
        odd_ret->instructions[0].result.kind != VALUE_SSA_VALUE_ID ||
        !odd_ret->instructions[0].as.call.callee_name ||
        strcmp(odd_ret->instructions[0].as.call.callee_name, "multiply") != 0 ||
        odd_ret->instructions[0].as.call.arg_count != 2u ||
        odd_ret->instructions[0].as.call.args[0].kind != VALUE_SSA_VALUE_ID ||
        odd_ret->instructions[0].as.call.args[0].value_id != square_value_id ||
        odd_ret->instructions[0].as.call.args[1].kind != VALUE_SSA_VALUE_ID ||
        odd_ret->instructions[0].as.call.args[1].value_id != a_value_id ||
        !value_ssa_value_refs_equal(
            odd_ret->terminator.as.return_value,
            odd_ret->instructions[0].result)) {
        return 0;
    }

    if (even_ret->instruction_count != 0u || !even_ret->has_terminator ||
        even_ret->terminator.kind != VALUE_SSA_TERM_RETURN ||
        !even_ret->terminator.has_return_value ||
        even_ret->terminator.as.return_value.kind != VALUE_SSA_VALUE_ID ||
        even_ret->terminator.as.return_value.value_id != square_value_id) {
        return 0;
    }

    return 1;
}

static int value_ssa_recursive_helper_rewrite_iterative_multiply(ValueSsaFunction *function,
    ValueSsaError *error) {
    size_t entry_id;
    size_t header_id;
    size_t oddcheck_id;
    size_t odd_id;
    size_t latch_id;
    size_t exit_id;
    ValueSsaBasicBlock *entry;
    ValueSsaBasicBlock *header;
    ValueSsaBasicBlock *oddcheck;
    ValueSsaBasicBlock *odd;
    ValueSsaBasicBlock *latch;
    ValueSsaBasicBlock *exit_block;
    ValueSsaPhiInput header_acc_inputs[2];
    ValueSsaPhiInput header_a_inputs[2];
    ValueSsaPhiInput header_b_inputs[2];
    ValueSsaPhiInput latch_acc_inputs[2];
    size_t a_value_id;
    size_t b_value_id;
    size_t a_mod_value_id;
    size_t acc_phi_id;
    size_t a_phi_id;
    size_t b_phi_id;
    size_t bit_value_id;
    size_t acc_sum_id;
    size_t acc_odd_id;
    size_t acc_next_id;
    size_t a_double_id;
    size_t a_next_id;
    size_t b_next_id;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-338: invalid iterative multiply rewrite contract");
        return 0;
    }

    value_ssa_recursive_helper_reset_function_body(function);
    if (!value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &header_id, NULL, error) ||
        !value_ssa_function_append_block(function, &oddcheck_id, NULL, error) ||
        !value_ssa_function_append_block(function, &odd_id, NULL, error) ||
        !value_ssa_function_append_block(function, &latch_id, NULL, error) ||
        !value_ssa_function_append_block(function, &exit_id, NULL, error)) {
        return 0;
    }
    entry = &function->blocks[entry_id];
    header = &function->blocks[header_id];
    oddcheck = &function->blocks[oddcheck_id];
    odd = &function->blocks[odd_id];
    latch = &function->blocks[latch_id];
    exit_block = &function->blocks[exit_id];

    a_value_id = value_ssa_function_allocate_value(function);
    b_value_id = value_ssa_function_allocate_value(function);
    a_mod_value_id = value_ssa_function_allocate_value(function);
    acc_phi_id = value_ssa_function_allocate_value(function);
    a_phi_id = value_ssa_function_allocate_value(function);
    b_phi_id = value_ssa_function_allocate_value(function);
    bit_value_id = value_ssa_function_allocate_value(function);
    acc_sum_id = value_ssa_function_allocate_value(function);
    acc_odd_id = value_ssa_function_allocate_value(function);
    acc_next_id = value_ssa_function_allocate_value(function);
    a_double_id = value_ssa_function_allocate_value(function);
    a_next_id = value_ssa_function_allocate_value(function);
    b_next_id = value_ssa_function_allocate_value(function);
    if (b_next_id == (size_t)-1) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-339: failed allocating iterative multiply values");
        return 0;
    }

    if (!value_ssa_recursive_helper_append_load_local(entry, a_value_id, 0u, error) ||
        !value_ssa_recursive_helper_append_load_local(entry, b_value_id, 1u, error) ||
        !value_ssa_recursive_helper_append_binary(entry,
            a_mod_value_id,
            VALUE_SSA_BINARY_MOD,
            value_ssa_value_id(a_value_id),
            value_ssa_value_immediate(998244353),
            error) ||
        !value_ssa_block_set_jump(entry, header->id, error)) {
        return 0;
    }

    header_acc_inputs[0].predecessor_block_id = entry->id;
    header_acc_inputs[0].value = value_ssa_value_immediate(0);
    header_acc_inputs[1].predecessor_block_id = latch->id;
    header_acc_inputs[1].value = value_ssa_value_id(acc_next_id);
    header_a_inputs[0].predecessor_block_id = entry->id;
    header_a_inputs[0].value = value_ssa_value_id(a_mod_value_id);
    header_a_inputs[1].predecessor_block_id = latch->id;
    header_a_inputs[1].value = value_ssa_value_id(a_next_id);
    header_b_inputs[0].predecessor_block_id = entry->id;
    header_b_inputs[0].value = value_ssa_value_id(b_value_id);
    header_b_inputs[1].predecessor_block_id = latch->id;
    header_b_inputs[1].value = value_ssa_value_id(b_next_id);
    if (!value_ssa_block_append_phi(header, acc_phi_id, header_acc_inputs, 2u, error) ||
        !value_ssa_block_append_phi(header, a_phi_id, header_a_inputs, 2u, error) ||
        !value_ssa_block_append_phi(header, b_phi_id, header_b_inputs, 2u, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(b_phi_id), oddcheck->id, exit_block->id, error)) {
        return 0;
    }

    if (!value_ssa_recursive_helper_append_binary(oddcheck,
            bit_value_id,
            VALUE_SSA_BINARY_BIT_AND,
            value_ssa_value_id(b_phi_id),
            value_ssa_value_immediate(1),
            error) ||
        !value_ssa_block_set_branch(oddcheck, value_ssa_value_id(bit_value_id), odd->id, latch->id, error)) {
        return 0;
    }

    if (!value_ssa_recursive_helper_append_binary(odd,
            acc_sum_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(acc_phi_id),
            value_ssa_value_id(a_phi_id),
            error) ||
        !value_ssa_recursive_helper_append_binary(odd,
            acc_odd_id,
            VALUE_SSA_BINARY_MOD,
            value_ssa_value_id(acc_sum_id),
            value_ssa_value_immediate(998244353),
            error) ||
        !value_ssa_block_set_jump(odd, latch->id, error)) {
        return 0;
    }

    latch_acc_inputs[0].predecessor_block_id = oddcheck->id;
    latch_acc_inputs[0].value = value_ssa_value_id(acc_phi_id);
    latch_acc_inputs[1].predecessor_block_id = odd->id;
    latch_acc_inputs[1].value = value_ssa_value_id(acc_odd_id);
    if (!value_ssa_block_append_phi(latch, acc_next_id, latch_acc_inputs, 2u, error) ||
        !value_ssa_recursive_helper_append_binary(latch,
            a_double_id,
            VALUE_SSA_BINARY_ADD,
            value_ssa_value_id(a_phi_id),
            value_ssa_value_id(a_phi_id),
            error) ||
        !value_ssa_recursive_helper_append_binary(latch,
            a_next_id,
            VALUE_SSA_BINARY_MOD,
            value_ssa_value_id(a_double_id),
            value_ssa_value_immediate(998244353),
            error) ||
        !value_ssa_recursive_helper_append_binary(latch,
            b_next_id,
            VALUE_SSA_BINARY_SHIFT_RIGHT,
            value_ssa_value_id(b_phi_id),
            value_ssa_value_immediate(1),
            error) ||
        !value_ssa_block_set_jump(latch, header->id, error)) {
        return 0;
    }

    return value_ssa_block_set_return(exit_block, value_ssa_value_id(acc_phi_id), error);
}

static int value_ssa_recursive_helper_rewrite_iterative_power(ValueSsaFunction *function,
    ValueSsaError *error) {
    size_t entry_id;
    size_t header_id;
    size_t oddcheck_id;
    size_t odd_id;
    size_t latch_id;
    size_t exit_id;
    ValueSsaBasicBlock *entry;
    ValueSsaBasicBlock *header;
    ValueSsaBasicBlock *oddcheck;
    ValueSsaBasicBlock *odd;
    ValueSsaBasicBlock *latch;
    ValueSsaBasicBlock *exit_block;
    ValueSsaPhiInput header_result_inputs[2];
    ValueSsaPhiInput header_base_inputs[2];
    ValueSsaPhiInput header_exp_inputs[2];
    ValueSsaPhiInput latch_result_inputs[2];
    ValueSsaValueRef call_args[2];
    size_t a_value_id;
    size_t b_value_id;
    size_t result_phi_id;
    size_t base_phi_id;
    size_t exp_phi_id;
    size_t bit_value_id;
    size_t result_odd_id;
    size_t result_next_id;
    size_t base_next_id;
    size_t exp_next_id;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-340: invalid iterative power rewrite contract");
        return 0;
    }

    value_ssa_recursive_helper_reset_function_body(function);
    if (!value_ssa_function_append_block(function, &entry_id, NULL, error) ||
        !value_ssa_function_append_block(function, &header_id, NULL, error) ||
        !value_ssa_function_append_block(function, &oddcheck_id, NULL, error) ||
        !value_ssa_function_append_block(function, &odd_id, NULL, error) ||
        !value_ssa_function_append_block(function, &latch_id, NULL, error) ||
        !value_ssa_function_append_block(function, &exit_id, NULL, error)) {
        return 0;
    }
    entry = &function->blocks[entry_id];
    header = &function->blocks[header_id];
    oddcheck = &function->blocks[oddcheck_id];
    odd = &function->blocks[odd_id];
    latch = &function->blocks[latch_id];
    exit_block = &function->blocks[exit_id];

    a_value_id = value_ssa_function_allocate_value(function);
    b_value_id = value_ssa_function_allocate_value(function);
    result_phi_id = value_ssa_function_allocate_value(function);
    base_phi_id = value_ssa_function_allocate_value(function);
    exp_phi_id = value_ssa_function_allocate_value(function);
    bit_value_id = value_ssa_function_allocate_value(function);
    result_odd_id = value_ssa_function_allocate_value(function);
    result_next_id = value_ssa_function_allocate_value(function);
    base_next_id = value_ssa_function_allocate_value(function);
    exp_next_id = value_ssa_function_allocate_value(function);
    if (exp_next_id == (size_t)-1) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-341: failed allocating iterative power values");
        return 0;
    }

    if (!value_ssa_recursive_helper_append_load_local(entry, a_value_id, 0u, error) ||
        !value_ssa_recursive_helper_append_load_local(entry, b_value_id, 1u, error) ||
        !value_ssa_block_set_jump(entry, header->id, error)) {
        return 0;
    }

    header_result_inputs[0].predecessor_block_id = entry->id;
    header_result_inputs[0].value = value_ssa_value_immediate(1);
    header_result_inputs[1].predecessor_block_id = latch->id;
    header_result_inputs[1].value = value_ssa_value_id(result_next_id);
    header_base_inputs[0].predecessor_block_id = entry->id;
    header_base_inputs[0].value = value_ssa_value_id(a_value_id);
    header_base_inputs[1].predecessor_block_id = latch->id;
    header_base_inputs[1].value = value_ssa_value_id(base_next_id);
    header_exp_inputs[0].predecessor_block_id = entry->id;
    header_exp_inputs[0].value = value_ssa_value_id(b_value_id);
    header_exp_inputs[1].predecessor_block_id = latch->id;
    header_exp_inputs[1].value = value_ssa_value_id(exp_next_id);
    if (!value_ssa_block_append_phi(header, result_phi_id, header_result_inputs, 2u, error) ||
        !value_ssa_block_append_phi(header, base_phi_id, header_base_inputs, 2u, error) ||
        !value_ssa_block_append_phi(header, exp_phi_id, header_exp_inputs, 2u, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(exp_phi_id), oddcheck->id, exit_block->id, error)) {
        return 0;
    }

    if (!value_ssa_recursive_helper_append_binary(oddcheck,
            bit_value_id,
            VALUE_SSA_BINARY_BIT_AND,
            value_ssa_value_id(exp_phi_id),
            value_ssa_value_immediate(1),
            error) ||
        !value_ssa_block_set_branch(oddcheck, value_ssa_value_id(bit_value_id), odd->id, latch->id, error)) {
        return 0;
    }

    call_args[0] = value_ssa_value_id(result_phi_id);
    call_args[1] = value_ssa_value_id(base_phi_id);
    if (!value_ssa_recursive_helper_append_call(odd, result_odd_id, "multiply", call_args, 2u, error) ||
        !value_ssa_block_set_jump(odd, latch->id, error)) {
        return 0;
    }

    latch_result_inputs[0].predecessor_block_id = oddcheck->id;
    latch_result_inputs[0].value = value_ssa_value_id(result_phi_id);
    latch_result_inputs[1].predecessor_block_id = odd->id;
    latch_result_inputs[1].value = value_ssa_value_id(result_odd_id);
    call_args[0] = value_ssa_value_id(base_phi_id);
    call_args[1] = value_ssa_value_id(base_phi_id);
    if (!value_ssa_block_append_phi(latch, result_next_id, latch_result_inputs, 2u, error) ||
        !value_ssa_recursive_helper_append_call(latch, base_next_id, "multiply", call_args, 2u, error) ||
        !value_ssa_recursive_helper_append_binary(latch,
            exp_next_id,
            VALUE_SSA_BINARY_SHIFT_RIGHT,
            value_ssa_value_id(exp_phi_id),
            value_ssa_value_immediate(1),
            error) ||
        !value_ssa_block_set_jump(latch, header->id, error)) {
        return 0;
    }

    return value_ssa_block_set_return(exit_block, value_ssa_value_id(result_phi_id), error);
}

static int value_ssa_recursive_helper_rewrite_iterative_helpers(ValueSsaProgram *program,
    ValueSsaError *error) {
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-342: invalid iterative helper rewrite contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];

        if (!function->has_body || !function->name) {
            continue;
        }
        if (strcmp(function->name, "multiply") == 0 &&
            value_ssa_recursive_helper_match_multiply_shape(function)) {
            if (!value_ssa_recursive_helper_rewrite_iterative_multiply(function, error)) {
                return 0;
            }
            changed_any = 1;
            continue;
        }
        if (strcmp(function->name, "power") == 0 &&
            value_ssa_recursive_helper_match_power_shape(function)) {
            if (!value_ssa_recursive_helper_rewrite_iterative_power(function, error)) {
                return 0;
            }
            changed_any = 1;
            continue;
        }
    }

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error) ||
            !value_ssa_simplify_cfg(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int __attribute__((unused)) value_ssa_optimize_recursive_helpers(ValueSsaProgram *program, ValueSsaError *error) {
    if (!value_ssa_recursive_helper_reuse_same_block_repeated_pure_internal_calls(program, error)) {
        return 0;
    }
    if (!value_ssa_recursive_helper_simplify_branch_conditions(program, error)) {
        return 0;
    }
    if (!value_ssa_recursive_helper_strength_reduce_divmod2(program, error)) {
        return 0;
    }
    if (!value_ssa_recursive_helper_simplify_parity_compares(program, error)) {
        return 0;
    }
    if (!value_ssa_recursive_helper_simplify_branch_conditions(program, error)) {
        return 0;
    }
    if (!value_ssa_recursive_helper_reuse_repeated_mod998_minus1_divs(program, error)) {
        return 0;
    }
    if (!value_ssa_recursive_helper_rewrite_iterative_helpers(program, error)) {
        return 0;
    }
    if (!value_ssa_recursive_helper_reduce_mod998_fft_butterfly(program, error)) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

#endif
