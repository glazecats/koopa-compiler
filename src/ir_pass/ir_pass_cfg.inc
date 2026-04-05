#if defined(__INTELLISENSE__) && !defined(IR_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_PASS_SPLIT_AGGREGATOR)
#define IR_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "ir_pass.c"
#else

#ifndef IR_PASS_SPLIT_AGGREGATOR
#include "ir_pass.h"

#include <stdlib.h>
#endif

static void ir_pass_basic_block_free(IrBasicBlock *block) {
    size_t instruction_index;

    if (!block) {
        return;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        if (block->instructions[instruction_index].kind == IR_INSTR_CALL) {
            free(block->instructions[instruction_index].as.call.callee_name);
            free(block->instructions[instruction_index].as.call.args);
        }
    }

    free(block->instructions);
    block->instructions = NULL;
    block->instruction_count = 0;
    block->instruction_capacity = 0;
    block->has_terminator = 0;
}

static size_t ir_pass_resolve_empty_jump_target(const IrFunction *function, size_t target) {
    size_t depth = 0;

    if (!function || target >= function->block_count) {
        return target;
    }

    while (depth < function->block_count) {
        const IrBasicBlock *block = &function->blocks[target];
        size_t next_target;

        if (block->instruction_count != 0
            || !block->has_terminator
            || block->terminator.kind != IR_TERM_JUMP) {
            break;
        }

        next_target = block->terminator.as.jump_target;
        if (next_target >= function->block_count || next_target == target) {
            break;
        }

        target = next_target;
        depth += 1;
    }

    return target;
}

static int ir_pass_try_fold_jump_to_return(IrBasicBlock *block, const IrFunction *function) {
    const IrBasicBlock *target_block;

    if (!block || !function || !block->has_terminator || block->terminator.kind != IR_TERM_JUMP) {
        return 0;
    }
    if (block->terminator.as.jump_target >= function->block_count) {
        return 0;
    }

    target_block = &function->blocks[block->terminator.as.jump_target];
    if (target_block->instruction_count != 0
        || !target_block->has_terminator
        || target_block->terminator.kind != IR_TERM_RETURN) {
        return 0;
    }

    block->terminator.kind = IR_TERM_RETURN;
    block->terminator.as.return_value = target_block->terminator.as.return_value;
    return 1;
}

static int ir_pass_try_fold_branch_to_same_return(IrBasicBlock *block,
    const IrFunction *function,
    const IrPassTempAnalysis *temp_analysis,
    IrError *error,
    int *out_changed) {
    const IrBasicBlock *then_block;
    const IrBasicBlock *else_block;
    IrValueRef then_value;
    IrValueRef else_value;
    IrValueRef resolved_then;
    IrValueRef resolved_else;

    if (out_changed) {
        *out_changed = 0;
    }

    if (!block || !function || !block->has_terminator || block->terminator.kind != IR_TERM_BRANCH) {
        return 1;
    }
    if (block->terminator.as.branch.then_target >= function->block_count
        || block->terminator.as.branch.else_target >= function->block_count) {
        return 1;
    }

    then_block = &function->blocks[block->terminator.as.branch.then_target];
    else_block = &function->blocks[block->terminator.as.branch.else_target];
    if (then_block->instruction_count != 0 || else_block->instruction_count != 0
        || !then_block->has_terminator || !else_block->has_terminator
        || then_block->terminator.kind != IR_TERM_RETURN
        || else_block->terminator.kind != IR_TERM_RETURN) {
        return 1;
    }

    then_value = then_block->terminator.as.return_value;
    else_value = else_block->terminator.as.return_value;
    if (!ir_pass_resolve_equivalent_value(then_value,
            temp_analysis,
            function->next_temp_id,
            function->name,
            error,
            &resolved_then)
        || !ir_pass_resolve_equivalent_value(else_value,
            temp_analysis,
            function->next_temp_id,
            function->name,
            error,
            &resolved_else)) {
        return 0;
    }
    if (!ir_pass_value_ref_equals(resolved_then, resolved_else)) {
        return 1;
    }

    block->terminator.kind = IR_TERM_RETURN;
    block->terminator.as.return_value = resolved_then;
    if (out_changed) {
        *out_changed = 1;
    }
    return 1;
}

static int ir_pass_append_block_instructions(IrBasicBlock *block,
    IrBasicBlock *target_block,
    const char *function_name,
    IrError *error) {
    size_t combined_count;
    IrInstruction *new_instructions;

    if (!block || !target_block) {
        return 0;
    }
    if (target_block->instruction_count == 0) {
        return 1;
    }
    if (block->instruction_count > ((size_t)-1) - target_block->instruction_count) {
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-036: function '%s' instruction merge count overflow",
            function_name ? function_name : "<unnamed>");
        return 0;
    }

    combined_count = block->instruction_count + target_block->instruction_count;
    new_instructions = (IrInstruction *)realloc(block->instructions,
        combined_count * sizeof(IrInstruction));
    if (!new_instructions) {
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-037: out of memory merging block instructions for '%s'",
            function_name ? function_name : "<unnamed>");
        return 0;
    }

    block->instructions = new_instructions;
    memcpy(block->instructions + block->instruction_count,
        target_block->instructions,
        target_block->instruction_count * sizeof(IrInstruction));
    block->instruction_count = combined_count;
    block->instruction_capacity = combined_count;

    free(target_block->instructions);
    target_block->instructions = NULL;
    target_block->instruction_count = 0;
    target_block->instruction_capacity = 0;
    return 1;
}

