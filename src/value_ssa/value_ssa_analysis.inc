#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa.c"
#else

void value_ssa_cfg_analysis_init(ValueSsaCfgAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    analysis->block_count = 0;
    analysis->predecessor_matrix = NULL;
    analysis->predecessor_counts = NULL;
    analysis->reachable = NULL;
    analysis->dominates = NULL;
    analysis->immediate_dominator = NULL;
    analysis->dominator_tree_children = NULL;
    analysis->dominator_tree_child_counts = NULL;
    analysis->dominance_frontier = NULL;
    analysis->dominance_frontier_counts = NULL;
}

void value_ssa_cfg_analysis_free(ValueSsaCfgAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    free(analysis->predecessor_matrix);
    free(analysis->predecessor_counts);
    free(analysis->reachable);
    free(analysis->dominates);
    free(analysis->immediate_dominator);
    free(analysis->dominator_tree_children);
    free(analysis->dominator_tree_child_counts);
    free(analysis->dominance_frontier);
    free(analysis->dominance_frontier_counts);
    value_ssa_cfg_analysis_init(analysis);
}

static int value_ssa_analysis_build_predecessor_matrix(const ValueSsaFunction *function,
    unsigned char *predecessor_matrix,
    size_t *predecessor_counts,
    ValueSsaError *error) {
    size_t i;
    size_t block_count;

    if (!function || !predecessor_matrix || !predecessor_counts) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-132: invalid CFG analysis contract");
        return 0;
    }

    block_count = function->block_count;
    for (i = 0; i < block_count; ++i) {
        const ValueSsaBasicBlock *block = &function->blocks[i];
        size_t target;

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            break;
        case VALUE_SSA_TERM_JUMP:
            target = block->terminator.as.jump_target;
            if (!predecessor_matrix[i * block_count + target]) {
                predecessor_matrix[i * block_count + target] = 1;
                ++predecessor_counts[target];
            }
            break;
        case VALUE_SSA_TERM_BRANCH:
            target = block->terminator.as.branch.then_target;
            if (!predecessor_matrix[i * block_count + target]) {
                predecessor_matrix[i * block_count + target] = 1;
                ++predecessor_counts[target];
            }
            target = block->terminator.as.branch.else_target;
            if (!predecessor_matrix[i * block_count + target]) {
                predecessor_matrix[i * block_count + target] = 1;
                ++predecessor_counts[target];
            }
            break;
        default:
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-133: bb.%zu uses unknown terminator kind %d during CFG analysis",
                block->id,
                (int)block->terminator.kind);
            return 0;
        }
    }

    return 1;
}

static int value_ssa_analysis_mark_reachable_blocks(const ValueSsaFunction *function,
    unsigned char *reachable,
    ValueSsaError *error) {
    size_t *stack = NULL;
    size_t stack_size = 0;
    size_t block_count;

    if (!function || !reachable) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-132: invalid CFG analysis contract");
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }

    block_count = function->block_count;
    stack = (size_t *)malloc(block_count * sizeof(size_t));
    if (!stack) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-134: out of memory while computing CFG reachability");
        return 0;
    }

    stack[stack_size++] = 0;
    reachable[0] = 1;

    while (stack_size > 0) {
        const ValueSsaBasicBlock *block = &function->blocks[stack[--stack_size]];

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            break;
        case VALUE_SSA_TERM_JUMP:
            if (!reachable[block->terminator.as.jump_target]) {
                reachable[block->terminator.as.jump_target] = 1;
                stack[stack_size++] = block->terminator.as.jump_target;
            }
            break;
        case VALUE_SSA_TERM_BRANCH:
            if (!reachable[block->terminator.as.branch.then_target]) {
                reachable[block->terminator.as.branch.then_target] = 1;
                stack[stack_size++] = block->terminator.as.branch.then_target;
            }
            if (!reachable[block->terminator.as.branch.else_target]) {
                reachable[block->terminator.as.branch.else_target] = 1;
                stack[stack_size++] = block->terminator.as.branch.else_target;
            }
            break;
        default:
            free(stack);
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-133: bb.%zu uses unknown terminator kind %d during CFG analysis",
                block->id,
                (int)block->terminator.kind);
            return 0;
        }
    }

    free(stack);
    return 1;
}

