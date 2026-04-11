#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

int value_ssa_canonicalize_program(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-172: invalid canonicalize contract");
        return 0;
    }
    if (!value_ssa_verify_program(program, error)) {
        return 0;
    }

    if (!value_ssa_simplify_trivial_values(program, error) ||
        !value_ssa_forward_local_loads(program, error) ||
        !value_ssa_forward_global_loads(program, error) ||
        !value_ssa_simplify_trivial_values(program, error) ||
        !value_ssa_eliminate_redundant_stores(program, error) ||
        !value_ssa_eliminate_dead_stores(program, error) ||
        !value_ssa_normalize_binary_operands(program, error) ||
        !value_ssa_simplify_algebraic_identities(program, error) ||
        !value_ssa_fold_constants(program, error) ||
        !value_ssa_simplify_trivial_values(program, error) ||
        !value_ssa_eliminate_redundant_binaries(program, error) ||
        !value_ssa_simplify_trivial_values(program, error) ||
        !value_ssa_eliminate_dead_value_defs(program, error) ||
        !value_ssa_simplify_cfg(program, error) ||
        !value_ssa_eliminate_redundant_stores(program, error) ||
        !value_ssa_eliminate_dead_stores(program, error) ||
        !value_ssa_simplify_cfg(program, error) ||
        !value_ssa_eliminate_dead_value_defs(program, error)) {
        return 0;
    }

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

int value_ssa_build_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    if (!program || !out_program) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-122: invalid lower-ir canonicalized conversion entry contract");
        return 0;
    }

    if (!value_ssa_build_from_lower_ir(program, out_program, error)) {
        return 0;
    }

    if (!value_ssa_canonicalize_program(out_program, error) ||
        !value_ssa_canonicalize_program(out_program, error)) {
        value_ssa_program_free(out_program);
        return 0;
    }

    return 1;
}

#endif
