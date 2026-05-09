#if defined(__INTELLISENSE__) && !defined(LOWER_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(LOWER_IR_SPLIT_AGGREGATOR)
#define LOWER_IR_SPLIT_INTELLISENSE_CONTEXT
#include "lower_ir.c"
#else

void lower_ir_cfg_analysis_init(LowerIrCfgAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    analysis->block_count = 0;
    analysis->predecessor_matrix = NULL;
    analysis->predecessor_counts = NULL;
    analysis->predecessors = NULL;
    analysis->successor_counts = NULL;
    analysis->successors = NULL;
    analysis->reachable = NULL;
    analysis->dominates = NULL;
    analysis->immediate_dominator = NULL;
    analysis->dominator_tree_children = NULL;
    analysis->dominator_tree_child_counts = NULL;
    analysis->dominance_frontier = NULL;
    analysis->dominance_frontier_counts = NULL;
}

void lower_ir_cfg_analysis_free(LowerIrCfgAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    free(analysis->predecessor_matrix);
    free(analysis->predecessor_counts);
    free(analysis->predecessors);
    free(analysis->successor_counts);
    free(analysis->successors);
    free(analysis->reachable);
    free(analysis->dominates);
    free(analysis->immediate_dominator);
    free(analysis->dominator_tree_children);
    free(analysis->dominator_tree_child_counts);
    free(analysis->dominance_frontier);
    free(analysis->dominance_frontier_counts);
    lower_ir_cfg_analysis_init(analysis);
}

static int lower_ir_analysis_build_predecessor_matrix(const LowerIrFunction *function,
    unsigned char *predecessor_matrix,
    size_t *predecessor_counts,
    LowerIrError *error) {
    size_t block_index;
    size_t block_count;

    if (!function || !predecessor_matrix || !predecessor_counts) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-041: invalid CFG analysis contract");
        return 0;
    }

    block_count = function->block_count;
    for (block_index = 0; block_index < block_count; ++block_index) {
        const LowerIrBasicBlock *block = &function->blocks[block_index];
        size_t target;

        switch (block->terminator.kind) {
        case LOWER_IR_TERM_RETURN:
            break;
        case LOWER_IR_TERM_JUMP:
            target = block->terminator.as.jump_target;
            if (!predecessor_matrix[block_index * block_count + target]) {
                predecessor_matrix[block_index * block_count + target] = 1;
                ++predecessor_counts[target];
            }
            break;
        case LOWER_IR_TERM_BRANCH:
            target = block->terminator.as.branch.then_target;
            if (!predecessor_matrix[block_index * block_count + target]) {
                predecessor_matrix[block_index * block_count + target] = 1;
                ++predecessor_counts[target];
            }
            target = block->terminator.as.branch.else_target;
            if (!predecessor_matrix[block_index * block_count + target]) {
                predecessor_matrix[block_index * block_count + target] = 1;
                ++predecessor_counts[target];
            }
            break;
        default:
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-042: bb.%zu uses unknown terminator kind %d during CFG analysis",
                block->id,
                (int)block->terminator.kind);
            return 0;
        }
    }

    return 1;
}

static int lower_ir_analysis_build_successor_lists(const LowerIrFunction *function,
    size_t *successor_counts,
    size_t *successors,
    LowerIrError *error) {
    size_t block_index;

    if (!function || !successor_counts || !successors) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-041: invalid CFG analysis contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const LowerIrBasicBlock *block = &function->blocks[block_index];

        switch (block->terminator.kind) {
        case LOWER_IR_TERM_RETURN:
            successor_counts[block_index] = 0;
            break;
        case LOWER_IR_TERM_JUMP:
            successors[block_index * function->block_count] = block->terminator.as.jump_target;
            successor_counts[block_index] = 1;
            break;
        case LOWER_IR_TERM_BRANCH:
            successors[block_index * function->block_count] = block->terminator.as.branch.then_target;
            if (block->terminator.as.branch.else_target != block->terminator.as.branch.then_target) {
                successors[block_index * function->block_count + 1] = block->terminator.as.branch.else_target;
                successor_counts[block_index] = 2;
            } else {
                successor_counts[block_index] = 1;
            }
            break;
        default:
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-042: bb.%zu uses unknown terminator kind %d during CFG analysis",
                block->id,
                (int)block->terminator.kind);
            return 0;
        }
    }

    return 1;
}