static int value_ssa_analysis_compute_dominators(const ValueSsaFunction *function,
    const unsigned char *reachable,
    const unsigned char *predecessor_matrix,
    unsigned char *dominates,
    ValueSsaError *error) {
    size_t block_count;
    size_t block_index;
    int changed = 1;
    unsigned char *scratch = NULL;

    block_count = function->block_count;
    scratch = (unsigned char *)malloc(block_count * sizeof(unsigned char));
    if (!scratch) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-137: out of memory while computing dominators");
        return 0;
    }

    for (block_index = 0; block_index < block_count; ++block_index) {
        size_t dom_index;

        if (!reachable[block_index]) {
            continue;
        }

        if (block_index == 0) {
            dominates[block_index * block_count + 0] = 1;
            continue;
        }

        for (dom_index = 0; dom_index < block_count; ++dom_index) {
            dominates[dom_index * block_count + block_index] = reachable[dom_index] ? 1u : 0u;
        }
    }

    while (changed) {
        changed = 0;
        for (block_index = 1; block_index < block_count; ++block_index) {
            size_t pred_index;
            unsigned char found_predecessor = 0;
            size_t dom_index;

            if (!reachable[block_index]) {
                continue;
            }

            for (dom_index = 0; dom_index < block_count; ++dom_index) {
                scratch[dom_index] = reachable[dom_index] ? 1u : 0u;
            }

            for (pred_index = 0; pred_index < block_count; ++pred_index) {
                if (!predecessor_matrix[pred_index * block_count + block_index] || !reachable[pred_index]) {
                    continue;
                }

                if (!found_predecessor) {
                    for (dom_index = 0; dom_index < block_count; ++dom_index) {
                        scratch[dom_index] = dominates[dom_index * block_count + pred_index];
                    }
                    found_predecessor = 1;
                } else {
                    for (dom_index = 0; dom_index < block_count; ++dom_index) {
                        scratch[dom_index] =
                            (unsigned char)(scratch[dom_index] && dominates[dom_index * block_count + pred_index]);
                    }
                }
            }

            scratch[block_index] = 1;
            for (dom_index = 0; dom_index < block_count; ++dom_index) {
                if (dominates[dom_index * block_count + block_index] != scratch[dom_index]) {
                    dominates[dom_index * block_count + block_index] = scratch[dom_index];
                    changed = 1;
                }
            }
        }
    }

    free(scratch);
    return 1;
}

static void value_ssa_analysis_compute_immediate_dominators(const ValueSsaFunction *function,
    const unsigned char *reachable,
    const unsigned char *dominates,
    size_t *immediate_dominator) {
    size_t block_count;
    size_t block_index;

    block_count = function->block_count;
    for (block_index = 0; block_index < block_count; ++block_index) {
        size_t candidate;

        immediate_dominator[block_index] = (size_t)-1;
        if (!reachable[block_index] || block_index == 0) {
            continue;
        }

        for (candidate = 0; candidate < block_count; ++candidate) {
            size_t other;
            int is_immediate = 1;

            if (!reachable[candidate] || candidate == block_index ||
                !dominates[candidate * block_count + block_index]) {
                continue;
            }

            for (other = 0; other < block_count; ++other) {
                if (!reachable[other] || other == block_index || other == candidate ||
                    !dominates[other * block_count + block_index]) {
                    continue;
                }

                if (dominates[candidate * block_count + other]) {
                    is_immediate = 0;
                    break;
                }
            }

            if (is_immediate) {
                immediate_dominator[block_index] = candidate;
                break;
            }
        }
    }
}

