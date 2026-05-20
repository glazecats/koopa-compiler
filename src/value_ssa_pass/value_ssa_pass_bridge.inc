#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

static int value_ssa_collapse_same_block_identical_binary_runs(ValueSsaProgram *program, ValueSsaError *error);

static int value_ssa_lower_ir_is_extreme_straight_line_hotspot(const LowerIrProgram *program) {
    size_t function_index;
    int found_hotspot = 0;

    if (!program || !program->functions) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const LowerIrFunction *function = &program->functions[function_index];
        const LowerIrBasicBlock *block = NULL;
        size_t instruction_count = 0;
        size_t block_index;
        int function_is_single_block_return = 0;
        int function_has_indirect_memory = 0;

        if (!function->has_body) {
            continue;
        }
        if (function->block_count == 1u && function->blocks) {
            block = &function->blocks[0];
            function_is_single_block_return =
                block->has_terminator && block->terminator.kind == LOWER_IR_TERM_RETURN;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const LowerIrBasicBlock *scan_block = &function->blocks[block_index];
            size_t instruction_index;

            instruction_count += scan_block->instruction_count;
            for (instruction_index = 0; instruction_index < scan_block->instruction_count; ++instruction_index) {
                switch (scan_block->instructions[instruction_index].kind) {
                case LOWER_IR_INSTR_ADDR_LOCAL:
                case LOWER_IR_INSTR_ADDR_GLOBAL:
                case LOWER_IR_INSTR_LOAD_INDIRECT:
                case LOWER_IR_INSTR_STORE_INDIRECT:
                    function_has_indirect_memory = 1;
                    break;
                default:
                    break;
                }
                if (function_has_indirect_memory) {
                    break;
                }
            }
            if (function_has_indirect_memory) {
                break;
            }
        }

        if (!function_is_single_block_return || function_has_indirect_memory) {
            continue;
        }

        if (function->parameter_count > 512u ||
            function->next_temp_id >= 12000u ||
            instruction_count >= 12000u) {
            found_hotspot = 1;
        }
    }

    return found_hotspot;
}

static int value_ssa_lower_ir_uses_indirect_memory(const LowerIrProgram *program) {
    size_t function_index;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const LowerIrFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const LowerIrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                switch (block->instructions[instruction_index].kind) {
                case LOWER_IR_INSTR_ADDR_LOCAL:
                case LOWER_IR_INSTR_ADDR_GLOBAL:
                case LOWER_IR_INSTR_LOAD_INDIRECT:
                case LOWER_IR_INSTR_STORE_INDIRECT:
                    return 1;
                default:
                    break;
                }
            }
        }
    }

    return 0;
}

static int value_ssa_lower_ir_has_huge_parameter_hotspot(const LowerIrProgram *program) {
    size_t function_index;

    if (!program || !program->functions) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const LowerIrFunction *function = &program->functions[function_index];

        if (function->has_body && function->parameter_count > 512u) {
            return 1;
        }
    }

    return 0;
}