static void lower_ir_analysis_build_predecessor_lists(const LowerIrFunction *function,
    const unsigned char *predecessor_matrix,
    const size_t *predecessor_counts,
    size_t *predecessors) {
    size_t block_index;
    size_t block_count;

    if (!function || !predecessor_matrix || !predecessor_counts || !predecessors) {
        return;
    }

    block_count = function->block_count;
    for (block_index = 0; block_index < block_count; ++block_index) {
        size_t write_index = 0;
        size_t pred_index;

        for (pred_index = 0; pred_index < block_count; ++pred_index) {
            if (!predecessor_matrix[pred_index * block_count + block_index]) {
                continue;
            }
            predecessors[block_index * block_count + write_index++] = pred_index;
        }
        while (write_index < predecessor_counts[block_index]) {
            predecessors[block_index * block_count + write_index++] = 0;
        }
    }
}

static int lower_ir_analysis_mark_reachable_blocks(const LowerIrFunction *function,
    unsigned char *reachable,
    LowerIrError *error) {
    size_t *stack = NULL;
    size_t stack_size = 0;
    size_t block_count;

    if (!function || !reachable) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-041: invalid CFG analysis contract");
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }

    block_count = function->block_count;
    stack = (size_t *)malloc(block_count * sizeof(size_t));
    if (!stack) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-043: out of memory while computing CFG reachability");
        return 0;
    }

    stack[stack_size++] = 0;
    reachable[0] = 1;

    while (stack_size > 0) {
        const LowerIrBasicBlock *block = &function->blocks[stack[--stack_size]];

        switch (block->terminator.kind) {
        case LOWER_IR_TERM_RETURN:
            break;
        case LOWER_IR_TERM_JUMP:
            if (!reachable[block->terminator.as.jump_target]) {
                reachable[block->terminator.as.jump_target] = 1;
                stack[stack_size++] = block->terminator.as.jump_target;
            }
            break;
        case LOWER_IR_TERM_BRANCH:
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
            lower_ir_set_error(error,
                0,
                0,
                "LOWER-IR-042: bb.%zu uses unknown terminator kind %d during CFG analysis",
                block->id,
                (int)block->terminator.kind);
            return 0;
        }
    }

    free(stack);
    return 1;
}