static void value_ssa_analysis_compute_dominator_tree(const ValueSsaFunction *function,
    const unsigned char *reachable,
    const size_t *immediate_dominator,
    unsigned char *dominator_tree_children,
    size_t *dominator_tree_child_counts) {
    size_t block_count;
    size_t block_index;

    block_count = function->block_count;
    for (block_index = 1; block_index < block_count; ++block_index) {
        size_t parent;

        if (!reachable[block_index]) {
            continue;
        }

        parent = immediate_dominator[block_index];
        if (parent == (size_t)-1 || parent >= block_count) {
            continue;
        }

        if (!dominator_tree_children[parent * block_count + block_index]) {
            dominator_tree_children[parent * block_count + block_index] = 1;
            ++dominator_tree_child_counts[parent];
        }
    }
}

static void value_ssa_analysis_compute_dominance_frontier(const ValueSsaFunction *function,
    const unsigned char *reachable,
    const unsigned char *predecessor_matrix,
    const unsigned char *dominates,
    unsigned char *dominance_frontier,
    size_t *dominance_frontier_counts) {
    size_t block_count;
    size_t x;

    block_count = function->block_count;
    for (x = 0; x < block_count; ++x) {
        size_t y;

        if (!reachable[x]) {
            continue;
        }

        for (y = 0; y < block_count; ++y) {
            size_t pred;
            int in_frontier = 0;

            if (!reachable[y]) {
                continue;
            }

            for (pred = 0; pred < block_count; ++pred) {
                if (!reachable[pred] || !predecessor_matrix[pred * block_count + y]) {
                    continue;
                }

                if (dominates[x * block_count + pred] &&
                    (x == y || !dominates[x * block_count + y])) {
                    in_frontier = 1;
                    break;
                }
            }

            if (in_frontier) {
                dominance_frontier[x * block_count + y] = 1;
                ++dominance_frontier_counts[x];
            }
        }
    }
}

int value_ssa_compute_cfg_analysis(const ValueSsaFunction *function,
    ValueSsaCfgAnalysis *analysis,
    ValueSsaError *error) {
    size_t block_count;

    if (!function || !analysis) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-132: invalid CFG analysis contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-135: cannot compute CFG analysis for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    value_ssa_cfg_analysis_free(analysis);
    block_count = function->block_count;
    analysis->block_count = block_count;
    if (block_count == 0) {
        return 1;
    }

    analysis->predecessor_matrix = (unsigned char *)calloc(block_count * block_count, sizeof(unsigned char));
    analysis->predecessor_counts = (size_t *)calloc(block_count, sizeof(size_t));
    analysis->reachable = (unsigned char *)calloc(block_count, sizeof(unsigned char));
    analysis->dominates = (unsigned char *)calloc(block_count * block_count, sizeof(unsigned char));
    analysis->immediate_dominator = (size_t *)malloc(block_count * sizeof(size_t));
    analysis->dominator_tree_children = (unsigned char *)calloc(block_count * block_count, sizeof(unsigned char));
    analysis->dominator_tree_child_counts = (size_t *)calloc(block_count, sizeof(size_t));
    analysis->dominance_frontier = (unsigned char *)calloc(block_count * block_count, sizeof(unsigned char));
    analysis->dominance_frontier_counts = (size_t *)calloc(block_count, sizeof(size_t));
    if (!analysis->predecessor_matrix || !analysis->predecessor_counts || !analysis->reachable ||
        !analysis->dominates || !analysis->immediate_dominator ||
        !analysis->dominator_tree_children || !analysis->dominator_tree_child_counts ||
        !analysis->dominance_frontier || !analysis->dominance_frontier_counts) {
        value_ssa_cfg_analysis_free(analysis);
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-136: out of memory while computing CFG analysis for function '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    if (!value_ssa_analysis_build_predecessor_matrix(function,
            analysis->predecessor_matrix,
            analysis->predecessor_counts,
            error) ||
        !value_ssa_analysis_mark_reachable_blocks(function, analysis->reachable, error)) {
        value_ssa_cfg_analysis_free(analysis);
        return 0;
    }

    if (!value_ssa_analysis_compute_dominators(function,
            analysis->reachable,
            analysis->predecessor_matrix,
            analysis->dominates,
            error)) {
        value_ssa_cfg_analysis_free(analysis);
        return 0;
    }
    value_ssa_analysis_compute_immediate_dominators(function,
        analysis->reachable,
        analysis->dominates,
        analysis->immediate_dominator);
    value_ssa_analysis_compute_dominator_tree(function,
        analysis->reachable,
        analysis->immediate_dominator,
        analysis->dominator_tree_children,
        analysis->dominator_tree_child_counts);
    value_ssa_analysis_compute_dominance_frontier(function,
        analysis->reachable,
        analysis->predecessor_matrix,
        analysis->dominates,
        analysis->dominance_frontier,
        analysis->dominance_frontier_counts);
    return 1;
}

