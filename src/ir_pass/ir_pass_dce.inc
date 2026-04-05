#if defined(__INTELLISENSE__) && !defined(IR_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_PASS_SPLIT_AGGREGATOR)
#define IR_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "ir_pass.c"
#else

#ifndef IR_PASS_SPLIT_AGGREGATOR
#include "ir_pass.h"

#include <stdlib.h>
#endif

int ir_pass_eliminate_dead_temp_defs(IrProgram *program, IrError *error) {
    size_t function_index;

    if (!program) {
        ir_pass_set_error(error, 0, 0, "IR-PASS-008: null program for dead-temp elimination");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        IrFunction *function = &program->functions[function_index];
        int changed;

        if (!function->has_body || function->next_temp_id == 0) {
            continue;
        }
        if (function->block_count > 0 && !function->blocks) {
            ir_pass_set_error(error,
                0,
                0,
                "IR-PASS-009: malformed function '%s' block table",
                function->name ? function->name : "<unnamed>");
            return 0;
        }
        if (function->next_temp_id > ((size_t)-1) / sizeof(size_t)) {
            ir_pass_set_error(error,
                0,
                0,
                "IR-PASS-017: function '%s' temp-use table capacity overflow",
                function->name ? function->name : "<unnamed>");
            return 0;
        }

        do {
            IrPassTempAnalysis analysis;
            size_t block_index;

            changed = 0;
            if (!ir_pass_build_temp_analysis(function, &analysis, error)) {
                return 0;
            }

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                IrBasicBlock *block = &function->blocks[block_index];
                size_t instruction_index;
                size_t write_index = 0;

                for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                    IrInstruction *instruction = &block->instructions[instruction_index];

                    if (ir_pass_is_removable_dead_temp_def(instruction,
                            analysis.use_counts,
                            analysis.temp_count)) {
                        changed = 1;
                        continue;
                    }

                    if (write_index != instruction_index) {
                        block->instructions[write_index] = block->instructions[instruction_index];
                    }
                    ++write_index;
                }

                block->instruction_count = write_index;
            }

            ir_pass_temp_analysis_free(&analysis);
        } while (changed);

        if (!ir_pass_compact_function_temp_ids(function, error)) {
            return 0;
        }
    }

    return 1;
}

#endif