static int lower_ir_analysis_compute_dominators(const LowerIrFunction *function,
    const unsigned char *reachable,
    const unsigned char *predecessor_matrix,
    unsigned char *dominates,
    LowerIrError *error) {
    size_t block_count;
    size_t block_index;
    int changed = 1;
    unsigned char *scratch = NULL;

    block_count = function->block_count;
    scratch = (unsigned char *)malloc(block_count * sizeof(unsigned char));
    if (!scratch) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-044: out of memory while computing dominators");
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

static void lower_ir_analysis_compute_immediate_dominators(const LowerIrFunction *function,
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

static void lower_ir_analysis_compute_dominator_tree(const LowerIrFunction *function,
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

static void lower_ir_analysis_compute_dominance_frontier(const LowerIrFunction *function,
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

int lower_ir_compute_cfg_analysis(const LowerIrFunction *function,
    LowerIrCfgAnalysis *analysis,
    LowerIrError *error) {
    size_t block_count;

    if (!function || !analysis) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-041: invalid CFG analysis contract");
        return 0;
    }
    if (!function->has_body) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-045: cannot compute CFG analysis for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    lower_ir_cfg_analysis_free(analysis);
    block_count = function->block_count;
    analysis->block_count = block_count;
    if (block_count == 0) {
        return 1;
    }

    analysis->predecessor_matrix = (unsigned char *)calloc(block_count * block_count, sizeof(unsigned char));
    analysis->predecessor_counts = (size_t *)calloc(block_count, sizeof(size_t));
    analysis->predecessors = (size_t *)calloc(block_count * block_count, sizeof(size_t));
    analysis->successor_counts = (size_t *)calloc(block_count, sizeof(size_t));
    analysis->successors = (size_t *)calloc(block_count * block_count, sizeof(size_t));
    analysis->reachable = (unsigned char *)calloc(block_count, sizeof(unsigned char));
    analysis->dominates = (unsigned char *)calloc(block_count * block_count, sizeof(unsigned char));
    analysis->immediate_dominator = (size_t *)malloc(block_count * sizeof(size_t));
    analysis->dominator_tree_children = (unsigned char *)calloc(block_count * block_count, sizeof(unsigned char));
    analysis->dominator_tree_child_counts = (size_t *)calloc(block_count, sizeof(size_t));
    analysis->dominance_frontier = (unsigned char *)calloc(block_count * block_count, sizeof(unsigned char));
    analysis->dominance_frontier_counts = (size_t *)calloc(block_count, sizeof(size_t));
    if (!analysis->predecessor_matrix || !analysis->predecessor_counts || !analysis->predecessors ||
        !analysis->successor_counts || !analysis->successors || !analysis->reachable ||
        !analysis->dominates || !analysis->immediate_dominator ||
        !analysis->dominator_tree_children || !analysis->dominator_tree_child_counts ||
        !analysis->dominance_frontier || !analysis->dominance_frontier_counts) {
        lower_ir_cfg_analysis_free(analysis);
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-046: out of memory while computing CFG analysis for function '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    if (!lower_ir_analysis_build_predecessor_matrix(function,
            analysis->predecessor_matrix,
            analysis->predecessor_counts,
            error) ||
        !lower_ir_analysis_build_successor_lists(function,
            analysis->successor_counts,
            analysis->successors,
            error) ||
        !lower_ir_analysis_mark_reachable_blocks(function, analysis->reachable, error)) {
        lower_ir_cfg_analysis_free(analysis);
        return 0;
    }

    lower_ir_analysis_build_predecessor_lists(function,
        analysis->predecessor_matrix,
        analysis->predecessor_counts,
        analysis->predecessors);

    if (!lower_ir_analysis_compute_dominators(function,
            analysis->reachable,
            analysis->predecessor_matrix,
            analysis->dominates,
            error)) {
        lower_ir_cfg_analysis_free(analysis);
        return 0;
    }

    lower_ir_analysis_compute_immediate_dominators(function,
        analysis->reachable,
        analysis->dominates,
        analysis->immediate_dominator);
    lower_ir_analysis_compute_dominator_tree(function,
        analysis->reachable,
        analysis->immediate_dominator,
        analysis->dominator_tree_children,
        analysis->dominator_tree_child_counts);
    lower_ir_analysis_compute_dominance_frontier(function,
        analysis->reachable,
        analysis->predecessor_matrix,
        analysis->dominates,
        analysis->dominance_frontier,
        analysis->dominance_frontier_counts);
    return 1;
}

int lower_ir_collect_temp_definition_blocks(const LowerIrFunction *function,
    size_t temp_id,
    unsigned char *out_definition_blocks,
    LowerIrError *error) {
    size_t block_index;

    if (!function || !out_definition_blocks) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-047: invalid temp-definition collection contract");
        return 0;
    }
    if (!function->has_body) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-048: cannot collect temp definition blocks for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }
    if (temp_id >= function->next_temp_id) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-049: temp.%zu is out of range for function '%s' next_temp_id=%zu",
            temp_id,
            function->name ? function->name : "<anon>",
            function->next_temp_id);
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const LowerIrBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        out_definition_blocks[block_index] = 0;
        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const LowerIrInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->has_result &&
                instruction->result.kind == LOWER_IR_VALUE_TEMP &&
                instruction->result.temp_id == temp_id) {
                out_definition_blocks[block_index] = 1;
                break;
            }
        }
    }

    return 1;
}