int value_ssa_compute_phi_placement(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    const unsigned char *definition_blocks,
    unsigned char *out_phi_blocks,
    ValueSsaError *error) {
    unsigned char *in_worklist = NULL;
    size_t *worklist = NULL;
    size_t worklist_count = 0;
    size_t block_count;
    size_t block_index;

    if (!function || !analysis || !definition_blocks || !out_phi_blocks) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-138: invalid phi-placement contract");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-139: phi-placement analysis block_count=%zu does not match function block_count=%zu",
            analysis->block_count,
            function->block_count);
        return 0;
    }

    block_count = function->block_count;
    in_worklist = (unsigned char *)calloc(block_count, sizeof(unsigned char));
    worklist = (size_t *)malloc(block_count * sizeof(size_t));
    if (!in_worklist || !worklist) {
        free(in_worklist);
        free(worklist);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-140: out of memory while computing phi placement");
        return 0;
    }

    for (block_index = 0; block_index < block_count; ++block_index) {
        out_phi_blocks[block_index] = 0;
        if (!analysis->reachable[block_index] || !definition_blocks[block_index]) {
            continue;
        }

        worklist[worklist_count++] = block_index;
        in_worklist[block_index] = 1;
    }

    while (worklist_count > 0) {
        size_t current = worklist[--worklist_count];
        size_t frontier_block;

        for (frontier_block = 0; frontier_block < block_count; ++frontier_block) {
            if (!analysis->dominance_frontier[current * block_count + frontier_block]) {
                continue;
            }

            if (!out_phi_blocks[frontier_block]) {
                out_phi_blocks[frontier_block] = 1;
                if (!definition_blocks[frontier_block] && !in_worklist[frontier_block]) {
                    worklist[worklist_count++] = frontier_block;
                    in_worklist[frontier_block] = 1;
                }
            }
        }
    }

    free(in_worklist);
    free(worklist);
    return 1;
}

int value_ssa_compute_dominator_tree_preorder(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t *out_order,
    size_t *out_count,
    ValueSsaError *error) {
    size_t *stack = NULL;
    size_t stack_size = 0;
    size_t write_index = 0;
    size_t block_count;

    if (!function || !analysis || !out_order || !out_count) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-141: invalid dominator-tree preorder contract");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-142: dominator-tree preorder analysis block_count=%zu does not match function block_count=%zu",
            analysis->block_count,
            function->block_count);
        return 0;
    }

    block_count = function->block_count;
    *out_count = 0;
    if (block_count == 0) {
        return 1;
    }
    if (!analysis->reachable[0]) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-143: dominator-tree preorder requires reachable entry block");
        return 0;
    }

    stack = (size_t *)malloc(block_count * sizeof(size_t));
    if (!stack) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-144: out of memory while computing dominator-tree preorder");
        return 0;
    }

    stack[stack_size++] = 0;
    while (stack_size > 0) {
        size_t block_index = stack[--stack_size];
        size_t child_index = block_count;

        out_order[write_index++] = block_index;
        while (child_index > 0) {
            --child_index;
            if (analysis->dominator_tree_children[block_index * block_count + child_index]) {
                stack[stack_size++] = child_index;
            }
        }
    }

    free(stack);
    *out_count = write_index;
    return 1;
}

#endif