static int ir_pass_try_merge_single_predecessor_jump_block(IrBasicBlock *block,
    IrFunction *function,
    const size_t *predecessor_counts,
    IrError *error,
    int *out_changed) {
    IrBasicBlock *target_block;
    size_t target;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!block || !function || !predecessor_counts || !block->has_terminator
        || block->terminator.kind != IR_TERM_JUMP) {
        return 1;
    }

    target = block->terminator.as.jump_target;
    if (target >= function->block_count || target == block->id || predecessor_counts[target] != 1u) {
        return 1;
    }

    target_block = &function->blocks[target];
    if (!target_block->has_terminator) {
        return 1;
    }

    if (!ir_pass_append_block_instructions(block,
            target_block,
            function->name,
            error)) {
        return 0;
    }

    block->terminator = target_block->terminator;
    target_block->has_terminator = 0;
    if (out_changed) {
        *out_changed = 1;
    }
    return 1;
}

static int ir_pass_compact_function_blocks(IrFunction *function,
    const IrPassCfgAnalysis *analysis,
    IrError *error) {
    size_t *block_remap;
    IrBasicBlock *new_blocks;
    size_t old_block_id;
    size_t reachable_count = 0;
    size_t old_block_count;

    if (!function || !analysis) {
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }

    old_block_count = function->block_count;

    block_remap = (size_t *)malloc(old_block_count * sizeof(size_t));
    if (!block_remap) {
        free(block_remap);
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-029: out of memory compacting CFG for '%s'",
            function->name ? function->name : "<unnamed>");
        return 0;
    }

    for (old_block_id = 0; old_block_id < old_block_count; ++old_block_id) {
        if (analysis->reachable && analysis->reachable[old_block_id]) {
            block_remap[old_block_id] = reachable_count;
            reachable_count += 1;
        } else {
            block_remap[old_block_id] = (size_t)-1;
        }
    }

    if (reachable_count == old_block_count) {
        free(block_remap);
        return 1;
    }

    new_blocks = (IrBasicBlock *)calloc(reachable_count, sizeof(IrBasicBlock));
    if (!new_blocks) {
        free(block_remap);
        ir_pass_set_error(error,
            0,
            0,
            "IR-PASS-030: out of memory allocating compacted CFG for '%s'",
            function->name ? function->name : "<unnamed>");
        return 0;
    }

    for (old_block_id = 0; old_block_id < old_block_count; ++old_block_id) {
        if (analysis->reachable && analysis->reachable[old_block_id]) {
            size_t new_block_id = block_remap[old_block_id];

            new_blocks[new_block_id] = function->blocks[old_block_id];
            new_blocks[new_block_id].id = new_block_id;
        } else {
            ir_pass_basic_block_free(&function->blocks[old_block_id]);
        }
    }

    free(function->blocks);
    function->blocks = new_blocks;
    function->block_count = reachable_count;
    function->block_capacity = reachable_count;

    for (old_block_id = 0; old_block_id < function->block_count; ++old_block_id) {
        IrBasicBlock *block = &function->blocks[old_block_id];

        if (!block->has_terminator) {
            continue;
        }
        if (block->terminator.kind == IR_TERM_JUMP) {
            size_t old_target = block->terminator.as.jump_target;

            if (old_target >= old_block_count || block_remap[old_target] == (size_t)-1) {
                free(block_remap);
                ir_pass_set_error(error,
                    0,
                    0,
                    "IR-PASS-031: function '%s' jump target remap failed",
                    function->name ? function->name : "<unnamed>");
                return 0;
            }
            block->terminator.as.jump_target = block_remap[old_target];
        } else if (block->terminator.kind == IR_TERM_BRANCH) {
            size_t old_then = block->terminator.as.branch.then_target;
            size_t old_else = block->terminator.as.branch.else_target;

            if (old_then >= old_block_count || old_else >= old_block_count
                || block_remap[old_then] == (size_t)-1 || block_remap[old_else] == (size_t)-1) {
                free(block_remap);
                ir_pass_set_error(error,
                    0,
                    0,
                    "IR-PASS-032: function '%s' branch target remap failed",
                    function->name ? function->name : "<unnamed>");
                return 0;
            }
            block->terminator.as.branch.then_target = block_remap[old_then];
            block->terminator.as.branch.else_target = block_remap[old_else];
        }
    }

    free(block_remap);
    return 1;
}