int lower_ir_compute_phi_placement(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    const unsigned char *definition_blocks,
    unsigned char *out_phi_blocks,
    LowerIrError *error) {
    unsigned char *in_worklist = NULL;
    size_t *worklist = NULL;
    size_t worklist_count = 0;
    size_t block_count;
    size_t block_index;

    if (!function || !analysis || !definition_blocks || !out_phi_blocks) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-050: invalid phi-placement contract");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-051: phi-placement analysis block_count=%zu does not match function block_count=%zu",
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
        lower_ir_set_error(error, 0, 0, "LOWER-IR-052: out of memory while computing phi placement");
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

int lower_ir_compute_temp_phi_candidates(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    unsigned char *out_phi_candidate_blocks,
    LowerIrError *error) {
    unsigned char *definition_blocks = NULL;
    unsigned char *phi_blocks = NULL;
    size_t block_count;
    size_t temp_count;
    size_t temp_id;

    if (!function || !analysis || !out_phi_candidate_blocks) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-060: invalid temp phi-candidate contract");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-061: temp phi-candidate analysis block_count=%zu does not match function block_count=%zu",
            analysis->block_count,
            function->block_count);
        return 0;
    }

    block_count = function->block_count;
    temp_count = function->next_temp_id;
    if (block_count == 0 || temp_count == 0) {
        return 1;
    }

    definition_blocks = (unsigned char *)calloc(block_count, sizeof(unsigned char));
    phi_blocks = (unsigned char *)calloc(block_count, sizeof(unsigned char));
    if (!definition_blocks || !phi_blocks) {
        free(definition_blocks);
        free(phi_blocks);
        lower_ir_set_error(error, 0, 0, "LOWER-IR-062: out of memory while computing temp phi candidates");
        return 0;
    }

    for (temp_id = 0; temp_id < temp_count; ++temp_id) {
        size_t block_index;

        if (!lower_ir_collect_temp_definition_blocks(function, temp_id, definition_blocks, error) ||
            !lower_ir_compute_phi_placement(function, analysis, definition_blocks, phi_blocks, error)) {
            free(definition_blocks);
            free(phi_blocks);
            return 0;
        }

        for (block_index = 0; block_index < block_count; ++block_index) {
            out_phi_candidate_blocks[block_index * temp_count + temp_id] = phi_blocks[block_index];
            phi_blocks[block_index] = 0;
        }
    }

    free(definition_blocks);
    free(phi_blocks);
    return 1;
}

int lower_ir_compute_block_phi_candidate_lists(const LowerIrFunction *function,
    const unsigned char *phi_candidate_blocks,
    size_t *out_phi_candidate_counts,
    size_t *out_phi_candidate_temps,
    LowerIrError *error) {
    size_t block_index;
    size_t temp_id;
    size_t temp_count;

    if (!function || !phi_candidate_blocks || !out_phi_candidate_counts || !out_phi_candidate_temps) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-063: invalid block phi-candidate list contract");
        return 0;
    }

    temp_count = function->next_temp_id;
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        size_t write_index = 0;

        out_phi_candidate_counts[block_index] = 0;
        for (temp_id = 0; temp_id < temp_count; ++temp_id) {
            if (!phi_candidate_blocks[block_index * temp_count + temp_id]) {
                continue;
            }
            out_phi_candidate_temps[block_index * temp_count + write_index++] = temp_id;
        }
        out_phi_candidate_counts[block_index] = write_index;
    }

    return 1;
}

int lower_ir_compute_temp_phi_candidate_lists(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    size_t *out_phi_candidate_counts,
    size_t *out_phi_candidate_temps,
    LowerIrError *error) {
    unsigned char *phi_candidate_blocks = NULL;
    size_t block_count;
    size_t temp_count;
    int ok = 0;

    if (!function || !analysis || !out_phi_candidate_counts || !out_phi_candidate_temps) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-064: invalid temp phi-candidate list contract");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-065: temp phi-candidate list analysis block_count=%zu does not match function block_count=%zu",
            analysis->block_count,
            function->block_count);
        return 0;
    }

    block_count = function->block_count;
    temp_count = function->next_temp_id;
    if (block_count == 0 || temp_count == 0) {
        return 1;
    }

    phi_candidate_blocks = (unsigned char *)calloc(block_count * temp_count, sizeof(unsigned char));
    if (!phi_candidate_blocks) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-066: out of memory while computing temp phi-candidate lists");
        return 0;
    }

    ok = lower_ir_compute_temp_phi_candidates(function, analysis, phi_candidate_blocks, error) &&
        lower_ir_compute_block_phi_candidate_lists(function,
            phi_candidate_blocks,
            out_phi_candidate_counts,
            out_phi_candidate_temps,
            error);

    free(phi_candidate_blocks);
    return ok;
}

static int lower_ir_value_uses_temp(LowerIrValueRef value, size_t temp_id) {
    return value.kind == LOWER_IR_VALUE_TEMP && value.temp_id == temp_id;
}

