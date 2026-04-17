#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_PASS_SPLIT_AGGREGATOR)
#define MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa_pass.c"
#else

int memory_ssa_pass_scalar_replace_local_slots(ValueSsaProgram *program, ValueSsaError *error) {
    char *before_text = NULL;
    char *after_text = NULL;
    size_t iteration;
    int ok = 0;

    if (!program) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-057: invalid local-slot scalar-replacement contract");
        return 0;
    }
    if (!value_ssa_verify_program(program, error)) {
        return 0;
    }

    for (iteration = 0; iteration < 8; ++iteration) {
        free(before_text);
        free(after_text);
        before_text = NULL;
        after_text = NULL;

        if (!memory_ssa_pass_dump_program_copy(program, &before_text, error) ||
            !memory_ssa_pass_run_one_step("promote-local-slots", memory_ssa_pass_promote_local_slots, program, error) ||
            !memory_ssa_pass_run_one_step("forward-local-loads", memory_ssa_pass_forward_local_loads, program, error) ||
            !memory_ssa_pass_run_one_step(
                "eliminate-redundant-local-stores", memory_ssa_pass_eliminate_redundant_local_stores, program, error) ||
            !memory_ssa_pass_run_one_step(
                "eliminate-dead-local-stores", memory_ssa_pass_eliminate_dead_local_stores, program, error) ||
            !memory_ssa_pass_run_one_step("simplify-trivial-values", value_ssa_simplify_trivial_values, program, error) ||
            !memory_ssa_pass_run_one_step("eliminate-dead-value-defs", value_ssa_eliminate_dead_value_defs, program, error) ||
            !memory_ssa_pass_run_one_step("simplify-cfg", value_ssa_simplify_cfg, program, error) ||
            !memory_ssa_pass_run_one_step(
                "simplify-trivial-values-post-cfg", value_ssa_simplify_trivial_values, program, error) ||
            !memory_ssa_pass_run_one_step(
                "eliminate-dead-value-defs-post-cfg", value_ssa_eliminate_dead_value_defs, program, error) ||
            !memory_ssa_pass_dump_program_copy(program, &after_text, error)) {
            goto cleanup;
        }

        if (strcmp(before_text, after_text) == 0) {
            ok = 1;
            goto cleanup;
        }
    }

    memory_ssa_pass_set_error(error,
        0,
        0,
        "MEMORY-SSA-PASS-058: local-slot scalar replacement did not converge within %d iterations",
        8);

cleanup:
    free(before_text);
    free(after_text);
    return ok;
}

#endif