int ir_pass_simplify_cfg(IrProgram *program, IrError *error) {
    size_t function_index;

    if (!program) {
        ir_pass_set_error(error, 0, 0, "IR-PASS-033: null program for CFG simplification");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        IrFunction *function = &program->functions[function_index];
        IrPassCfgAnalysis analysis;
        IrPassTempAnalysis temp_analysis;
        int changed;

        if (!function->has_body || function->block_count == 0) {
            continue;
        }
        if (!function->blocks) {
            ir_pass_set_error(error,
                0,
                0,
                "IR-PASS-025: malformed function '%s' block table",
                function->name ? function->name : "<unnamed>");
            return 0;
        }

        do {
            size_t block_index;

            changed = 0;
            if (!ir_pass_build_cfg_analysis(function, &analysis, error)) {
                return 0;
            }
            if (!ir_pass_build_temp_analysis(function, &temp_analysis, error)) {
                ir_pass_cfg_analysis_free(&analysis);
                return 0;
            }
            for (block_index = 0; block_index < function->block_count; ++block_index) {
                IrBasicBlock *block = &function->blocks[block_index];

                if (!block->has_terminator) {
                    continue;
                }

                if (block->terminator.kind == IR_TERM_JUMP) {
                    size_t threaded_target = ir_pass_resolve_empty_jump_target(function,
                        block->terminator.as.jump_target);

                    if (threaded_target != block->terminator.as.jump_target) {
                        block->terminator.as.jump_target = threaded_target;
                        changed = 1;
                    }
                    if (ir_pass_try_fold_jump_to_return(block, function)) {
                        changed = 1;
                    }
                    {
                        int merged = 0;

                        if (!ir_pass_try_merge_single_predecessor_jump_block(block,
                                function,
                                analysis.predecessor_counts,
                                error,
                                &merged)) {
                            ir_pass_temp_analysis_free(&temp_analysis);
                            ir_pass_cfg_analysis_free(&analysis);
                            return 0;
                        }
                        if (merged) {
                            changed = 1;
                        }
                    }
                    continue;
                }

                if (block->terminator.kind != IR_TERM_BRANCH) {
                    continue;
                }

                {
                    size_t threaded_then = ir_pass_resolve_empty_jump_target(function,
                        block->terminator.as.branch.then_target);
                    size_t threaded_else = ir_pass_resolve_empty_jump_target(function,
                        block->terminator.as.branch.else_target);

                    if (threaded_then != block->terminator.as.branch.then_target) {
                        block->terminator.as.branch.then_target = threaded_then;
                        changed = 1;
                    }
                    if (threaded_else != block->terminator.as.branch.else_target) {
                        block->terminator.as.branch.else_target = threaded_else;
                        changed = 1;
                    }
                }

                {
                    int folded_same_return = 0;

                    if (!ir_pass_try_fold_branch_to_same_return(block,
                            function,
                            &temp_analysis,
                            error,
                            &folded_same_return)) {
                        ir_pass_temp_analysis_free(&temp_analysis);
                        ir_pass_cfg_analysis_free(&analysis);
                        return 0;
                    }
                    if (folded_same_return) {
                        changed = 1;
                        continue;
                    }
                }

                if (block->terminator.as.branch.then_target == block->terminator.as.branch.else_target) {
                    block->terminator.kind = IR_TERM_JUMP;
                    block->terminator.as.jump_target = block->terminator.as.branch.then_target;
                    changed = 1;
                    if (ir_pass_try_fold_jump_to_return(block, function)) {
                        changed = 1;
                    }
                    {
                        int merged = 0;

                        if (!ir_pass_try_merge_single_predecessor_jump_block(block,
                                function,
                                analysis.predecessor_counts,
                                error,
                                &merged)) {
                            ir_pass_temp_analysis_free(&temp_analysis);
                            ir_pass_cfg_analysis_free(&analysis);
                            return 0;
                        }
                        if (merged) {
                            changed = 1;
                        }
                    }
                    continue;
                }

                if (block->terminator.as.branch.condition.kind == IR_VALUE_IMMEDIATE) {
                    block->terminator.kind = IR_TERM_JUMP;
                    block->terminator.as.jump_target =
                        block->terminator.as.branch.condition.immediate != 0
                        ? block->terminator.as.branch.then_target
                        : block->terminator.as.branch.else_target;
                    changed = 1;
                    if (ir_pass_try_fold_jump_to_return(block, function)) {
                        changed = 1;
                    }
                    {
                        int merged = 0;

                        if (!ir_pass_try_merge_single_predecessor_jump_block(block,
                                function,
                                analysis.predecessor_counts,
                                error,
                                &merged)) {
                            ir_pass_temp_analysis_free(&temp_analysis);
                            ir_pass_cfg_analysis_free(&analysis);
                            return 0;
                        }
                        if (merged) {
                            changed = 1;
                        }
                    }
                }
            }

            ir_pass_temp_analysis_free(&temp_analysis);
            ir_pass_cfg_analysis_free(&analysis);
        } while (changed);

        if (!ir_pass_build_cfg_analysis(function, &analysis, error)) {
            return 0;
        }
        if (!ir_pass_compact_function_blocks(function, &analysis, error)) {
            ir_pass_cfg_analysis_free(&analysis);
            return 0;
        }
        ir_pass_cfg_analysis_free(&analysis);

        if (!ir_pass_compact_function_temp_ids(function, error)) {
            return 0;
        }
    }

    return 1;
}

#endif