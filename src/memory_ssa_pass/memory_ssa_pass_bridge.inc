#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_PASS_SPLIT_AGGREGATOR)
#define MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa_pass.c"
#else

static int memory_ssa_pass_build_from_lower_ir_with_pass(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    int (*pass_fn)(ValueSsaProgram *, ValueSsaError *),
    const char *invalid_error_code,
    ValueSsaError *error) {
    if (!program || !out_program) {
        memory_ssa_pass_set_error(error,
            0,
            0,
            "%s",
            invalid_error_code ? invalid_error_code : "MEMORY-SSA-PASS-041: invalid lower-ir build contract");
        return 0;
    }
    if (!pass_fn) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-043: invalid lower-ir build pass callback");
        return 0;
    }

    if (!value_ssa_build_from_lower_ir(program, out_program, error)) {
        return 0;
    }
    if (!pass_fn(out_program, error)) {
        value_ssa_program_free(out_program);
        return 0;
    }

    return 1;
}

int memory_ssa_pass_build_memory_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    return memory_ssa_pass_build_from_lower_ir_with_pass(program,
        out_program,
        memory_ssa_pass_canonicalize_memory_values,
        "MEMORY-SSA-PASS-042: invalid lower-ir memory-value canonicalized build contract",
        error);
}

int memory_ssa_pass_build_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error) {
    return memory_ssa_pass_build_from_lower_ir_with_pass(program,
        out_program,
        memory_ssa_pass_run_pipeline,
        "MEMORY-SSA-PASS-041: invalid lower-ir canonicalized build contract",
        error);
}

#endif