static int value_ssa_run_extreme_straight_line_fast_cleanup(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    int trace_timing = value_ssa_trace_enabled();
    double stage_start = trace_timing ? value_ssa_now_s() : 0.0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-268: invalid extreme straight-line cleanup contract");
        return 0;
    }

    if (!value_ssa_forward_local_loads(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("extreme-forward-local-loads", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }
    if (!value_ssa_forward_global_loads(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("extreme-forward-global-loads", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }
    if (!value_ssa_simplify_trivial_values(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("extreme-simplify-1", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }
    if (!value_ssa_eliminate_redundant_stores(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("extreme-redundant-stores", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }
    if (!value_ssa_eliminate_dead_stores(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("extreme-dead-stores", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }
    if (!value_ssa_normalize_binary_operands(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("extreme-normalize", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }
    if (!value_ssa_simplify_algebraic_identities(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("extreme-identities", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }
    if (!value_ssa_fold_constants(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("extreme-fold-constants", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }
    if (!value_ssa_simplify_trivial_values(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("extreme-simplify-2", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }
    /*
     * On extreme straight-line giants such as register_alloc10000, the
     * general cross-block redundant-binary/GVN pass dominates compile time
     * while buying little. Keep the cheap local simplifications here and let
     * the later backend handle the remaining straightforward code.
     */

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis analysis;
        int ok;

        if (!function->has_body) {
            continue;
        }

        value_ssa_cfg_analysis_init(&analysis);
        ok = value_ssa_compute_cfg_analysis(function, &analysis, error) &&
            value_ssa_rename_function_values(function, &analysis, error);
        value_ssa_cfg_analysis_free(&analysis);
        if (!ok) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_collapse_same_block_identical_binary_runs(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    int trace_timing = value_ssa_trace_enabled();
    size_t total_collapsed = 0u;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-270: invalid identical-binary-run cleanup contract");
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

            for (instruction_index = 0u; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *current = &block->instructions[instruction_index];
                size_t candidate_index;

                if (current->kind != VALUE_SSA_INSTR_BINARY ||
                    !current->has_result ||
                    current->result.kind != VALUE_SSA_VALUE_ID ||
                    !value_ssa_binary_op_is_safe_reusable_pure_binary(
                        current->as.binary.op,
                        current->as.binary.lhs,
                        current->as.binary.rhs)) {
                    continue;
                }

                for (candidate_index = instruction_index; candidate_index > 0u; --candidate_index) {
                    ValueSsaInstruction *candidate = &block->instructions[candidate_index - 1u];

                    if (candidate->kind != VALUE_SSA_INSTR_BINARY ||
                        !candidate->has_result ||
                        candidate->result.kind != VALUE_SSA_VALUE_ID ||
                        candidate->as.binary.op != current->as.binary.op ||
                        !value_ssa_value_refs_equal(candidate->as.binary.lhs, current->as.binary.lhs) ||
                        !value_ssa_value_refs_equal(candidate->as.binary.rhs, current->as.binary.rhs) ||
                        !value_ssa_binary_op_is_safe_reusable_pure_binary(
                            candidate->as.binary.op,
                            candidate->as.binary.lhs,
                            candidate->as.binary.rhs)) {
                        continue;
                    }

                    current->kind = VALUE_SSA_INSTR_MOV;
                    current->as.mov_value = candidate->result;
                    ++total_collapsed;
                    break;
                }
            }
        }
    }

    if (trace_timing && total_collapsed > 0u) {
        fprintf(stderr, "[value-ssa-timing] identical-binary-run-collapse collapsed=%zu\n", total_collapsed);
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_run_default_direct_binary_cleanup(ValueSsaProgram *program, ValueSsaError *error) {
    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-269: invalid default direct binary cleanup contract");
        return 0;
    }

    if (!value_ssa_inline_tiny_internal_helpers(program, error) ||
        !value_ssa_forward_non_address_taken_global_loads(program, error) ||
        !value_ssa_licm_hoist_simple_loop_invariant_values(program, error) ||
        !value_ssa_normalize_binary_operands(program, error) ||
        !value_ssa_collapse_same_block_identical_binary_runs(program, error) ||
        !value_ssa_simplify_trivial_values(program, error) ||
        !value_ssa_eliminate_dead_value_defs(program, error) ||
        !value_ssa_sparse_conditional_constant_propagation(program, error) ||
        !value_ssa_eliminate_redundant_binaries(program, error) ||
        !value_ssa_simplify_trivial_values(program, error) ||
        !value_ssa_eliminate_dead_value_defs(program, error) ||
        !value_ssa_normalize_binary_operands(program, error) ||
        !value_ssa_collapse_same_block_identical_binary_runs(program, error) ||
        !value_ssa_simplify_trivial_values(program, error) ||
        !value_ssa_eliminate_dead_value_defs(program, error) ||
        !value_ssa_eliminate_redundant_binaries(program, error) ||
        !value_ssa_simplify_trivial_values(program, error) ||
        !value_ssa_eliminate_dead_value_defs(program, error)) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_build_from_lower_ir_with_bridge(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    int (*bridge_fn)(const LowerIrProgram *, ValueSsaProgram *, ValueSsaError *),
    const char *invalid_error_code,
    ValueSsaError *error) {
    if (!program || !out_program) {
        value_ssa_set_error(error,
            0,
            0,
            "%s",
            invalid_error_code ? invalid_error_code : "VALUE-SSA-173: invalid lower-ir bridge contract");
        return 0;
    }
    if (!bridge_fn) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-175: invalid lower-ir bridge callback");
        return 0;
    }

    return bridge_fn(program, out_program, error);
}

static ValueSsaInstruction *value_ssa_bridge_lookup_instruction_definition(ValueSsaFunction *function,
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

typedef enum {
    VALUE_SSA_ADDRESS_ROOT_UNKNOWN = 0,
    VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT,
    VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT,
    VALUE_SSA_ADDRESS_ROOT_PARAMETER_POINTER,
} ValueSsaAddressRootKind;

typedef struct {
    ValueSsaAddressRootKind kind;
    size_t id;
} ValueSsaAddressRoot;

static const ValueSsaFunction *value_ssa_bridge_find_function_by_name(const ValueSsaProgram *program,
    const char *name) {
    size_t function_index;

    if (!program || !name || name[0] == '\0') {
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

static size_t value_ssa_bridge_find_function_index_by_name(const ValueSsaProgram *program,
    const char *name) {
    const ValueSsaFunction *function = value_ssa_bridge_find_function_by_name(program, name);

    if (!program || !function) {
        return (size_t)-1;
    }

    return (size_t)(function - program->functions);
}

static int value_ssa_bridge_collect_readonly_scalar_globals(const ValueSsaProgram *program,
    unsigned char *readonly_flags) {
    size_t global_index;
    size_t function_index;

    if (!program || !readonly_flags) {
        return 0;
    }

    for (global_index = 0; global_index < program->global_count; ++global_index) {
        const ValueSsaGlobal *global = &program->globals[global_index];

        readonly_flags[global_index] =
            (unsigned char)(global->byte_size == 4u && !global->has_runtime_initializer);
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL &&
                    instruction->as.store.slot.id < program->global_count) {
                    readonly_flags[instruction->as.store.slot.id] = 0u;
                } else if (instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL &&
                           instruction->as.addr_slot.id < program->global_count) {
                    readonly_flags[instruction->as.addr_slot.id] = 0u;
                }
            }
        }
    }

    return 1;
}

static int value_ssa_bridge_function_is_reusable_pure_internal_call_target(const ValueSsaProgram *program,
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
            default:
                return 0;
            }
        }
    }

    return 1;
}

static int value_ssa_bridge_function_is_pure_internal_call_candidate(const ValueSsaProgram *program,
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

static int value_ssa_bridge_collect_pure_internal_function_flags(const ValueSsaProgram *program,
    const unsigned char *readonly_globals,
    unsigned char *pure_flags) {
    size_t function_index;
    int changed = 1;

    if (!program || !readonly_globals || !pure_flags) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        pure_flags[function_index] = (unsigned char)value_ssa_bridge_function_is_pure_internal_call_candidate(
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
                        size_t callee_index = value_ssa_bridge_find_function_index_by_name(
                            program,
                            instruction->as.call.callee_name);

                        if (callee_index == (size_t)-1 || !pure_flags[callee_index]) {
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

static int value_ssa_bridge_same_call_args(const ValueSsaInstruction *lhs, const ValueSsaInstruction *rhs) {
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

static int value_ssa_bridge_instruction_blocks_pure_internal_call_reuse(const ValueSsaInstruction *instruction) {
    (void)instruction;
    return 0;
}

static int value_ssa_bridge_reuse_repeated_pure_internal_calls_in_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    unsigned char *readonly_globals = NULL;
    unsigned char *pure_function_flags = NULL;
    size_t function_index;
    int changed = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-271: invalid pure-call reuse contract");
        return 0;
    }

    readonly_globals = (unsigned char *)calloc(program->global_count ? program->global_count : 1u, sizeof(unsigned char));
    if (!readonly_globals) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-272: out of memory preparing pure-call reuse globals");
        return 0;
    }
    if (!value_ssa_bridge_collect_readonly_scalar_globals(program, readonly_globals)) {
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-273: failed collecting pure-call reuse globals");
        return 0;
    }
    pure_function_flags = (unsigned char *)calloc(program->function_count ? program->function_count : 1u,
        sizeof(unsigned char));
    if (!pure_function_flags) {
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-287: out of memory preparing pure-call reuse flags");
        return 0;
    }
    if (!value_ssa_bridge_collect_pure_internal_function_flags(program, readonly_globals, pure_function_flags)) {
        free(pure_function_flags);
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-288: failed collecting pure-call reuse flags");
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
                size_t candidate_index;

                if (instruction->kind != VALUE_SSA_INSTR_CALL ||
                    !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    !instruction->as.call.callee_name) {
                    continue;
                }

                callee = value_ssa_bridge_find_function_by_name(program, instruction->as.call.callee_name);
                if (!callee) {
                    continue;
                }
                {
                    size_t callee_index = (size_t)(callee - program->functions);
                    if (callee_index >= program->function_count || !pure_function_flags[callee_index]) {
                        continue;
                    }
                }
                if (!value_ssa_bridge_function_is_pure_internal_call_candidate(
                        program, callee, readonly_globals)) {
                    continue;
                }

                for (candidate_index = 0; candidate_index < instruction_index; ++candidate_index) {
                    ValueSsaInstruction *candidate = &block->instructions[candidate_index];
                    size_t middle_index;
                    int blocked = 0;

                    if (candidate->kind != VALUE_SSA_INSTR_CALL ||
                        !candidate->has_result ||
                        candidate->result.kind != VALUE_SSA_VALUE_ID ||
                        !candidate->as.call.callee_name ||
                        strcmp(candidate->as.call.callee_name, instruction->as.call.callee_name) != 0 ||
                        !value_ssa_bridge_same_call_args(candidate, instruction)) {
                        continue;
                    }

                    for (middle_index = candidate_index + 1u; middle_index < instruction_index; ++middle_index) {
                        if (value_ssa_bridge_instruction_blocks_pure_internal_call_reuse(
                                &block->instructions[middle_index])) {
                            blocked = 1;
                            break;
                        }
                    }
                    if (blocked) {
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

static int value_ssa_bridge_reuse_addr_roots_in_program(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-268: invalid addr-root reuse contract");
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
            unsigned char *seen_locals = NULL;
            unsigned char *seen_globals = NULL;
            ValueSsaValueRef *local_values = NULL;
            ValueSsaValueRef *global_values = NULL;
            size_t instruction_index;

            if (block->instruction_count == 0u) {
                continue;
            }

            if (function->local_count > 0u) {
                seen_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
                local_values = (ValueSsaValueRef *)calloc(function->local_count, sizeof(ValueSsaValueRef));
                if (!seen_locals || !local_values) {
                    free(local_values);
                    free(seen_locals);
                    free(global_values);
                    free(seen_globals);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-269: out of memory preparing local addr-root reuse");
                    return 0;
                }
            }
            if (program->global_count > 0u) {
                seen_globals = (unsigned char *)calloc(program->global_count, sizeof(unsigned char));
                global_values = (ValueSsaValueRef *)calloc(program->global_count, sizeof(ValueSsaValueRef));
                if (!seen_globals || !global_values) {
                    free(local_values);
                    free(seen_locals);
                    free(global_values);
                    free(seen_globals);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-270: out of memory preparing global addr-root reuse");
                    return 0;
                }
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }

                if (instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL &&
                    instruction->as.addr_slot.id < function->local_count) {
                    if (seen_locals[instruction->as.addr_slot.id]) {
                        instruction->kind = VALUE_SSA_INSTR_MOV;
                        instruction->as.mov_value = local_values[instruction->as.addr_slot.id];
                    } else {
                        seen_locals[instruction->as.addr_slot.id] = 1u;
                        local_values[instruction->as.addr_slot.id] = instruction->result;
                    }
                } else if (instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL &&
                           instruction->as.addr_slot.id < program->global_count) {
                    if (seen_globals[instruction->as.addr_slot.id]) {
                        instruction->kind = VALUE_SSA_INSTR_MOV;
                        instruction->as.mov_value = global_values[instruction->as.addr_slot.id];
                    } else {
                        seen_globals[instruction->as.addr_slot.id] = 1u;
                        global_values[instruction->as.addr_slot.id] = instruction->result;
                    }
                }
            }

            free(local_values);
            free(seen_locals);
            free(global_values);
            free(seen_globals);
        }
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_bridge_same_pure_address_value(ValueSsaFunction *function,
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
    if (lhs.kind != VALUE_SSA_VALUE_ID || rhs.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    lhs_def = value_ssa_bridge_lookup_instruction_definition(function, analysis, lhs.value_id);
    rhs_def = value_ssa_bridge_lookup_instruction_definition(function, analysis, rhs.value_id);
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
        return value_ssa_bridge_same_pure_address_value(
            function, analysis, lhs_def->as.mov_value, rhs_def->as.mov_value, depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        return lhs_def->as.binary.op == rhs_def->as.binary.op &&
            value_ssa_bridge_same_pure_address_value(
                function, analysis, lhs_def->as.binary.lhs, rhs_def->as.binary.lhs, depth + 1u) &&
            value_ssa_bridge_same_pure_address_value(
                function, analysis, lhs_def->as.binary.rhs, rhs_def->as.binary.rhs, depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_bridge_instruction_may_invalidate_address_value(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef address_value,
    const ValueSsaInstruction *instruction);

static int value_ssa_bridge_reuse_same_block_pure_address_values_in_program(
    ValueSsaProgram *program,
    ValueSsaError *error) {
    size_t function_index;
    int changed = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-349: invalid pure-address reuse contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis def_use;
        size_t block_index;

        if (!function->has_body) {
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
                size_t candidate_index;

                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }
                if (instruction->kind != VALUE_SSA_INSTR_BINARY &&
                    instruction->kind != VALUE_SSA_INSTR_MOV &&
                    instruction->kind != VALUE_SSA_INSTR_ADDR_LOCAL &&
                    instruction->kind != VALUE_SSA_INSTR_ADDR_GLOBAL &&
                    instruction->kind != VALUE_SSA_INSTR_LOAD_LOCAL &&
                    instruction->kind != VALUE_SSA_INSTR_LOAD_GLOBAL) {
                    continue;
                }

                for (candidate_index = instruction_index; candidate_index > 0; --candidate_index) {
                    ValueSsaInstruction *candidate = &block->instructions[candidate_index - 1u];
                    size_t middle_index;
                    int blocked = 0;

                    if (!candidate->has_result || candidate->result.kind != VALUE_SSA_VALUE_ID) {
                        continue;
                    }
                    if (candidate->kind != VALUE_SSA_INSTR_BINARY &&
                        candidate->kind != VALUE_SSA_INSTR_MOV &&
                        candidate->kind != VALUE_SSA_INSTR_ADDR_LOCAL &&
                        candidate->kind != VALUE_SSA_INSTR_ADDR_GLOBAL &&
                        candidate->kind != VALUE_SSA_INSTR_LOAD_LOCAL &&
                        candidate->kind != VALUE_SSA_INSTR_LOAD_GLOBAL) {
                        continue;
                    }
                    if (!value_ssa_bridge_same_pure_address_value(
                            function, &def_use, candidate->result, instruction->result, 0u)) {
                        continue;
                    }

                    for (middle_index = candidate_index; middle_index < instruction_index; ++middle_index) {
                        ValueSsaInstruction *middle = &block->instructions[middle_index];

                        if (middle->kind == VALUE_SSA_INSTR_CALL ||
                            value_ssa_bridge_instruction_may_invalidate_address_value(
                                function, &def_use, candidate->result, middle)) {
                            blocked = 1;
                            break;
                        }
                    }
                    if (blocked) {
                        continue;
                    }

                    instruction->kind = VALUE_SSA_INSTR_MOV;
                    instruction->as.mov_value = candidate->result;
                    changed = 1;
                    break;
                }
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
    }

    if (changed &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_bridge_extract_address_root(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    ValueSsaAddressRoot *out_root,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!out_root) {
        return 0;
    }

    out_root->kind = VALUE_SSA_ADDRESS_ROOT_UNKNOWN;
    out_root->id = 0u;
    if (depth > 8u || value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_bridge_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
        out_root->kind = VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT;
        out_root->id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        out_root->kind = VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT;
        out_root->id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        if (definition->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            definition->as.load_slot.id < function->local_count &&
            function->locals[definition->as.load_slot.id].is_parameter &&
            function->locals[definition->as.load_slot.id].array_rank > 0u) {
            out_root->kind = VALUE_SSA_ADDRESS_ROOT_PARAMETER_POINTER;
            out_root->id = definition->as.load_slot.id;
            return 1;
        }
        return 0;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_bridge_extract_address_root(
            function, analysis, definition->as.mov_value, out_root, depth + 1u);
    case VALUE_SSA_INSTR_BINARY: {
        ValueSsaAddressRoot lhs_root;
        ValueSsaAddressRoot rhs_root;
        int has_lhs_root;
        int has_rhs_root;

        if (definition->as.binary.op != VALUE_SSA_BINARY_ADD) {
            return 0;
        }

        has_lhs_root = value_ssa_bridge_extract_address_root(
            function, analysis, definition->as.binary.lhs, &lhs_root, depth + 1u);
        has_rhs_root = value_ssa_bridge_extract_address_root(
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

static int value_ssa_bridge_value_may_depend_on_slot(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    ValueSsaSlotRef slot,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (depth > 8u || value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_bridge_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 1;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return 0;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        return definition->as.load_slot.kind == slot.kind &&
            definition->as.load_slot.id == slot.id;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_bridge_value_may_depend_on_slot(
            function, analysis, definition->as.mov_value, slot, depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        return value_ssa_bridge_value_may_depend_on_slot(
                   function, analysis, definition->as.binary.lhs, slot, depth + 1u) ||
            value_ssa_bridge_value_may_depend_on_slot(
                   function, analysis, definition->as.binary.rhs, slot, depth + 1u);
    default:
        return 1;
    }
}

static int value_ssa_bridge_instruction_may_invalidate_address_value(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef address_value,
    const ValueSsaInstruction *instruction) {
    ValueSsaSlotRef slot;

    if (!function || !analysis || !instruction) {
        return 1;
    }

    switch (instruction->kind) {
    case VALUE_SSA_INSTR_STORE_LOCAL:
        slot = instruction->as.store.slot;
        return value_ssa_bridge_value_may_depend_on_slot(function, analysis, address_value, slot, 0u);
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        slot = instruction->as.store.slot;
        return value_ssa_bridge_value_may_depend_on_slot(function, analysis, address_value, slot, 0u);
    default:
        return 0;
    }
}

static int value_ssa_bridge_address_roots_proven_non_alias(const ValueSsaAddressRoot *lhs,
    const ValueSsaAddressRoot *rhs) {
    if (!lhs || !rhs ||
        lhs->kind == VALUE_SSA_ADDRESS_ROOT_UNKNOWN ||
        rhs->kind == VALUE_SSA_ADDRESS_ROOT_UNKNOWN) {
        return 0;
    }

    if (lhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT &&
        rhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if (lhs->kind == VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT &&
        rhs->kind == VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if ((lhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT && rhs->kind == VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT) ||
        (lhs->kind == VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT && rhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }
    if ((lhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT && rhs->kind == VALUE_SSA_ADDRESS_ROOT_PARAMETER_POINTER) ||
        (lhs->kind == VALUE_SSA_ADDRESS_ROOT_PARAMETER_POINTER && rhs->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }

    return 0;
}

static int value_ssa_bridge_store_cannot_alias_loaded_address(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaAddressRoot *load_root,
    const ValueSsaInstruction *store_instruction) {
    ValueSsaAddressRoot store_root;

    if (!function || !analysis || !load_root || !store_instruction) {
        return 0;
    }

    switch (store_instruction->kind) {
    case VALUE_SSA_INSTR_STORE_LOCAL:
        if (load_root->kind != VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT ||
            store_instruction->as.store.slot.kind != VALUE_SSA_SLOT_LOCAL ||
            store_instruction->as.store.slot.id >= function->local_count) {
            return load_root->kind != VALUE_SSA_ADDRESS_ROOT_PARAMETER_POINTER;
        }
        if (store_instruction->as.store.slot.id == load_root->id) {
            return 0;
        }
        return function->locals[store_instruction->as.store.slot.id].array_rank == 0u;
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        if (store_instruction->as.store.slot.kind != VALUE_SSA_SLOT_GLOBAL) {
            return 0;
        }
        if (load_root->kind == VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT) {
            return store_instruction->as.store.slot.id != load_root->id;
        }
        return load_root->kind == VALUE_SSA_ADDRESS_ROOT_LOCAL_SLOT;
    case VALUE_SSA_INSTR_STORE_INDIRECT:
        if (!value_ssa_bridge_extract_address_root(
                function, analysis, store_instruction->as.store_indirect.addr, &store_root, 0u)) {
            return 0;
        }
        return value_ssa_bridge_address_roots_proven_non_alias(load_root, &store_root);
    default:
        return 1;
    }
}

static int value_ssa_bridge_forward_same_block_repeated_indirect_loads(ValueSsaProgram *program,
    ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-274: invalid same-block indirect-load forward contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis def_use;
        int changed = 0;
        size_t block_index;

        if (!function->has_body) {
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
                ValueSsaInstruction *load = &block->instructions[instruction_index];
                ValueSsaAddressRoot load_root;
                int has_load_root;
                size_t candidate_index;

                if (load->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                    !load->has_result ||
                    load->result.kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }
                has_load_root = value_ssa_bridge_extract_address_root(
                    function, &def_use, load->as.load_indirect_addr, &load_root, 0u);

                for (candidate_index = instruction_index; candidate_index > 0; --candidate_index) {
                    ValueSsaInstruction *candidate = &block->instructions[candidate_index - 1u];
                    size_t middle_index;
                    int blocked = 0;

                    if (candidate->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                        !candidate->has_result ||
                        candidate->result.kind != VALUE_SSA_VALUE_ID) {
                        continue;
                    }
                    if (!value_ssa_value_refs_equal(candidate->as.load_indirect_addr, load->as.load_indirect_addr) &&
                        !value_ssa_bridge_same_pure_address_value(
                            function, &def_use, candidate->as.load_indirect_addr, load->as.load_indirect_addr, 0u)) {
                        continue;
                    }

                    for (middle_index = candidate_index; middle_index < instruction_index; ++middle_index) {
                        ValueSsaInstruction *middle = &block->instructions[middle_index];

                        if (middle->kind == VALUE_SSA_INSTR_CALL) {
                            blocked = 1;
                            break;
                        }
                        if (value_ssa_bridge_instruction_may_invalidate_address_value(
                                function, &def_use, load->as.load_indirect_addr, middle)) {
                            blocked = 1;
                            break;
                        }
                        if (has_load_root) {
                            if (!value_ssa_bridge_store_cannot_alias_loaded_address(
                                    function, &def_use, &load_root, middle)) {
                                blocked = 1;
                                break;
                            }
                        } else if (middle->kind == VALUE_SSA_INSTR_STORE_LOCAL ||
                                   middle->kind == VALUE_SSA_INSTR_STORE_GLOBAL ||
                                   middle->kind == VALUE_SSA_INSTR_STORE_INDIRECT) {
                            blocked = 1;
                            break;
                        }
                    }
                    if (blocked) {
                        continue;
                    }

                    load->kind = VALUE_SSA_INSTR_MOV;
                    load->as.mov_value = candidate->result;
                    changed = 1;
                    break;
                }
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        if (changed &&
            (!value_ssa_simplify_trivial_values(program, error) ||
                !value_ssa_eliminate_dead_value_defs(program, error))) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_bridge_forward_dominated_repeated_indirect_loads(ValueSsaProgram *program,
    ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-296: invalid dominated indirect-load forward contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis def_use;
        int changed = 0;
        size_t block_index;

        if (!function->has_body || function->block_count == 0u) {
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
                ValueSsaInstruction *load = &block->instructions[instruction_index];
                ValueSsaAddressRoot load_root;
                int has_load_root;
                size_t candidate_block_id;
                int blocked = 0;

                if (load->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                    !load->has_result ||
                    load->result.kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }

                has_load_root = value_ssa_bridge_extract_address_root(
                    function, &def_use, load->as.load_indirect_addr, &load_root, 0u);

                for (candidate_block_id = 0; candidate_block_id < function->block_count; ++candidate_block_id) {
                    ValueSsaBasicBlock *candidate_block;
                    size_t candidate_index;

                    if (candidate_block_id == block_index ||
                        !cfg.reachable[candidate_block_id] ||
                        !cfg.dominates[candidate_block_id * function->block_count + block_index]) {
                        continue;
                    }

                    candidate_block = &function->blocks[candidate_block_id];
                    for (candidate_index = candidate_block->instruction_count; candidate_index > 0; --candidate_index) {
                        ValueSsaInstruction *candidate = &candidate_block->instructions[candidate_index - 1u];
                        size_t scan_block_id;

                        if (candidate->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                            !candidate->has_result ||
                            candidate->result.kind != VALUE_SSA_VALUE_ID) {
                            continue;
                        }
                        if (!value_ssa_value_refs_equal(candidate->as.load_indirect_addr, load->as.load_indirect_addr) &&
                            !value_ssa_bridge_same_pure_address_value(
                                function,
                                &def_use,
                                candidate->as.load_indirect_addr,
                                load->as.load_indirect_addr,
                                0u)) {
                            continue;
                        }

                        blocked = 0;
                        for (scan_block_id = 0; scan_block_id < function->block_count; ++scan_block_id) {
                            ValueSsaBasicBlock *scan_block;
                            size_t scan_index;
                            size_t start_index = 0u;
                            size_t end_index;

                            if (!cfg.reachable[scan_block_id] ||
                                !cfg.dominates[candidate_block_id * function->block_count + scan_block_id] ||
                                !cfg.dominates[scan_block_id * function->block_count + block_index]) {
                                continue;
                            }

                            scan_block = &function->blocks[scan_block_id];
                            end_index = scan_block->instruction_count;
                            if (scan_block_id == candidate_block_id) {
                                start_index = candidate_index;
                            }
                            if (scan_block_id == block_index) {
                                end_index = instruction_index;
                            }
                            if (start_index >= end_index) {
                                continue;
                            }

                            for (scan_index = start_index; scan_index < end_index; ++scan_index) {
                                ValueSsaInstruction *middle = &scan_block->instructions[scan_index];

                                if (middle->kind == VALUE_SSA_INSTR_CALL) {
                                    blocked = 1;
                                    break;
                                }
                                if (value_ssa_bridge_instruction_may_invalidate_address_value(
                                        function, &def_use, load->as.load_indirect_addr, middle)) {
                                    blocked = 1;
                                    break;
                                }
                                if (has_load_root) {
                                    if (!value_ssa_bridge_store_cannot_alias_loaded_address(
                                            function, &def_use, &load_root, middle)) {
                                        blocked = 1;
                                        break;
                                    }
                                } else if (middle->kind == VALUE_SSA_INSTR_STORE_LOCAL ||
                                           middle->kind == VALUE_SSA_INSTR_STORE_GLOBAL ||
                                           middle->kind == VALUE_SSA_INSTR_STORE_INDIRECT) {
                                    blocked = 1;
                                    break;
                                }
                            }
                            if (blocked) {
                                break;
                            }
                        }
                        if (blocked) {
                            continue;
                        }

                        load->kind = VALUE_SSA_INSTR_MOV;
                        load->as.mov_value = candidate->result;
                        changed = 1;
                        candidate_block_id = function->block_count;
                        break;
                    }
                }
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_cfg_analysis_free(&cfg);
        if (changed &&
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
    size_t block_count;
    unsigned char *loop_marks;
} ValueSsaBridgeSimpleLoop;

static void value_ssa_bridge_simple_loop_init(ValueSsaBridgeSimpleLoop *loop) {
    if (!loop) {
        return;
    }

    loop->header_id = (size_t)-1;
    loop->body_id = (size_t)-1;
    loop->preheader_id = (size_t)-1;
    loop->block_count = 0u;
    loop->loop_marks = NULL;
}

static void value_ssa_bridge_simple_loop_free(ValueSsaBridgeSimpleLoop *loop) {
    if (!loop) {
        return;
    }

    free(loop->loop_marks);
    loop->loop_marks = NULL;
    loop->block_count = 0u;
}

static int value_ssa_bridge_mark_natural_loop_blocks(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t header_id,
    size_t backedge_pred_id,
    unsigned char *loop_marks,
    size_t *stack) {
    size_t stack_size = 0u;
    size_t block_count;

    if (!function || !analysis || !loop_marks || !stack ||
        analysis->block_count != function->block_count ||
        header_id >= function->block_count ||
        backedge_pred_id >= function->block_count) {
        return 0;
    }

    block_count = function->block_count;
    memset(loop_marks, 0, block_count * sizeof(unsigned char));
    loop_marks[header_id] = 1u;
    loop_marks[backedge_pred_id] = 1u;
    if (backedge_pred_id != header_id) {
        stack[stack_size++] = backedge_pred_id;
    }

    while (stack_size > 0u) {
        size_t block_id = stack[--stack_size];
        size_t predecessor_id;

        for (predecessor_id = 0; predecessor_id < block_count; ++predecessor_id) {
            if (!analysis->reachable[predecessor_id] ||
                !analysis->predecessor_matrix[predecessor_id * block_count + block_id]) {
                continue;
            }
            if (predecessor_id != header_id &&
                !analysis->dominates[header_id * block_count + predecessor_id]) {
                continue;
            }
            if (loop_marks[predecessor_id]) {
                continue;
            }

            loop_marks[predecessor_id] = 1u;
            stack[stack_size++] = predecessor_id;
        }
    }

    return 1;
}

static int value_ssa_bridge_loop_contains_block(const ValueSsaBridgeSimpleLoop *loop, size_t block_id) {
    return loop &&
        loop->loop_marks &&
        block_id < loop->block_count &&
        loop->loop_marks[block_id];
}

static int value_ssa_bridge_find_simple_single_body_loop(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t header_id,
    ValueSsaBridgeSimpleLoop *out_loop) {
    const ValueSsaBasicBlock *header;
    size_t predecessor_index;
    size_t predecessor_a = (size_t)-1;
    size_t predecessor_b = (size_t)-1;
    size_t body_id = (size_t)-1;
    size_t preheader_id = (size_t)-1;
    unsigned char *loop_marks = NULL;
    size_t *stack = NULL;

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

    loop_marks = (unsigned char *)calloc(function->block_count ? function->block_count : 1u, sizeof(unsigned char));
    stack = (size_t *)malloc((function->block_count ? function->block_count : 1u) * sizeof(size_t));
    if (!loop_marks || !stack ||
        !value_ssa_bridge_mark_natural_loop_blocks(function, analysis, header_id, body_id, loop_marks, stack)) {
        free(stack);
        free(loop_marks);
        return 0;
    }
    free(stack);

    out_loop->header_id = header_id;
    out_loop->body_id = body_id;
    out_loop->preheader_id = preheader_id;
    out_loop->block_count = function->block_count;
    out_loop->loop_marks = loop_marks;
    return 1;
}

static int value_ssa_bridge_call_is_reusable_pure_internal_target(const ValueSsaProgram *program,
    const unsigned char *readonly_globals,
    const ValueSsaInstruction *instruction) {
    const ValueSsaFunction *callee = NULL;

    if (!program || !readonly_globals || !instruction ||
        instruction->kind != VALUE_SSA_INSTR_CALL ||
        !instruction->as.call.callee_name) {
        return 0;
    }

    callee = value_ssa_bridge_find_function_by_name(program, instruction->as.call.callee_name);
    if (!callee) {
        return 0;
    }

    return value_ssa_bridge_function_is_reusable_pure_internal_call_target(program, callee, readonly_globals);
}

static int value_ssa_bridge_call_targets_pure_internal_function(const ValueSsaProgram *program,
    const unsigned char *pure_function_flags,
    const ValueSsaInstruction *instruction) {
    size_t callee_index;

    if (!program || !pure_function_flags || !instruction ||
        instruction->kind != VALUE_SSA_INSTR_CALL ||
        !instruction->as.call.callee_name) {
        return 0;
    }

    callee_index = value_ssa_bridge_find_function_index_by_name(program, instruction->as.call.callee_name);
    return callee_index != (size_t)-1 && pure_function_flags[callee_index];
}

static int value_ssa_bridge_loop_instruction_preserves_global_invariance(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *pure_function_flags,
    size_t global_id,
    const ValueSsaInstruction *instruction) {
    ValueSsaAddressRoot load_root;

    if (!program || !function || !analysis || !readonly_globals || !instruction || global_id >= program->global_count) {
        return 0;
    }

    if (readonly_globals[global_id]) {
        return 1;
    }

    load_root.kind = VALUE_SSA_ADDRESS_ROOT_GLOBAL_SLOT;
    load_root.id = global_id;

    if (instruction->kind == VALUE_SSA_INSTR_CALL) {
        return value_ssa_bridge_call_targets_pure_internal_function(program, pure_function_flags, instruction);
    }

    return value_ssa_bridge_store_cannot_alias_loaded_address(function, analysis, &load_root, instruction);
}

static int value_ssa_bridge_loop_preserves_local_slot(const ValueSsaFunction *function,
    const ValueSsaBridgeSimpleLoop *loop,
    size_t slot_id) {
    size_t block_id;

    if (!function || !loop || slot_id >= function->local_count) {
        return 0;
    }

    for (block_id = 0; block_id < function->block_count; ++block_id) {
        const ValueSsaBasicBlock *block;
        size_t instruction_index;

        if (!value_ssa_bridge_loop_contains_block(loop, block_id)) {
            continue;
        }
        block = &function->blocks[block_id];
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

static int value_ssa_bridge_loop_preserves_global_slot(const ValueSsaProgram *program,
    const unsigned char *readonly_globals,
    size_t slot_id) {
    return program && readonly_globals && slot_id < program->global_count && readonly_globals[slot_id];
}

static int value_ssa_bridge_value_is_simple_loop_invariant(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *pure_function_flags,
    const ValueSsaBridgeSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned depth) {
    ValueSsaInstruction *definition;
    size_t scan_index;

    if (!program || !function || !analysis || !readonly_globals || !loop) {
        return 0;
    }
    if (value.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }
    if (depth > 12u) {
        return 0;
    }

    definition = value_ssa_bridge_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }
    if (definition->kind == VALUE_SSA_INSTR_CALL) {
        return 0;
    }
    if (!value_ssa_bridge_loop_contains_block(loop, analysis->def_block_ids[value.value_id])) {
        return 1;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return 1;
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        if (definition->as.load_slot.kind != VALUE_SSA_SLOT_GLOBAL ||
            definition->as.load_slot.id >= program->global_count) {
            return 0;
        }
        for (scan_index = 0; scan_index < function->block_count; ++scan_index) {
            const ValueSsaBasicBlock *block = &function->blocks[scan_index];
            size_t instruction_index;

            if (!value_ssa_bridge_loop_contains_block(loop, scan_index)) {
                continue;
            }
            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                if (!value_ssa_bridge_loop_instruction_preserves_global_invariance(
                        program,
                        function,
                        analysis,
                        readonly_globals,
                        pure_function_flags,
                        definition->as.load_slot.id,
                        &block->instructions[instruction_index])) {
                    return 0;
                }
            }
        }
        return 1;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_bridge_value_is_simple_loop_invariant(
            program, function, analysis, readonly_globals, pure_function_flags, loop, definition->as.mov_value, depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        return value_ssa_bridge_value_is_simple_loop_invariant(
                   program, function, analysis, readonly_globals, pure_function_flags, loop, definition->as.binary.lhs, depth + 1u) &&
            value_ssa_bridge_value_is_simple_loop_invariant(
                   program, function, analysis, readonly_globals, pure_function_flags, loop, definition->as.binary.rhs, depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_bridge_clone_loop_invariant_value_into_preheader(const ValueSsaProgram *program,
    ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const unsigned char *readonly_globals,
    const unsigned char *pure_function_flags,
    const ValueSsaBridgeSimpleLoop *loop,
    ValueSsaValueRef value,
    unsigned char *has_mapping,
    ValueSsaValueRef *mappings,
    ValueSsaBasicBlock *rebuilt_preheader,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    ValueSsaInstruction *definition;
    ValueSsaInstruction cloned_instruction;

    if (!out_value) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-275: invalid loop-invariant clone output");
        return 0;
    }

    if (value.kind != VALUE_SSA_VALUE_ID) {
        *out_value = value;
        return 1;
    }
    if (value.value_id < function->next_value_id && has_mapping[value.value_id]) {
        *out_value = mappings[value.value_id];
        return 1;
    }

    definition = value_ssa_bridge_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-276: missing loop-invariant definition for ssa.%zu", value.value_id);
        return 0;
    }
    if (!value_ssa_bridge_loop_contains_block(loop, analysis->def_block_ids[value.value_id])) {
        *out_value = value;
        return 1;
    }
    if (!value_ssa_bridge_value_is_simple_loop_invariant(
            program, function, analysis, readonly_globals, pure_function_flags, loop, value, 0u)) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-277: attempted to clone non-invariant loop value ssa.%zu", value.value_id);
        return 0;
    }

    memset(&cloned_instruction, 0, sizeof(cloned_instruction));
    cloned_instruction.kind = definition->kind;
    cloned_instruction.has_result = 1;
    cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
    if (cloned_instruction.result.value_id == (size_t)-1) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-278: failed to allocate hoisted loop value");
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        cloned_instruction.as.addr_slot = definition->as.addr_slot;
        break;
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        cloned_instruction.as.load_slot = definition->as.load_slot;
        break;
    case VALUE_SSA_INSTR_MOV:
        if (!value_ssa_bridge_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
                pure_function_flags,
                loop,
                definition->as.mov_value,
                has_mapping,
                mappings,
                rebuilt_preheader,
                &cloned_instruction.as.mov_value,
                error)) {
            return 0;
        }
        break;
    case VALUE_SSA_INSTR_BINARY:
        cloned_instruction.as.binary.op = definition->as.binary.op;
        if (!value_ssa_bridge_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
                pure_function_flags,
                loop,
                definition->as.binary.lhs,
                has_mapping,
                mappings,
                rebuilt_preheader,
                &cloned_instruction.as.binary.lhs,
                error) ||
            !value_ssa_bridge_clone_loop_invariant_value_into_preheader(
                program,
                function,
                analysis,
                readonly_globals,
                pure_function_flags,
                loop,
                definition->as.binary.rhs,
                has_mapping,
                mappings,
                rebuilt_preheader,
                &cloned_instruction.as.binary.rhs,
                error)) {
            return 0;
        }
        break;
    default:
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-279: unsupported invariant clone instruction kind %d",
            (int)definition->kind);
        return 0;
    }

    if (!value_ssa_block_append_instruction(rebuilt_preheader, &cloned_instruction, error)) {
        return 0;
    }

    has_mapping[value.value_id] = 1u;
    mappings[value.value_id] = cloned_instruction.result;
    *out_value = cloned_instruction.result;
    return 1;
}

static int value_ssa_bridge_hoist_simple_loop_invariant_loads_in_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    unsigned char *readonly_globals = NULL;
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-289: invalid loop-invariant load hoist contract");
        return 0;
    }

    readonly_globals = (unsigned char *)calloc(program->global_count ? program->global_count : 1u, sizeof(unsigned char));
    if (!readonly_globals) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-290: out of memory preparing loop-invariant load globals");
        return 0;
    }
    if (!value_ssa_bridge_collect_readonly_scalar_globals(program, readonly_globals)) {
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-291: failed collecting loop-invariant load globals");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis cfg;
        ValueSsaDefUseAnalysis def_use;
        size_t header_id;
        int changed_this_round = 0;

        if (!function->has_body) {
            continue;
        }

        value_ssa_cfg_analysis_init(&cfg);
        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_cfg_analysis(function, &cfg, error) ||
            !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            free(readonly_globals);
            return 0;
        }

        for (header_id = 0; header_id < function->block_count; ++header_id) {
            ValueSsaBridgeSimpleLoop loop;
            ValueSsaBasicBlock *preheader;
            ValueSsaBasicBlock *body;
            ValueSsaBasicBlock rebuilt_preheader;
            ValueSsaBasicBlock rebuilt_body;
            unsigned char *has_mapping = NULL;
            ValueSsaValueRef *mappings = NULL;
            size_t instruction_index;
            int changed_loop = 0;
            int ok = 1;

            value_ssa_bridge_simple_loop_init(&loop);
            if (!value_ssa_bridge_find_simple_single_body_loop(function, &cfg, header_id, &loop)) {
                continue;
            }

            preheader = &function->blocks[loop.preheader_id];
            body = &function->blocks[loop.body_id];
            if (preheader->instruction_count == 0u || body->instruction_count == 0u) {
                value_ssa_bridge_simple_loop_free(&loop);
                continue;
            }

            memset(&rebuilt_preheader, 0, sizeof(rebuilt_preheader));
            memset(&rebuilt_body, 0, sizeof(rebuilt_body));
            rebuilt_preheader.id = preheader->id;
            rebuilt_body.id = body->id;

            has_mapping = (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1u,
                sizeof(unsigned char));
            mappings = (ValueSsaValueRef *)calloc(function->next_value_id ? function->next_value_id : 1u,
                sizeof(ValueSsaValueRef));
            if (!has_mapping || !mappings) {
                free(mappings);
                free(has_mapping);
                value_ssa_basic_block_free(&rebuilt_preheader);
                value_ssa_basic_block_free(&rebuilt_body);
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-292: out of memory preparing invariant-load mappings");
                return 0;
            }

            for (instruction_index = 0; instruction_index < preheader->instruction_count; ++instruction_index) {
                if (!value_ssa_block_append_instruction(&rebuilt_preheader, &preheader->instructions[instruction_index], error)) {
                    ok = 0;
                    break;
                }
            }

            for (instruction_index = 0; ok && instruction_index < body->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &body->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
                    instruction->as.load_slot.id < function->local_count &&
                    function->locals[instruction->as.load_slot.id].is_parameter &&
                    value_ssa_bridge_loop_preserves_local_slot(function, &loop, instruction->as.load_slot.id)) {
                    ValueSsaInstruction cloned_load;
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;
                    size_t slot_id = instruction->as.load_slot.id;

                    if (!has_mapping[slot_id]) {
                        memset(&cloned_load, 0, sizeof(cloned_load));
                        cloned_load.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
                        cloned_load.has_result = 1;
                        cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                        cloned_load.as.load_slot = instruction->as.load_slot;
                        if (cloned_load.result.value_id == (size_t)-1 ||
                            !value_ssa_block_append_instruction(&rebuilt_preheader, &cloned_load, error)) {
                            ok = 0;
                            break;
                        }
                        has_mapping[slot_id] = 1u;
                        mappings[slot_id] = cloned_load.result;
                    }

                    hoisted_value = mappings[slot_id];
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if (instruction->kind == VALUE_SSA_INSTR_LOAD_GLOBAL &&
                    instruction->has_result &&
                    instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    instruction->as.load_slot.kind == VALUE_SSA_SLOT_GLOBAL &&
                    instruction->as.load_slot.id < program->global_count &&
                    value_ssa_bridge_loop_preserves_global_slot(program, readonly_globals, instruction->as.load_slot.id)) {
                    ValueSsaInstruction cloned_load;
                    ValueSsaInstruction replacement;
                    ValueSsaValueRef hoisted_value;
                    size_t slot_id = instruction->as.load_slot.id;

                    if (!has_mapping[slot_id]) {
                        memset(&cloned_load, 0, sizeof(cloned_load));
                        cloned_load.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
                        cloned_load.has_result = 1;
                        cloned_load.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                        cloned_load.as.load_slot = instruction->as.load_slot;
                        if (cloned_load.result.value_id == (size_t)-1 ||
                            !value_ssa_block_append_instruction(&rebuilt_preheader, &cloned_load, error)) {
                            ok = 0;
                            break;
                        }
                        has_mapping[slot_id] = 1u;
                        mappings[slot_id] = cloned_load.result;
                    }

                    hoisted_value = mappings[slot_id];
                    memset(&replacement, 0, sizeof(replacement));
                    replacement.kind = VALUE_SSA_INSTR_MOV;
                    replacement.has_result = 1;
                    replacement.result = instruction->result;
                    replacement.as.mov_value = hoisted_value;
                    if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                        ok = 0;
                        break;
                    }
                    changed_loop = 1;
                    continue;
                }

                if (!value_ssa_block_append_instruction(&rebuilt_body, instruction, error)) {
                    ok = 0;
                    break;
                }
            }

            if (ok && changed_loop) {
                rebuilt_preheader.has_terminator = preheader->has_terminator;
                rebuilt_preheader.terminator = preheader->terminator;
                rebuilt_body.has_terminator = body->has_terminator;
                rebuilt_body.terminator = body->terminator;

                for (instruction_index = 0; instruction_index < preheader->instruction_count; ++instruction_index) {
                    value_ssa_instruction_free(&preheader->instructions[instruction_index]);
                }
                free(preheader->instructions);
                preheader->instructions = rebuilt_preheader.instructions;
                preheader->instruction_count = rebuilt_preheader.instruction_count;
                preheader->instruction_capacity = rebuilt_preheader.instruction_capacity;
                rebuilt_preheader.instructions = NULL;
                rebuilt_preheader.instruction_count = 0u;
                rebuilt_preheader.instruction_capacity = 0u;

                for (instruction_index = 0; instruction_index < body->instruction_count; ++instruction_index) {
                    value_ssa_instruction_free(&body->instructions[instruction_index]);
                }
                free(body->instructions);
                body->instructions = rebuilt_body.instructions;
                body->instruction_count = rebuilt_body.instruction_count;
                body->instruction_capacity = rebuilt_body.instruction_capacity;
                rebuilt_body.instructions = NULL;
                rebuilt_body.instruction_count = 0u;
                rebuilt_body.instruction_capacity = 0u;

                changed_any = 1;
                changed_this_round = 1;
            }

            value_ssa_basic_block_free(&rebuilt_preheader);
            value_ssa_basic_block_free(&rebuilt_body);
            free(mappings);
            free(has_mapping);
            value_ssa_bridge_simple_loop_free(&loop);

            if (!ok) {
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                return 0;
            }
            if (changed_loop) {
                break;
            }
        }

        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_cfg_analysis_free(&cfg);

        if (changed_this_round) {
            --function_index;
        }
    }

    free(readonly_globals);

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_bridge_hoist_simple_loop_invariant_pure_calls_in_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    unsigned char *readonly_globals = NULL;
    unsigned char *pure_function_flags = NULL;
    size_t function_index;
    int changed_any = 0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-280: invalid loop-invariant pure-call hoist contract");
        return 0;
    }

    readonly_globals = (unsigned char *)calloc(program->global_count ? program->global_count : 1u, sizeof(unsigned char));
    if (!readonly_globals) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-281: out of memory preparing loop-invariant pure-call globals");
        return 0;
    }
    if (!value_ssa_bridge_collect_readonly_scalar_globals(program, readonly_globals)) {
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-282: failed collecting loop-invariant pure-call globals");
        return 0;
    }
    pure_function_flags = (unsigned char *)calloc(program->function_count ? program->function_count : 1u, sizeof(unsigned char));
    if (!pure_function_flags) {
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-285: out of memory preparing loop-invariant pure-call flags");
        return 0;
    }
    if (!value_ssa_bridge_collect_pure_internal_function_flags(program, readonly_globals, pure_function_flags)) {
        free(pure_function_flags);
        free(readonly_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-286: failed collecting loop-invariant pure-call flags");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];

        if (!function->has_body) {
            continue;
        }

        do {
            ValueSsaCfgAnalysis cfg;
            ValueSsaDefUseAnalysis def_use;
            size_t header_id;
            int changed_this_round = 0;

            value_ssa_cfg_analysis_init(&cfg);
            value_ssa_def_use_analysis_init(&def_use);
            if (!value_ssa_compute_cfg_analysis(function, &cfg, error) ||
                !value_ssa_compute_def_use_analysis(function, &def_use, error)) {
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                return 0;
            }

            for (header_id = 0; header_id < function->block_count; ++header_id) {
                ValueSsaBridgeSimpleLoop loop;
                ValueSsaBasicBlock *preheader;
                ValueSsaBasicBlock *body;
                ValueSsaBasicBlock rebuilt_preheader;
                ValueSsaBasicBlock rebuilt_body;
                unsigned char *has_mapping = NULL;
                ValueSsaValueRef *mappings = NULL;
            size_t instruction_index;
            int changed_loop = 0;
            int ok = 1;

            value_ssa_bridge_simple_loop_init(&loop);
            if (!value_ssa_bridge_find_simple_single_body_loop(function, &cfg, header_id, &loop)) {
                continue;
            }

            preheader = &function->blocks[loop.preheader_id];
            body = &function->blocks[loop.body_id];
            if (preheader->instruction_count == 0u || body->instruction_count == 0u) {
                value_ssa_bridge_simple_loop_free(&loop);
                continue;
            }

                memset(&rebuilt_preheader, 0, sizeof(rebuilt_preheader));
                memset(&rebuilt_body, 0, sizeof(rebuilt_body));
                rebuilt_preheader.id = preheader->id;
                rebuilt_body.id = body->id;

                has_mapping = (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1u,
                    sizeof(unsigned char));
                mappings = (ValueSsaValueRef *)calloc(function->next_value_id ? function->next_value_id : 1u,
                    sizeof(ValueSsaValueRef));
                if (!has_mapping || !mappings) {
                    free(mappings);
                    free(has_mapping);
                value_ssa_basic_block_free(&rebuilt_preheader);
                value_ssa_basic_block_free(&rebuilt_body);
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_cfg_analysis_free(&cfg);
                free(readonly_globals);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-283: out of memory preparing loop-invariant mappings");
                    return 0;
                }

                for (instruction_index = 0; instruction_index < preheader->instruction_count; ++instruction_index) {
                    if (!value_ssa_block_append_instruction(&rebuilt_preheader, &preheader->instructions[instruction_index], error)) {
                        ok = 0;
                        break;
                    }
                }

                for (instruction_index = 0; ok && instruction_index < body->instruction_count; ++instruction_index) {
                    const ValueSsaInstruction *instruction = &body->instructions[instruction_index];

                    if (instruction->kind == VALUE_SSA_INSTR_CALL &&
                        instruction->has_result &&
                        instruction->result.kind == VALUE_SSA_VALUE_ID &&
                        value_ssa_bridge_call_targets_pure_internal_function(program, pure_function_flags, instruction)) {
                        size_t arg_index;
                        ValueSsaInstruction hoisted_call;
                        ValueSsaInstruction replacement;

                        for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                            if (!value_ssa_bridge_value_is_simple_loop_invariant(
                                    program,
                                    function,
                                    &def_use,
                                    readonly_globals,
                                    pure_function_flags,
                                    &loop,
                                    instruction->as.call.args[arg_index],
                                    0u)) {
                                break;
                            }
                        }

                        if (arg_index == instruction->as.call.arg_count) {
                            memset(&hoisted_call, 0, sizeof(hoisted_call));
                            hoisted_call.kind = VALUE_SSA_INSTR_CALL;
                            hoisted_call.has_result = 1;
                            hoisted_call.result = value_ssa_value_id(value_ssa_function_allocate_value(function));
                            hoisted_call.as.call.callee_name = instruction->as.call.callee_name;
                            hoisted_call.as.call.arg_count = instruction->as.call.arg_count;
                            hoisted_call.as.call.args = (ValueSsaValueRef *)calloc(
                                instruction->as.call.arg_count ? instruction->as.call.arg_count : 1u,
                                sizeof(ValueSsaValueRef));
                            if (hoisted_call.result.value_id == (size_t)-1 || !hoisted_call.as.call.args) {
                                free(hoisted_call.as.call.args);
                                ok = 0;
                                value_ssa_set_error(error, 0, 0, "VALUE-SSA-284: out of memory preparing hoisted pure call");
                                break;
                            }

                            for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                                if (!value_ssa_bridge_clone_loop_invariant_value_into_preheader(
                                        program,
                                        function,
                                        &def_use,
                                        readonly_globals,
                                        pure_function_flags,
                                        &loop,
                                        instruction->as.call.args[arg_index],
                                        has_mapping,
                                        mappings,
                                        &rebuilt_preheader,
                                        &hoisted_call.as.call.args[arg_index],
                                        error)) {
                                    free(hoisted_call.as.call.args);
                                    ok = 0;
                                    break;
                                }
                            }
                            if (!ok || !value_ssa_block_append_instruction(&rebuilt_preheader, &hoisted_call, error)) {
                                free(hoisted_call.as.call.args);
                                ok = 0;
                                break;
                            }
                            free(hoisted_call.as.call.args);

                            memset(&replacement, 0, sizeof(replacement));
                            replacement.kind = VALUE_SSA_INSTR_MOV;
                            replacement.has_result = 1;
                            replacement.result = instruction->result;
                            replacement.as.mov_value = hoisted_call.result;
                            if (!value_ssa_block_append_instruction(&rebuilt_body, &replacement, error)) {
                                ok = 0;
                                break;
                            }
                            changed_loop = 1;
                            continue;
                        }
                    }

                    if (!value_ssa_block_append_instruction(&rebuilt_body, instruction, error)) {
                        ok = 0;
                        break;
                    }
                }

                if (ok && changed_loop) {
                    rebuilt_preheader.has_terminator = preheader->has_terminator;
                    rebuilt_preheader.terminator = preheader->terminator;
                    rebuilt_body.has_terminator = body->has_terminator;
                    rebuilt_body.terminator = body->terminator;

                    for (instruction_index = 0; instruction_index < preheader->instruction_count; ++instruction_index) {
                        value_ssa_instruction_free(&preheader->instructions[instruction_index]);
                    }
                    free(preheader->instructions);
                    preheader->instructions = rebuilt_preheader.instructions;
                    preheader->instruction_count = rebuilt_preheader.instruction_count;
                    preheader->instruction_capacity = rebuilt_preheader.instruction_capacity;
                    rebuilt_preheader.instructions = NULL;
                    rebuilt_preheader.instruction_count = 0u;
                    rebuilt_preheader.instruction_capacity = 0u;

                    for (instruction_index = 0; instruction_index < body->instruction_count; ++instruction_index) {
                        value_ssa_instruction_free(&body->instructions[instruction_index]);
                    }
                    free(body->instructions);
                    body->instructions = rebuilt_body.instructions;
                    body->instruction_count = rebuilt_body.instruction_count;
                    body->instruction_capacity = rebuilt_body.instruction_capacity;
                    rebuilt_body.instructions = NULL;
                    rebuilt_body.instruction_count = 0u;
                    rebuilt_body.instruction_capacity = 0u;

                    changed_any = 1;
                    changed_this_round = 1;
                }

                value_ssa_basic_block_free(&rebuilt_preheader);
                value_ssa_basic_block_free(&rebuilt_body);
                free(mappings);
                free(has_mapping);
                value_ssa_bridge_simple_loop_free(&loop);

                if (!ok) {
                    value_ssa_def_use_analysis_free(&def_use);
                    value_ssa_cfg_analysis_free(&cfg);
                    free(readonly_globals);
                    free(pure_function_flags);
                    return 0;
                }
                if (changed_loop) {
                    break;
                }
            }

            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);

            if (!changed_this_round) {
                break;
            }
        } while (1);
    }

    free(pure_function_flags);
    free(readonly_globals);

    if (changed_any &&
        (!value_ssa_simplify_trivial_values(program, error) ||
            !value_ssa_eliminate_dead_value_defs(program, error))) {
        return 0;
    }

    return value_ssa_verify_program(program, error);
}

static int value_ssa_run_indirect_memory_direct_fast_cleanup(ValueSsaProgram *program, ValueSsaError *error) {
    int trace_timing = value_ssa_trace_enabled();
    double stage_start = trace_timing ? value_ssa_now_s() : 0.0;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-257: invalid indirect-memory direct cleanup contract");
        return 0;
    }

    if (!value_ssa_forward_local_loads(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-forward-local-loads", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_bridge_hoist_simple_loop_invariant_loads_in_program(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-hoist-simple-loop-invariant-loads", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_eliminate_unread_scalar_local_stores(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-eliminate-unread-scalar-local-stores", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_inline_tiny_internal_helpers(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-inline-tiny-helpers", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_licm_hoist_simple_loop_invariant_values(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-licm", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    {
        size_t function_index;
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            ValueSsaFunction *function = &program->functions[function_index];
            ValueSsaCfgAnalysis cfg;
            ValueSsaDefUseAnalysis def_use;
            int changed = 0;
            size_t block_index;

            if (!function->has_body || function->block_count == 0u) {
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
                size_t predecessor_id = value_ssa_pass_get_single_idom_predecessor(function, &cfg, block_index);
                ValueSsaBasicBlock *predecessor;
                size_t predecessor_inst_index;
                size_t instruction_index;

                if (predecessor_id == (size_t)-1 || block->instruction_count == 0u) {
                    continue;
                }
                predecessor = &function->blocks[predecessor_id];

                for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                    ValueSsaInstruction *load = &block->instructions[instruction_index];
                    ValueSsaAddressRoot load_root;
                    int has_load_root;
                    size_t candidate_index;
                    ValueSsaInstruction *candidate = NULL;
                    size_t scan_index;
                    int blocked = 0;

                    if (load->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                        !load->has_result ||
                        load->result.kind != VALUE_SSA_VALUE_ID) {
                        continue;
                    }
                    has_load_root = value_ssa_bridge_extract_address_root(
                        function, &def_use, load->as.load_indirect_addr, &load_root, 0u);

                    for (scan_index = 0; scan_index < instruction_index; ++scan_index) {
                        ValueSsaInstruction *middle = &block->instructions[scan_index];

                        if (middle->kind == VALUE_SSA_INSTR_CALL) {
                            blocked = 1;
                            break;
                        }
                        if (value_ssa_bridge_instruction_may_invalidate_address_value(
                                function, &def_use, load->as.load_indirect_addr, middle)) {
                            blocked = 1;
                            break;
                        }
                        if (has_load_root) {
                            if (!value_ssa_bridge_store_cannot_alias_loaded_address(
                                    function, &def_use, &load_root, middle)) {
                                blocked = 1;
                                break;
                            }
                        } else if (middle->kind == VALUE_SSA_INSTR_STORE_LOCAL ||
                                   middle->kind == VALUE_SSA_INSTR_STORE_GLOBAL ||
                                   middle->kind == VALUE_SSA_INSTR_STORE_INDIRECT) {
                            blocked = 1;
                            break;
                        }
                        if (blocked) {
                            break;
                        }
                    }
                    if (blocked) {
                        continue;
                    }

                    for (candidate_index = predecessor->instruction_count; candidate_index > 0; --candidate_index) {
                        candidate = &predecessor->instructions[candidate_index - 1u];
                        if (candidate->kind != VALUE_SSA_INSTR_LOAD_INDIRECT ||
                            !candidate->has_result ||
                            candidate->result.kind != VALUE_SSA_VALUE_ID) {
                            continue;
                        }

                        blocked = 0;
                        for (predecessor_inst_index = candidate_index; predecessor_inst_index < predecessor->instruction_count;
                             ++predecessor_inst_index) {
                            ValueSsaInstruction *after = &predecessor->instructions[predecessor_inst_index];
                            if (after->result.kind == VALUE_SSA_VALUE_ID &&
                                candidate->result.kind == VALUE_SSA_VALUE_ID &&
                                after->result.value_id == candidate->result.value_id) {
                                blocked = 1;
                                break;
                            }
                            if (after->kind == VALUE_SSA_INSTR_CALL) {
                                blocked = 1;
                                break;
                            }
                            if (value_ssa_bridge_instruction_may_invalidate_address_value(
                                    function, &def_use, load->as.load_indirect_addr, after)) {
                                blocked = 1;
                                break;
                            }
                            if (has_load_root) {
                                if (!value_ssa_bridge_store_cannot_alias_loaded_address(
                                        function, &def_use, &load_root, after)) {
                                    blocked = 1;
                                    break;
                                }
                            } else if (after->kind == VALUE_SSA_INSTR_STORE_LOCAL ||
                                       after->kind == VALUE_SSA_INSTR_STORE_GLOBAL ||
                                       after->kind == VALUE_SSA_INSTR_STORE_INDIRECT) {
                                blocked = 1;
                                break;
                            }
                            if (blocked) {
                                break;
                            }
                        }
                        if (blocked) {
                            continue;
                        }

                        if (value_ssa_value_refs_equal(candidate->as.load_indirect_addr, load->as.load_indirect_addr)) {
                            load->kind = VALUE_SSA_INSTR_MOV;
                            load->as.mov_value = candidate->result;
                            changed = 1;
                            break;
                        }

                        if (value_ssa_bridge_same_pure_address_value(
                                function,
                                &def_use,
                                candidate->as.load_indirect_addr,
                                load->as.load_indirect_addr,
                                0u)) {
                            load->kind = VALUE_SSA_INSTR_MOV;
                            load->as.mov_value = candidate->result;
                            changed = 1;
                            break;
                        }
                    }
                }
            }

            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_cfg_analysis_free(&cfg);
            if (changed && !value_ssa_simplify_trivial_values(program, error)) {
                return 0;
            }
        }
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-edge-load-forward", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_forward_non_address_taken_global_loads(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-forward-safe-global-loads", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_bridge_forward_same_block_repeated_indirect_loads(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-same-block-indirect-load-forward", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_simplify_trivial_values(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-simplify-trivial-values", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_bridge_forward_same_block_repeated_indirect_loads(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-same-block-indirect-load-forward-rerun",
            value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_bridge_reuse_repeated_pure_internal_calls_in_program(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-reuse-repeated-pure-internal-calls",
            value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_bridge_hoist_simple_loop_invariant_pure_calls_in_program(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-hoist-simple-loop-invariant-pure-calls",
            value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    if (!value_ssa_sparse_conditional_constant_propagation(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("indirect-direct-sccp", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }

    return value_ssa_verify_program(program, error);
}

int value_ssa_build_from_lower_ir_with_canonicalization(const LowerIrProgram *program,
    ValueSsaLowerIrCanonicalizeMode mode,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    if (!program || !out_program) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-176: invalid lower-ir canonicalization-mode entry contract");
        return 0;
    }

    switch (mode) {
    case VALUE_SSA_LOWER_IR_CANONICALIZE_CLASSIC:
        if (!value_ssa_build_from_lower_ir(program, out_program, error)) {
            return 0;
        }
        if (!value_ssa_canonicalize_program(out_program, error)) {
            value_ssa_program_free(out_program);
            return 0;
        }
        return 1;
    case VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_VALUE:
        return value_ssa_build_from_lower_ir_with_bridge(program,
            out_program,
            memory_ssa_pass_build_memory_canonicalized_from_lower_ir,
            "VALUE-SSA-173: invalid lower-ir memory-value canonicalized conversion entry contract",
            error);
    case VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_FULL:
        return value_ssa_build_from_lower_ir_with_bridge(program,
            out_program,
            memory_ssa_pass_build_canonicalized_from_lower_ir,
            "VALUE-SSA-174: invalid lower-ir memory canonicalized conversion entry contract",
            error);
    default:
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-177: unknown lower-ir canonicalization mode %d",
            (int)mode);
        return 0;
    }
}

int value_ssa_build_default_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    int trace_timing = value_ssa_trace_enabled();
    double total_start = trace_timing ? value_ssa_now_s() : 0.0;
    double stage_start = 0.0;
    int uses_indirect_memory = value_ssa_lower_ir_uses_indirect_memory(program);
    int use_extreme_straight_line_fast_path = value_ssa_lower_ir_is_extreme_straight_line_hotspot(program);
    int has_huge_parameter_hotspot = value_ssa_lower_ir_has_huge_parameter_hotspot(program);

    if (trace_timing) {
        fprintf(stderr,
            "[value-ssa-timing] flags indirect=%d extreme-straight-line=%d huge-params=%d\n",
            uses_indirect_memory,
            use_extreme_straight_line_fast_path,
            has_huge_parameter_hotspot);
    }
    if (uses_indirect_memory) {
        stage_start = trace_timing ? value_ssa_now_s() : 0.0;
        if (!value_ssa_build_from_lower_ir(program, out_program, error)) {
            return 0;
        }
        if (trace_timing) {
            value_ssa_trace_timing("direct-build-from-lower-ir", value_ssa_now_s() - stage_start);
            stage_start = value_ssa_now_s();
        }
        if (!value_ssa_run_indirect_memory_direct_fast_cleanup(out_program, error)) {
            value_ssa_program_free(out_program);
            return 0;
        }
        if (trace_timing) {
            value_ssa_trace_timing("indirect-direct-fast-cleanup", value_ssa_now_s() - stage_start);
            value_ssa_trace_timing("total", value_ssa_now_s() - total_start);
        }
        return 1;
    }
    if (has_huge_parameter_hotspot) {
        stage_start = trace_timing ? value_ssa_now_s() : 0.0;
        if (!value_ssa_build_from_lower_ir_with_canonicalization(
                program, VALUE_SSA_LOWER_IR_CANONICALIZE_CLASSIC, out_program, error)) {
            return 0;
        }
        if (trace_timing) {
            value_ssa_trace_timing("huge-params-classic-canonicalized-build", value_ssa_now_s() - stage_start);
            value_ssa_trace_timing("total", value_ssa_now_s() - total_start);
        }
        return 1;
    }
    if (use_extreme_straight_line_fast_path) {
        if (has_huge_parameter_hotspot) {
            stage_start = trace_timing ? value_ssa_now_s() : 0.0;
            if (!value_ssa_build_from_lower_ir_with_canonicalization(
                    program, VALUE_SSA_LOWER_IR_CANONICALIZE_CLASSIC, out_program, error)) {
                return 0;
            }
            if (trace_timing) {
                value_ssa_trace_timing("extreme-classic-canonicalized-build", value_ssa_now_s() - stage_start);
                value_ssa_trace_timing("total", value_ssa_now_s() - total_start);
            }
            return 1;
        }
        stage_start = trace_timing ? value_ssa_now_s() : 0.0;
        if (!value_ssa_build_from_lower_ir(program, out_program, error)) {
            return 0;
        }
        if (trace_timing) {
            value_ssa_trace_timing("extreme-direct-build-from-lower-ir", value_ssa_now_s() - stage_start);
            stage_start = value_ssa_now_s();
        }
        if (!value_ssa_run_extreme_straight_line_fast_cleanup(out_program, error)) {
            value_ssa_program_free(out_program);
            return 0;
        }
        if (trace_timing) {
            value_ssa_trace_timing("extreme-fast-cleanup", value_ssa_now_s() - stage_start);
            value_ssa_trace_timing("total", value_ssa_now_s() - total_start);
        }
        return 1;
    }
    stage_start = trace_timing ? value_ssa_now_s() : 0.0;
    if (!value_ssa_build_from_lower_ir(program, out_program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("default-direct-build-from-lower-ir", value_ssa_now_s() - stage_start);
        stage_start = value_ssa_now_s();
    }
    if (!value_ssa_run_default_direct_binary_cleanup(out_program, error)) {
        value_ssa_program_free(out_program);
        return 0;
    }
    if (trace_timing) {
        value_ssa_trace_timing("default-direct-binary-cleanup", value_ssa_now_s() - stage_start);
        value_ssa_trace_timing("total", value_ssa_now_s() - total_start);
    }
    return 1;
}

int value_ssa_build_memory_value_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    return value_ssa_build_from_lower_ir_with_canonicalization(program,
        VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_VALUE,
        out_program,
        error);
}

int value_ssa_build_memory_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    return value_ssa_build_from_lower_ir_with_canonicalization(program,
        VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_FULL,
        out_program,
        error);
}

#endif
