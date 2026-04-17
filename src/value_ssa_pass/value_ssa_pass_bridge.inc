#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

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
        if (!value_ssa_canonicalize_program(out_program, error) ||
            !value_ssa_canonicalize_program(out_program, error)) {
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
    return value_ssa_build_from_lower_ir_with_canonicalization(
        program, VALUE_SSA_LOWER_IR_CANONICALIZE_DEFAULT, out_program, error);
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