int lower_ir_collect_temp_live_in_blocks(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    size_t temp_id,
    unsigned char *out_live_in_blocks,
    LowerIrError *error) {
    unsigned char *use_before_def_blocks = NULL;
    unsigned char *def_blocks = NULL;
    unsigned char *live_out_blocks = NULL;
    size_t block_index;
    int changed = 1;

    if (!function || !analysis || !out_live_in_blocks) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-069: invalid temp live-in contract");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-070: temp live-in analysis block_count=%zu does not match function block_count=%zu",
            analysis->block_count,
            function->block_count);
        return 0;
    }
    if (temp_id >= function->next_temp_id) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-071: temp.%zu is out of range for live-in analysis of function '%s' next_temp_id=%zu",
            temp_id,
            function->name ? function->name : "<anon>",
            function->next_temp_id);
        return 0;
    }

    use_before_def_blocks = (unsigned char *)calloc(function->block_count, sizeof(unsigned char));
    def_blocks = (unsigned char *)calloc(function->block_count, sizeof(unsigned char));
    live_out_blocks = (unsigned char *)calloc(function->block_count, sizeof(unsigned char));
    if (!use_before_def_blocks || !def_blocks || !live_out_blocks) {
        free(use_before_def_blocks);
        free(def_blocks);
        free(live_out_blocks);
        lower_ir_set_error(error, 0, 0, "LOWER-IR-072: out of memory while computing temp live-in blocks");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const LowerIrBasicBlock *block = &function->blocks[block_index];
        int defined = 0;
        size_t instruction_index;

        out_live_in_blocks[block_index] = 0;
        if (!analysis->reachable[block_index]) {
            continue;
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const LowerIrInstruction *instruction = &block->instructions[instruction_index];

            switch (instruction->kind) {
            case LOWER_IR_INSTR_MOV:
                if (!defined && lower_ir_value_uses_temp(instruction->as.mov_value, temp_id)) {
                    use_before_def_blocks[block_index] = 1;
                }
                break;
            case LOWER_IR_INSTR_BINARY:
                if (!defined && (lower_ir_value_uses_temp(instruction->as.binary.lhs, temp_id) ||
                        lower_ir_value_uses_temp(instruction->as.binary.rhs, temp_id))) {
                    use_before_def_blocks[block_index] = 1;
                }
                break;
            case LOWER_IR_INSTR_CALL:
                if (!defined) {
                    size_t arg_index;
                    for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                        if (lower_ir_value_uses_temp(instruction->as.call.args[arg_index], temp_id)) {
                            use_before_def_blocks[block_index] = 1;
                            break;
                        }
                    }
                }
                break;
            case LOWER_IR_INSTR_STORE_LOCAL:
            case LOWER_IR_INSTR_STORE_GLOBAL:
                if (!defined && lower_ir_value_uses_temp(instruction->as.store.value, temp_id)) {
                    use_before_def_blocks[block_index] = 1;
                }
                break;
            case LOWER_IR_INSTR_LOAD_INDIRECT:
                if (!defined && lower_ir_value_uses_temp(instruction->as.load_indirect_addr, temp_id)) {
                    use_before_def_blocks[block_index] = 1;
                }
                break;
            case LOWER_IR_INSTR_STORE_INDIRECT:
                if (!defined &&
                    (lower_ir_value_uses_temp(instruction->as.store_indirect.addr, temp_id) ||
                        lower_ir_value_uses_temp(instruction->as.store_indirect.value, temp_id))) {
                    use_before_def_blocks[block_index] = 1;
                }
                break;
            case LOWER_IR_INSTR_LOAD_LOCAL:
            case LOWER_IR_INSTR_LOAD_GLOBAL:
                break;
            default:
                break;
            }

            if (instruction->has_result &&
                instruction->result.kind == LOWER_IR_VALUE_TEMP &&
                instruction->result.temp_id == temp_id) {
                def_blocks[block_index] = 1;
                defined = 1;
            }
        }

        switch (block->terminator.kind) {
        case LOWER_IR_TERM_RETURN:
            if (block->terminator.has_return_value &&
                !defined &&
                lower_ir_value_uses_temp(block->terminator.as.return_value, temp_id)) {
                use_before_def_blocks[block_index] = 1;
            }
            break;
        case LOWER_IR_TERM_BRANCH:
            if (!defined && lower_ir_value_uses_temp(block->terminator.as.branch.condition, temp_id)) {
                use_before_def_blocks[block_index] = 1;
            }
            break;
        case LOWER_IR_TERM_JUMP:
            break;
        default:
            break;
        }
    }

    while (changed) {
        changed = 0;
        for (block_index = function->block_count; block_index-- > 0;) {
            size_t successor_index;
            unsigned char live_out = 0;
            unsigned char live_in;

            if (!analysis->reachable[block_index]) {
                continue;
            }

            for (successor_index = 0; successor_index < analysis->successor_counts[block_index]; ++successor_index) {
                size_t successor_block = analysis->successors[block_index * function->block_count + successor_index];
                if (out_live_in_blocks[successor_block]) {
                    live_out = 1;
                    break;
                }
            }

            live_in = (unsigned char)(use_before_def_blocks[block_index] ||
                (!def_blocks[block_index] && live_out));
            if (live_out_blocks[block_index] != live_out) {
                live_out_blocks[block_index] = live_out;
                changed = 1;
            }
            if (out_live_in_blocks[block_index] != live_in) {
                out_live_in_blocks[block_index] = live_in;
                changed = 1;
            }
        }
    }

    free(use_before_def_blocks);
    free(def_blocks);
    free(live_out_blocks);
    return 1;
}

