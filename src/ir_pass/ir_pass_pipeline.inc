#if defined(__INTELLISENSE__) && !defined(IR_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_PASS_SPLIT_AGGREGATOR)
#define IR_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "ir_pass.c"
#else

#ifndef IR_PASS_SPLIT_AGGREGATOR
#include "ir_pass.h"
#endif

int ir_pass_run_pipeline(IrProgram *program,
    const IrPassSpec *passes,
    size_t pass_count,
    IrError *error) {
    size_t i;

    if (!program) {
        ir_pass_set_error(error, 0, 0, "IR-PASS-001: null program");
        return 0;
    }

    if (pass_count > 0 && !passes) {
        ir_pass_set_error(error, 0, 0, "IR-PASS-002: null pass table with non-zero pass_count");
        return 0;
    }

    for (i = 0; i < pass_count; ++i) {
        const IrPassSpec *pass = &passes[i];

        if (!pass->name || pass->name[0] == '\0' || !pass->run) {
            ir_pass_set_error(error,
                0,
                0,
                "IR-PASS-003: invalid pass spec at index %zu",
                i);
            return 0;
        }

        if (!pass->run(program, error)) {
            if (!error || error->message[0] == '\0') {
                ir_pass_set_error(error,
                    0,
                    0,
                    "IR-PASS-004: pass '%s' failed",
                    pass->name);
            }
            return 0;
        }
    }

    return 1;
}

int ir_pass_run_default_pipeline(IrProgram *program, IrError *error) {
    static const IrPassSpec k_default_passes[] = {
        {"fold-immediate-binary", ir_pass_fold_immediate_binary},
        {"propagate-temp-constants", ir_pass_propagate_temp_constants},
        {"propagate-temp-copies", ir_pass_propagate_temp_copies},
        {"fold-immediate-binary", ir_pass_fold_immediate_binary},
        {"propagate-temp-constants", ir_pass_propagate_temp_constants},
        {"propagate-temp-copies", ir_pass_propagate_temp_copies},
        {"simplify-cfg", ir_pass_simplify_cfg},
        {"eliminate-dead-temp-defs", ir_pass_eliminate_dead_temp_defs},
        {"simplify-cfg", ir_pass_simplify_cfg},
        {"eliminate-dead-temp-defs", ir_pass_eliminate_dead_temp_defs},
    };

    return ir_pass_run_pipeline(program,
        k_default_passes,
        sizeof(k_default_passes) / sizeof(k_default_passes[0]),
        error);
}

#endif
