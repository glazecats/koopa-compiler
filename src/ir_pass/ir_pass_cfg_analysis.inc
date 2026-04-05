#if defined(__INTELLISENSE__) && !defined(IR_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_PASS_SPLIT_AGGREGATOR)
#define IR_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "ir_pass.c"
#else

#ifndef IR_PASS_SPLIT_AGGREGATOR
#include "ir_pass.h"

#include <stdlib.h>
#endif

static void ir_pass_cfg_analysis_free(IrPassCfgAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    free(analysis->reachable);
    free(analysis->predecessor_counts);
    analysis->block_count = 0;
    analysis->reachable = NULL;
    analysis->predecessor_counts = NULL;
}

static int ir_pass_build_cfg_analysis(const IrFunction *function,
    IrPassCfgAnalysis *analysis,
    IrError *error) {
    size_t *stack;
    size_t stack_size = 0;
    size_t block_index;

    if (!function || !analysis) {
        return 0;
    }

    analysis->block_count = 0;
    analysis->reachable = NULL;
    analysis->predecessor_counts = NULL;
    if (function->block_count == 0) {
        return 1;
    }

    analysis->block_count = function->block_count;
    analysis->reachable = (unsigned char *)calloc(analysis->block_count, sizeof(unsigned char));
    analysis->predecessor_counts = (size_t *)calloc(analysis->block_count, sizeof(size_t));
    stack = (size_t *)malloc(analysis->block_count * sizeof(size_t));
    if (!analysis->reachable || !analysis->predecessor_counts || !stack) {
        free(stack);
        ir_pass_cfg_analysis_free(analysis);
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-026: out of memory building CFG analysis for '%s'",
            function->name ? function->name : "<unnamed>");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const IrBasicBlock *block = &function->blocks[block_index];

        if (!block->has_terminator) {
            continue;
        }

        if (block->terminator.kind == IR_TERM_JUMP) {
            size_t target = block->terminator.as.jump_target;

            if (target >= function->block_count) {
                free(stack);
                ir_pass_cfg_analysis_free(analysis);
                ir_pass_set_error(error,
                    0,
                    0,
                    "IR-PASS-027: function '%s' jump target bb.%zu is out of range",
                    function->name ? function->name : "<unnamed>",
                    target);
                return 0;
            }
            analysis->predecessor_counts[target] += 1;
        } else if (block->terminator.kind == IR_TERM_BRANCH) {
            size_t then_target = block->terminator.as.branch.then_target;
            size_t else_target = block->terminator.as.branch.else_target;

            if (then_target >= function->block_count || else_target >= function->block_count) {
                free(stack);
                ir_pass_cfg_analysis_free(analysis);
                ir_pass_set_error(error,
                    0,
                    0,
                    "IR-PASS-028: function '%s' branch target is out of range",
                    function->name ? function->name : "<unnamed>");
                return 0;
            }
            analysis->predecessor_counts[then_target] += 1;
            analysis->predecessor_counts[else_target] += 1;
        }
    }

    analysis->reachable[0] = 1u;
    stack[stack_size++] = 0;
    while (stack_size > 0) {
        size_t block_id = stack[--stack_size];
        const IrBasicBlock *block = &function->blocks[block_id];

        if (!block->has_terminator) {
            continue;
        }

        if (block->terminator.kind == IR_TERM_JUMP) {
            size_t target = block->terminator.as.jump_target;

            if (!analysis->reachable[target]) {
                analysis->reachable[target] = 1u;
                stack[stack_size++] = target;
            }
        } else if (block->terminator.kind == IR_TERM_BRANCH) {
            size_t then_target = block->terminator.as.branch.then_target;
            size_t else_target = block->terminator.as.branch.else_target;

            if (!analysis->reachable[then_target]) {
                analysis->reachable[then_target] = 1u;
                stack[stack_size++] = then_target;
            }
            if (!analysis->reachable[else_target]) {
                analysis->reachable[else_target] = 1u;
                stack[stack_size++] = else_target;
            }
        }
    }

    free(stack);
    return 1;
}

#endif