int lower_ir_compute_pruned_temp_phi_candidate_lists(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    size_t *out_phi_candidate_counts,
    size_t *out_phi_candidate_temps,
    LowerIrError *error) {
    unsigned char *phi_candidate_blocks = NULL;
    unsigned char *live_in_blocks = NULL;
    size_t block_count;
    size_t temp_count;
    size_t temp_id;

    if (!function || !analysis || !out_phi_candidate_counts || !out_phi_candidate_temps) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-073: invalid pruned temp phi-candidate list contract");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-074: pruned temp phi-candidate analysis block_count=%zu does not match function block_count=%zu",
            analysis->block_count,
            function->block_count);
        return 0;
    }

    block_count = function->block_count;
    temp_count = function->next_temp_id;
    if (block_count == 0 || temp_count == 0) {
        return 1;
    }

    phi_candidate_blocks = (unsigned char *)calloc(block_count * temp_count, sizeof(unsigned char));
    live_in_blocks = (unsigned char *)calloc(block_count, sizeof(unsigned char));
    if (!phi_candidate_blocks || !live_in_blocks) {
        free(phi_candidate_blocks);
        free(live_in_blocks);
        lower_ir_set_error(error, 0, 0, "LOWER-IR-075: out of memory while computing pruned temp phi-candidate lists");
        return 0;
    }

    if (!lower_ir_compute_temp_phi_candidates(function, analysis, phi_candidate_blocks, error)) {
        free(phi_candidate_blocks);
        free(live_in_blocks);
        return 0;
    }

    for (temp_id = 0; temp_id < temp_count; ++temp_id) {
        size_t block_index;

        memset(live_in_blocks, 0, block_count * sizeof(unsigned char));
        if (!lower_ir_collect_temp_live_in_blocks(function, analysis, temp_id, live_in_blocks, error)) {
            free(phi_candidate_blocks);
            free(live_in_blocks);
            return 0;
        }

        for (block_index = 0; block_index < block_count; ++block_index) {
            phi_candidate_blocks[block_index * temp_count + temp_id] =
                (unsigned char)(phi_candidate_blocks[block_index * temp_count + temp_id] && live_in_blocks[block_index]);
        }
    }

    if (!lower_ir_compute_block_phi_candidate_lists(function,
            phi_candidate_blocks,
            out_phi_candidate_counts,
            out_phi_candidate_temps,
            error)) {
        free(phi_candidate_blocks);
        free(live_in_blocks);
        return 0;
    }

    free(phi_candidate_blocks);
    free(live_in_blocks);
    return 1;
}

int lower_ir_compute_block_successor_phi_use_lists(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    const size_t *phi_candidate_counts,
    const size_t *phi_candidate_temps,
    size_t *out_successor_phi_use_counts,
    size_t *out_successor_phi_use_temps,
    LowerIrError *error) {
    size_t block_index;
    size_t temp_count;

    if (!function || !analysis || !phi_candidate_counts || !phi_candidate_temps ||
        !out_successor_phi_use_counts || !out_successor_phi_use_temps) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-067: invalid successor phi-use list contract");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-068: successor phi-use list analysis block_count=%zu does not match function block_count=%zu",
            analysis->block_count,
            function->block_count);
        return 0;
    }

    temp_count = function->next_temp_id;
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        size_t write_index = 0;
        size_t successor_index;

        out_successor_phi_use_counts[block_index] = 0;
        for (successor_index = 0; successor_index < analysis->successor_counts[block_index]; ++successor_index) {
            size_t successor_block = analysis->successors[block_index * function->block_count + successor_index];
            size_t candidate_index;

            for (candidate_index = 0; candidate_index < phi_candidate_counts[successor_block]; ++candidate_index) {
                size_t temp_id = phi_candidate_temps[successor_block * temp_count + candidate_index];
                size_t existing_index;
                int seen = 0;

                for (existing_index = 0; existing_index < write_index; ++existing_index) {
                    if (out_successor_phi_use_temps[block_index * temp_count + existing_index] == temp_id) {
                        seen = 1;
                        break;
                    }
                }
                if (seen) {
                    continue;
                }
                out_successor_phi_use_temps[block_index * temp_count + write_index++] = temp_id;
            }
        }
        out_successor_phi_use_counts[block_index] = write_index;
    }

    return 1;
}

int lower_ir_compute_dominator_tree_preorder(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    size_t *out_order,
    size_t *out_count,
    LowerIrError *error) {
    size_t *stack = NULL;
    size_t stack_size = 0;
    size_t write_index = 0;
    size_t block_count;

    if (!function || !analysis || !out_order || !out_count) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-053: invalid dominator-tree preorder contract");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-054: dominator-tree preorder analysis block_count=%zu does not match function block_count=%zu",
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
        lower_ir_set_error(error, 0, 0, "LOWER-IR-055: dominator-tree preorder requires reachable entry block");
        return 0;
    }

    stack = (size_t *)malloc(block_count * sizeof(size_t));
    if (!stack) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-056: out of memory while computing dominator-tree preorder");
        return 0;
    }

    stack[stack_size++] = 0;
    while (stack_size > 0) {
        size_t block_id = stack[--stack_size];
        size_t child_index;

        out_order[write_index++] = block_id;
        for (child_index = block_count; child_index-- > 0;) {
            if (!analysis->dominator_tree_children[block_id * block_count + child_index]) {
                continue;
            }
            stack[stack_size++] = child_index;
        }
    }

    free(stack);
    *out_count = write_index;
    return 1;
}

static int lower_ir_walk_dominator_tree_from(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    size_t block_id,
    LowerIrDominatorTreeWalkFn enter_block,
    LowerIrDominatorTreeWalkFn leave_block,
    void *user_data,
    LowerIrError *error) {
    size_t child_index;
    size_t block_count;

    if (enter_block && !enter_block(function, analysis, block_id, user_data, error)) {
        return 0;
    }

    block_count = function->block_count;
    for (child_index = 0; child_index < block_count; ++child_index) {
        if (!analysis->dominator_tree_children[block_id * block_count + child_index]) {
            continue;
        }

        if (!lower_ir_walk_dominator_tree_from(function,
                analysis,
                child_index,
                enter_block,
                leave_block,
                user_data,
                error)) {
            return 0;
        }
    }

    if (leave_block && !leave_block(function, analysis, block_id, user_data, error)) {
        return 0;
    }

    return 1;
}

int lower_ir_walk_dominator_tree(const LowerIrFunction *function,
    const LowerIrCfgAnalysis *analysis,
    LowerIrDominatorTreeWalkFn enter_block,
    LowerIrDominatorTreeWalkFn leave_block,
    void *user_data,
    LowerIrError *error) {
    if (!function || !analysis) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-057: invalid dominator-tree walk contract");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        lower_ir_set_error(error,
            0,
            0,
            "LOWER-IR-058: dominator-tree walk analysis block_count=%zu does not match function block_count=%zu",
            analysis->block_count,
            function->block_count);
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }
    if (!analysis->reachable[0]) {
        lower_ir_set_error(error, 0, 0, "LOWER-IR-059: dominator-tree walk requires reachable entry block");
        return 0;
    }

    return lower_ir_walk_dominator_tree_from(function,
        analysis,
        0,
        enter_block,
        leave_block,
        user_data,
        error);
}

#endif
