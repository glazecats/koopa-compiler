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

void value_ssa_def_use_analysis_init(ValueSsaDefUseAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    analysis->value_count = 0;
    analysis->def_block_ids = NULL;
    analysis->def_phi_indices = NULL;
    analysis->def_instruction_indices = NULL;
    analysis->has_def = NULL;
    analysis->use_counts = NULL;
    analysis->use_offsets = NULL;
    analysis->use_sites = NULL;
}

void value_ssa_liveness_analysis_init(ValueSsaLivenessAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    analysis->block_count = 0;
    analysis->value_count = 0;
    analysis->live_in = NULL;
    analysis->live_out = NULL;
}

void value_ssa_interference_graph_init(ValueSsaInterferenceGraph *graph) {
    if (!graph) {
        return;
    }

    graph->value_count = 0;
    graph->interferes = NULL;
}

void value_ssa_copy_affinity_graph_init(ValueSsaCopyAffinityGraph *graph) {
    if (!graph) {
        return;
    }

    graph->value_count = 0;
    graph->weights = NULL;
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

void value_ssa_def_use_analysis_free(ValueSsaDefUseAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    free(analysis->def_block_ids);
    free(analysis->def_phi_indices);
    free(analysis->def_instruction_indices);
    free(analysis->has_def);
    free(analysis->use_counts);
    free(analysis->use_offsets);
    free(analysis->use_sites);
    value_ssa_def_use_analysis_init(analysis);
}

void value_ssa_liveness_analysis_free(ValueSsaLivenessAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    free(analysis->live_in);
    free(analysis->live_out);
    value_ssa_liveness_analysis_init(analysis);
}

void value_ssa_interference_graph_free(ValueSsaInterferenceGraph *graph) {
    if (!graph) {
        return;
    }

    free(graph->interferes);
    value_ssa_interference_graph_init(graph);
}

void value_ssa_copy_affinity_graph_free(ValueSsaCopyAffinityGraph *graph) {
    if (!graph) {
        return;
    }

    free(graph->weights);
    value_ssa_copy_affinity_graph_init(graph);
}

static void value_ssa_record_use_count(ValueSsaValueRef value, size_t *use_counts) {
    if (!use_counts || value.kind != VALUE_SSA_VALUE_ID) {
        return;
    }

    ++use_counts[value.value_id];
}

static int value_ssa_write_use_site(ValueSsaValueRef value,
    ValueSsaUseKind kind,
    ValueSsaUseRole role,
    size_t block_id,
    size_t phi_index,
    size_t instruction_index,
    size_t operand_index,
    size_t *cursor,
    size_t *use_offsets,
    ValueSsaUseSite *use_sites) {
    size_t slot;

    if (value.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }
    if (!cursor || !use_offsets || !use_sites) {
        return 0;
    }

    slot = use_offsets[value.value_id] + cursor[value.value_id];
    use_sites[slot].kind = kind;
    use_sites[slot].role = role;
    use_sites[slot].block_id = block_id;
    use_sites[slot].phi_index = phi_index;
    use_sites[slot].instruction_index = instruction_index;
    use_sites[slot].operand_index = operand_index;
    ++cursor[value.value_id];
    return 1;
}

static void value_ssa_liveness_record_edge_use(ValueSsaValueRef value,
    size_t predecessor_block_id,
    size_t value_count,
    unsigned char *edge_uses) {
    if (!edge_uses || value.kind != VALUE_SSA_VALUE_ID || value.value_id >= value_count) {
        return;
    }

    edge_uses[(predecessor_block_id * value_count) + value.value_id] = 1;
}

static void value_ssa_liveness_record_use_if_not_defined(ValueSsaValueRef value,
    const unsigned char *block_defs,
    size_t value_count,
    unsigned char *block_uses) {
    if (!block_defs || !block_uses || value.kind != VALUE_SSA_VALUE_ID || value.value_id >= value_count) {
        return;
    }

    if (!block_defs[value.value_id]) {
        block_uses[value.value_id] = 1;
    }
}

static void value_ssa_liveness_record_definition(ValueSsaValueRef result,
    size_t value_count,
    unsigned char *block_defs) {
    if (!block_defs || result.kind != VALUE_SSA_VALUE_ID || result.value_id >= value_count) {
        return;
    }

    block_defs[result.value_id] = 1;
}

static void value_ssa_liveness_record_instruction_uses(const ValueSsaInstruction *instruction,
    const unsigned char *block_defs,
    size_t value_count,
    unsigned char *block_uses) {
    size_t arg_index;

    if (!instruction || !block_defs || !block_uses) {
        return;
    }

    switch (instruction->kind) {
    case VALUE_SSA_INSTR_MOV:
        value_ssa_liveness_record_use_if_not_defined(
            instruction->as.mov_value, block_defs, value_count, block_uses);
        break;
    case VALUE_SSA_INSTR_BINARY:
        value_ssa_liveness_record_use_if_not_defined(
            instruction->as.binary.lhs, block_defs, value_count, block_uses);
        value_ssa_liveness_record_use_if_not_defined(
            instruction->as.binary.rhs, block_defs, value_count, block_uses);
        break;
    case VALUE_SSA_INSTR_CALL:
        for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
            value_ssa_liveness_record_use_if_not_defined(
                instruction->as.call.args[arg_index], block_defs, value_count, block_uses);
        }
        break;
    case VALUE_SSA_INSTR_LOAD_INDIRECT:
        value_ssa_liveness_record_use_if_not_defined(
            instruction->as.load_indirect_addr, block_defs, value_count, block_uses);
        break;
    case VALUE_SSA_INSTR_STORE_LOCAL:
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        value_ssa_liveness_record_use_if_not_defined(
            instruction->as.store.value, block_defs, value_count, block_uses);
        break;
    case VALUE_SSA_INSTR_STORE_INDIRECT:
        value_ssa_liveness_record_use_if_not_defined(
            instruction->as.store_indirect.addr, block_defs, value_count, block_uses);
        value_ssa_liveness_record_use_if_not_defined(
            instruction->as.store_indirect.value, block_defs, value_count, block_uses);
        break;
    case VALUE_SSA_INSTR_ADDR_FUNCTION:
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
    case VALUE_SSA_INSTR_LOAD_LOCAL:
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        break;
    default:
        break;
    }
}

static void value_ssa_liveness_record_terminator_uses(const ValueSsaTerminator *terminator,
    const unsigned char *block_defs,
    size_t value_count,
    unsigned char *block_uses) {
    if (!terminator || !block_defs || !block_uses) {
        return;
    }

    switch (terminator->kind) {
    case VALUE_SSA_TERM_RETURN:
        if (terminator->has_return_value) {
            value_ssa_liveness_record_use_if_not_defined(
                terminator->as.return_value, block_defs, value_count, block_uses);
        }
        break;
    case VALUE_SSA_TERM_BRANCH:
        value_ssa_liveness_record_use_if_not_defined(
            terminator->as.branch.condition, block_defs, value_count, block_uses);
        break;
    case VALUE_SSA_TERM_JUMP:
        break;
    default:
        break;
    }
}

static int value_ssa_analysis_collect_block_liveness_seeds(const ValueSsaFunction *function,
    size_t value_count,
    unsigned char *block_defs,
    unsigned char *block_uses,
    unsigned char *edge_uses,
    ValueSsaError *error) {
    size_t block_index;

    if (!function || !block_defs || !block_uses || !edge_uses) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-198: invalid liveness seed contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        unsigned char *defs = &block_defs[block_index * value_count];
        unsigned char *uses = &block_uses[block_index * value_count];
        size_t phi_index;
        size_t instruction_index;

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            const ValueSsaPhi *phi = &block->phis[phi_index];
            size_t input_index;

            value_ssa_liveness_record_definition(value_ssa_value_id(phi->result_id), value_count, defs);
            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                size_t predecessor_block_id = phi->inputs[input_index].predecessor_block_id;

                if (predecessor_block_id >= function->block_count) {
                    value_ssa_set_error(error,
                        0,
                        0,
                        "VALUE-SSA-199: phi in bb.%zu references invalid predecessor bb.%zu during liveness",
                        block->id,
                        predecessor_block_id);
                    return 0;
                }

                value_ssa_liveness_record_edge_use(phi->inputs[input_index].value,
                    predecessor_block_id,
                    value_count,
                    edge_uses);
            }
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            value_ssa_liveness_record_instruction_uses(instruction, defs, value_count, uses);
            if (instruction->has_result) {
                value_ssa_liveness_record_definition(instruction->result, value_count, defs);
            }
        }

        value_ssa_liveness_record_terminator_uses(&block->terminator, defs, value_count, uses);
    }

    return 1;
}

static void value_ssa_interference_add_edge(ValueSsaInterferenceGraph *graph, size_t lhs, size_t rhs) {
    if (!graph || !graph->interferes || lhs >= graph->value_count || rhs >= graph->value_count || lhs == rhs) {
        return;
    }

    graph->interferes[lhs * graph->value_count + rhs] = 1;
    graph->interferes[rhs * graph->value_count + lhs] = 1;
}

static void value_ssa_interference_add_live_clique(ValueSsaInterferenceGraph *graph, const unsigned char *live) {
    size_t lhs;

    if (!graph || !graph->interferes || !live) {
        return;
    }

    for (lhs = 0; lhs < graph->value_count; ++lhs) {
        size_t rhs;

        if (!live[lhs]) {
            continue;
        }

        for (rhs = lhs + 1; rhs < graph->value_count; ++rhs) {
            if (live[rhs]) {
                value_ssa_interference_add_edge(graph, lhs, rhs);
            }
        }
    }
}

static void value_ssa_interference_add_value_use(ValueSsaValueRef value,
    size_t value_count,
    unsigned char *live) {
    if (!live || value.kind != VALUE_SSA_VALUE_ID || value.value_id >= value_count) {
        return;
    }

    live[value.value_id] = 1;
}

static void value_ssa_interference_add_instruction_uses(const ValueSsaInstruction *instruction,
    size_t value_count,
    unsigned char *live) {
    size_t arg_index;

    if (!instruction || !live) {
        return;
    }

    switch (instruction->kind) {
    case VALUE_SSA_INSTR_MOV:
        value_ssa_interference_add_value_use(instruction->as.mov_value, value_count, live);
        break;
    case VALUE_SSA_INSTR_BINARY:
        value_ssa_interference_add_value_use(instruction->as.binary.lhs, value_count, live);
        value_ssa_interference_add_value_use(instruction->as.binary.rhs, value_count, live);
        break;
    case VALUE_SSA_INSTR_CALL:
        for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
            value_ssa_interference_add_value_use(instruction->as.call.args[arg_index], value_count, live);
        }
        break;
    case VALUE_SSA_INSTR_LOAD_INDIRECT:
        value_ssa_interference_add_value_use(instruction->as.load_indirect_addr, value_count, live);
        break;
    case VALUE_SSA_INSTR_STORE_LOCAL:
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        value_ssa_interference_add_value_use(instruction->as.store.value, value_count, live);
        break;
    case VALUE_SSA_INSTR_STORE_INDIRECT:
        value_ssa_interference_add_value_use(instruction->as.store_indirect.addr, value_count, live);
        value_ssa_interference_add_value_use(instruction->as.store_indirect.value, value_count, live);
        break;
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
    case VALUE_SSA_INSTR_LOAD_LOCAL:
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        break;
    default:
        break;
    }
}

static void value_ssa_interference_add_terminator_uses(const ValueSsaTerminator *terminator,
    size_t value_count,
    unsigned char *live) {
    if (!terminator || !live) {
        return;
    }

    switch (terminator->kind) {
    case VALUE_SSA_TERM_RETURN:
        if (terminator->has_return_value) {
            value_ssa_interference_add_value_use(terminator->as.return_value, value_count, live);
        }
        break;
    case VALUE_SSA_TERM_BRANCH:
        value_ssa_interference_add_value_use(terminator->as.branch.condition, value_count, live);
        break;
    case VALUE_SSA_TERM_JUMP:
        break;
    default:
        break;
    }
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

int value_ssa_compute_def_use_analysis(const ValueSsaFunction *function,
    ValueSsaDefUseAnalysis *analysis,
    ValueSsaError *error) {
    size_t block_index;
    size_t total_use_count = 0;
    size_t *cursor = NULL;

    if (!function || !analysis) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-189: invalid def-use analysis contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-190: cannot compute def-use analysis for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    value_ssa_def_use_analysis_free(analysis);
    analysis->value_count = function->next_value_id;
    if (function->next_value_id == 0) {
        return 1;
    }

    analysis->def_block_ids = (size_t *)malloc(function->next_value_id * sizeof(size_t));
    analysis->def_phi_indices = (size_t *)malloc(function->next_value_id * sizeof(size_t));
    analysis->def_instruction_indices = (size_t *)malloc(function->next_value_id * sizeof(size_t));
    analysis->has_def = (unsigned char *)calloc(function->next_value_id, sizeof(unsigned char));
    analysis->use_counts = (size_t *)calloc(function->next_value_id, sizeof(size_t));
    analysis->use_offsets = (size_t *)calloc(function->next_value_id, sizeof(size_t));
    if (!analysis->def_block_ids || !analysis->def_phi_indices || !analysis->def_instruction_indices ||
        !analysis->has_def || !analysis->use_counts || !analysis->use_offsets) {
        value_ssa_def_use_analysis_free(analysis);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-191: out of memory while preparing def-use analysis");
        return 0;
    }
    for (block_index = 0; block_index < function->next_value_id; ++block_index) {
        analysis->def_block_ids[block_index] = (size_t)-1;
        analysis->def_phi_indices[block_index] = (size_t)-1;
        analysis->def_instruction_indices[block_index] = (size_t)-1;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t phi_index;
        size_t instruction_index;

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            const ValueSsaPhi *phi = &block->phis[phi_index];
            size_t input_index;

            analysis->def_block_ids[phi->result_id] = block_index;
            analysis->def_phi_indices[phi->result_id] = phi_index;
            analysis->def_instruction_indices[phi->result_id] = (size_t)-1;
            analysis->has_def[phi->result_id] = 1;
            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                value_ssa_record_use_count(phi->inputs[input_index].value, analysis->use_counts);
            }
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
            size_t arg_index;

            if (instruction->has_result && instruction->result.kind == VALUE_SSA_VALUE_ID) {
                analysis->def_block_ids[instruction->result.value_id] = block_index;
                analysis->def_phi_indices[instruction->result.value_id] = (size_t)-1;
                analysis->def_instruction_indices[instruction->result.value_id] = instruction_index;
                analysis->has_def[instruction->result.value_id] = 1;
            }

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_MOV:
                value_ssa_record_use_count(instruction->as.mov_value, analysis->use_counts);
                break;
            case VALUE_SSA_INSTR_BINARY:
                value_ssa_record_use_count(instruction->as.binary.lhs, analysis->use_counts);
                value_ssa_record_use_count(instruction->as.binary.rhs, analysis->use_counts);
                break;
            case VALUE_SSA_INSTR_CALL:
                for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    value_ssa_record_use_count(instruction->as.call.args[arg_index], analysis->use_counts);
                }
                break;
            case VALUE_SSA_INSTR_LOAD_INDIRECT:
                value_ssa_record_use_count(instruction->as.load_indirect_addr, analysis->use_counts);
                break;
            case VALUE_SSA_INSTR_STORE_LOCAL:
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                value_ssa_record_use_count(instruction->as.store.value, analysis->use_counts);
                break;
            case VALUE_SSA_INSTR_STORE_INDIRECT:
                value_ssa_record_use_count(instruction->as.store_indirect.addr, analysis->use_counts);
                value_ssa_record_use_count(instruction->as.store_indirect.value, analysis->use_counts);
                break;
            case VALUE_SSA_INSTR_ADDR_FUNCTION:
            case VALUE_SSA_INSTR_ADDR_LOCAL:
            case VALUE_SSA_INSTR_ADDR_GLOBAL:
            case VALUE_SSA_INSTR_LOAD_LOCAL:
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                break;
            default:
                break;
            }
        }

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            if (block->terminator.has_return_value) {
                value_ssa_record_use_count(block->terminator.as.return_value, analysis->use_counts);
            }
            break;
        case VALUE_SSA_TERM_BRANCH:
            value_ssa_record_use_count(block->terminator.as.branch.condition, analysis->use_counts);
            break;
        case VALUE_SSA_TERM_JUMP:
            break;
        default:
            break;
        }
    }

    for (block_index = 0; block_index < function->next_value_id; ++block_index) {
        analysis->use_offsets[block_index] = total_use_count;
        total_use_count += analysis->use_counts[block_index];
    }

    if (total_use_count == 0) {
        return 1;
    }

    analysis->use_sites = (ValueSsaUseSite *)calloc(total_use_count, sizeof(ValueSsaUseSite));
    cursor = (size_t *)calloc(function->next_value_id, sizeof(size_t));
    if (!analysis->use_sites || !cursor) {
        free(cursor);
        value_ssa_def_use_analysis_free(analysis);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-192: out of memory while materializing def-use use-sites");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t phi_index;
        size_t instruction_index;

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            const ValueSsaPhi *phi = &block->phis[phi_index];
            size_t input_index;

            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                if (!value_ssa_write_use_site(phi->inputs[input_index].value,
                        VALUE_SSA_USE_PHI,
                        VALUE_SSA_USE_ROLE_PHI_INPUT,
                        block_index,
                        phi_index,
                        (size_t)-1,
                        input_index,
                        cursor,
                        analysis->use_offsets,
                        analysis->use_sites)) {
                    free(cursor);
                    value_ssa_def_use_analysis_free(analysis);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-193: failed to record phi use-site");
                    return 0;
                }
            }
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
            size_t arg_index;

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_MOV:
                if (!value_ssa_write_use_site(instruction->as.mov_value,
                        VALUE_SSA_USE_INSTR,
                        VALUE_SSA_USE_ROLE_MOV_VALUE,
                        block_index,
                        (size_t)-1,
                        instruction_index,
                        0,
                        cursor,
                        analysis->use_offsets,
                        analysis->use_sites)) {
                    free(cursor);
                    value_ssa_def_use_analysis_free(analysis);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-194: failed to record mov use-site");
                    return 0;
                }
                break;
            case VALUE_SSA_INSTR_BINARY:
                if (!value_ssa_write_use_site(instruction->as.binary.lhs,
                        VALUE_SSA_USE_INSTR,
                        VALUE_SSA_USE_ROLE_BINARY_LHS,
                        block_index,
                        (size_t)-1,
                        instruction_index,
                        0,
                        cursor,
                        analysis->use_offsets,
                        analysis->use_sites) ||
                    !value_ssa_write_use_site(instruction->as.binary.rhs,
                        VALUE_SSA_USE_INSTR,
                        VALUE_SSA_USE_ROLE_BINARY_RHS,
                        block_index,
                        (size_t)-1,
                        instruction_index,
                        1,
                        cursor,
                        analysis->use_offsets,
                        analysis->use_sites)) {
                    free(cursor);
                    value_ssa_def_use_analysis_free(analysis);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-195: failed to record binary use-site");
                    return 0;
                }
                break;
            case VALUE_SSA_INSTR_CALL:
                for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    if (!value_ssa_write_use_site(instruction->as.call.args[arg_index],
                            VALUE_SSA_USE_INSTR,
                            VALUE_SSA_USE_ROLE_CALL_ARG,
                            block_index,
                            (size_t)-1,
                            instruction_index,
                            arg_index,
                            cursor,
                            analysis->use_offsets,
                            analysis->use_sites)) {
                        free(cursor);
                        value_ssa_def_use_analysis_free(analysis);
                        value_ssa_set_error(error, 0, 0, "VALUE-SSA-196: failed to record call use-site");
                        return 0;
                    }
                }
                break;
            case VALUE_SSA_INSTR_LOAD_INDIRECT:
                if (!value_ssa_write_use_site(instruction->as.load_indirect_addr,
                        VALUE_SSA_USE_INSTR,
                        VALUE_SSA_USE_ROLE_LOAD_INDIRECT_ADDR,
                        block_index,
                        (size_t)-1,
                        instruction_index,
                        0,
                        cursor,
                        analysis->use_offsets,
                        analysis->use_sites)) {
                    free(cursor);
                    value_ssa_def_use_analysis_free(analysis);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-197: failed to record load_indirect use-site");
                    return 0;
                }
                break;
            case VALUE_SSA_INSTR_STORE_LOCAL:
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                if (!value_ssa_write_use_site(instruction->as.store.value,
                        VALUE_SSA_USE_INSTR,
                        VALUE_SSA_USE_ROLE_STORE_VALUE,
                        block_index,
                        (size_t)-1,
                        instruction_index,
                        0,
                        cursor,
                        analysis->use_offsets,
                        analysis->use_sites)) {
                    free(cursor);
                    value_ssa_def_use_analysis_free(analysis);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-197: failed to record store use-site");
                    return 0;
                }
                break;
            case VALUE_SSA_INSTR_STORE_INDIRECT:
                if (!value_ssa_write_use_site(instruction->as.store_indirect.addr,
                        VALUE_SSA_USE_INSTR,
                        VALUE_SSA_USE_ROLE_STORE_INDIRECT_ADDR,
                        block_index,
                        (size_t)-1,
                        instruction_index,
                        0,
                        cursor,
                        analysis->use_offsets,
                        analysis->use_sites) ||
                    !value_ssa_write_use_site(instruction->as.store_indirect.value,
                        VALUE_SSA_USE_INSTR,
                        VALUE_SSA_USE_ROLE_STORE_INDIRECT_VALUE,
                        block_index,
                        (size_t)-1,
                        instruction_index,
                        1,
                        cursor,
                        analysis->use_offsets,
                        analysis->use_sites)) {
                    free(cursor);
                    value_ssa_def_use_analysis_free(analysis);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-197: failed to record store_indirect use-site");
                    return 0;
                }
                break;
            case VALUE_SSA_INSTR_ADDR_FUNCTION:
            case VALUE_SSA_INSTR_ADDR_LOCAL:
            case VALUE_SSA_INSTR_ADDR_GLOBAL:
            case VALUE_SSA_INSTR_LOAD_LOCAL:
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                break;
            default:
                break;
            }
        }

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            if (block->terminator.has_return_value) {
                if (!value_ssa_write_use_site(block->terminator.as.return_value,
                        VALUE_SSA_USE_TERM,
                        VALUE_SSA_USE_ROLE_RETURN_VALUE,
                        block_index,
                        (size_t)-1,
                        (size_t)-1,
                        0,
                        cursor,
                        analysis->use_offsets,
                        analysis->use_sites)) {
                    free(cursor);
                    value_ssa_def_use_analysis_free(analysis);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-198: failed to record return use-site");
                    return 0;
                }
            }
            break;
        case VALUE_SSA_TERM_BRANCH:
            if (!value_ssa_write_use_site(block->terminator.as.branch.condition,
                    VALUE_SSA_USE_TERM,
                    VALUE_SSA_USE_ROLE_BRANCH_CONDITION,
                    block_index,
                    (size_t)-1,
                    (size_t)-1,
                    0,
                    cursor,
                    analysis->use_offsets,
                    analysis->use_sites)) {
                free(cursor);
                value_ssa_def_use_analysis_free(analysis);
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-199: failed to record branch use-site");
                return 0;
            }
            break;
        case VALUE_SSA_TERM_JUMP:
            break;
        default:
            break;
        }
    }

    free(cursor);
    return 1;
}

int value_ssa_compute_liveness_analysis(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg_analysis,
    ValueSsaLivenessAnalysis *analysis,
    ValueSsaError *error) {
    ValueSsaCfgAnalysis owned_cfg;
    const ValueSsaCfgAnalysis *active_cfg = cfg_analysis;
    unsigned char *block_defs = NULL;
    unsigned char *block_uses = NULL;
    unsigned char *edge_uses = NULL;
    size_t block_count;
    size_t value_count;
    int changed = 1;
    int ok = 0;

    if (!function || !analysis) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-200: invalid liveness analysis contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-201: cannot compute liveness analysis for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    value_ssa_liveness_analysis_free(analysis);
    analysis->block_count = function->block_count;
    analysis->value_count = function->next_value_id;
    if (analysis->block_count == 0 || analysis->value_count == 0) {
        return 1;
    }

    value_ssa_cfg_analysis_init(&owned_cfg);
    if (!active_cfg) {
        if (!value_ssa_compute_cfg_analysis(function, &owned_cfg, error)) {
            return 0;
        }
        active_cfg = &owned_cfg;
    } else if (active_cfg->block_count != function->block_count) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-202: liveness CFG block_count=%zu does not match function block_count=%zu",
            active_cfg->block_count,
            function->block_count);
        goto cleanup;
    }

    block_count = function->block_count;
    value_count = function->next_value_id;
    analysis->live_in = (unsigned char *)calloc(block_count * value_count, sizeof(unsigned char));
    analysis->live_out = (unsigned char *)calloc(block_count * value_count, sizeof(unsigned char));
    block_defs = (unsigned char *)calloc(block_count * value_count, sizeof(unsigned char));
    block_uses = (unsigned char *)calloc(block_count * value_count, sizeof(unsigned char));
    edge_uses = (unsigned char *)calloc(block_count * value_count, sizeof(unsigned char));
    if (!analysis->live_in || !analysis->live_out || !block_defs || !block_uses || !edge_uses) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-203: out of memory while preparing liveness analysis");
        goto cleanup;
    }

    if (!value_ssa_analysis_collect_block_liveness_seeds(
            function, value_count, block_defs, block_uses, edge_uses, error)) {
        goto cleanup;
    }

    while (changed) {
        size_t reverse_index = block_count;

        changed = 0;
        while (reverse_index > 0) {
            size_t block_index = --reverse_index;
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            unsigned char *live_in_row = &analysis->live_in[block_index * value_count];
            unsigned char *live_out_row = &analysis->live_out[block_index * value_count];
            const unsigned char *defs_row = &block_defs[block_index * value_count];
            const unsigned char *uses_row = &block_uses[block_index * value_count];
            const unsigned char *edge_row = &edge_uses[block_index * value_count];
            size_t succ_a = (size_t)-1;
            size_t succ_b = (size_t)-1;
            size_t value_index;

            switch (block->terminator.kind) {
            case VALUE_SSA_TERM_RETURN:
                break;
            case VALUE_SSA_TERM_JUMP:
                succ_a = block->terminator.as.jump_target;
                break;
            case VALUE_SSA_TERM_BRANCH:
                succ_a = block->terminator.as.branch.then_target;
                succ_b = block->terminator.as.branch.else_target;
                break;
            default:
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-204: bb.%zu uses unknown terminator kind %d during liveness",
                    block->id,
                    (int)block->terminator.kind);
                goto cleanup;
            }

            for (value_index = 0; value_index < value_count; ++value_index) {
                unsigned char new_live_out = edge_row[value_index];
                unsigned char new_live_in;

                if (succ_a != (size_t)-1) {
                    new_live_out = (unsigned char)(new_live_out ||
                        analysis->live_in[(succ_a * value_count) + value_index]);
                }
                if (succ_b != (size_t)-1) {
                    new_live_out = (unsigned char)(new_live_out ||
                        analysis->live_in[(succ_b * value_count) + value_index]);
                }

                new_live_in = (unsigned char)(uses_row[value_index] ||
                    (new_live_out && !defs_row[value_index]));

                if (live_out_row[value_index] != new_live_out) {
                    live_out_row[value_index] = new_live_out;
                    changed = 1;
                }
                if (live_in_row[value_index] != new_live_in) {
                    live_in_row[value_index] = new_live_in;
                    changed = 1;
                }
            }
        }
    }

    ok = 1;

cleanup:
    free(block_defs);
    free(block_uses);
    free(edge_uses);
    value_ssa_cfg_analysis_free(&owned_cfg);
    if (!ok) {
        value_ssa_liveness_analysis_free(analysis);
    }
    return ok;
}

int value_ssa_compute_interference_graph(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg_analysis,
    const ValueSsaLivenessAnalysis *liveness_analysis,
    ValueSsaInterferenceGraph *graph,
    ValueSsaError *error) {
    ValueSsaCfgAnalysis owned_cfg;
    ValueSsaLivenessAnalysis owned_liveness;
    const ValueSsaCfgAnalysis *active_cfg = cfg_analysis;
    const ValueSsaLivenessAnalysis *active_liveness = liveness_analysis;
    unsigned char *live = NULL;
    int ok = 0;

    if (!function || !graph) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-205: invalid interference-graph contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-206: cannot compute interference graph for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    value_ssa_interference_graph_free(graph);
    graph->value_count = function->next_value_id;
    if (graph->value_count == 0) {
        return 1;
    }

    value_ssa_cfg_analysis_init(&owned_cfg);
    value_ssa_liveness_analysis_init(&owned_liveness);
    if (!active_cfg) {
        if (!value_ssa_compute_cfg_analysis(function, &owned_cfg, error)) {
            goto cleanup;
        }
        active_cfg = &owned_cfg;
    } else if (active_cfg->block_count != function->block_count) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-207: interference CFG block_count=%zu does not match function block_count=%zu",
            active_cfg->block_count,
            function->block_count);
        goto cleanup;
    }

    if (!active_liveness) {
        if (!value_ssa_compute_liveness_analysis(function, active_cfg, &owned_liveness, error)) {
            goto cleanup;
        }
        active_liveness = &owned_liveness;
    } else if (active_liveness->block_count != function->block_count ||
        active_liveness->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-208: interference liveness shape (%zu blocks, %zu values) does not match function (%zu blocks, %zu values)",
            active_liveness->block_count,
            active_liveness->value_count,
            function->block_count,
            function->next_value_id);
        goto cleanup;
    }

    graph->interferes = (unsigned char *)calloc(graph->value_count * graph->value_count, sizeof(unsigned char));
    live = (unsigned char *)calloc(graph->value_count, sizeof(unsigned char));
    if (!graph->interferes || !live) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-209: out of memory while building interference graph");
        goto cleanup;
    }

    {
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t live_offset;

            if (active_cfg->reachable && !active_cfg->reachable[block_index]) {
                continue;
            }

            memset(live, 0, graph->value_count * sizeof(unsigned char));
            live_offset = block_index * graph->value_count;
            memcpy(live, &active_liveness->live_out[live_offset], graph->value_count * sizeof(unsigned char));
            value_ssa_interference_add_live_clique(graph, live);
            value_ssa_interference_add_terminator_uses(&block->terminator, graph->value_count, live);
            value_ssa_interference_add_live_clique(graph, live);

            if (block->instruction_count > 0) {
                size_t instruction_index = block->instruction_count;

                while (instruction_index > 0) {
                    const ValueSsaInstruction *instruction = &block->instructions[--instruction_index];

                    if (instruction->has_result && instruction->result.kind == VALUE_SSA_VALUE_ID) {
                        size_t result_id = instruction->result.value_id;
                        size_t value_index;

                        for (value_index = 0; value_index < graph->value_count; ++value_index) {
                            if (live[value_index]) {
                                value_ssa_interference_add_edge(graph, result_id, value_index);
                            }
                        }
                        live[result_id] = 0;
                    }

                    value_ssa_interference_add_instruction_uses(instruction, graph->value_count, live);
                    value_ssa_interference_add_live_clique(graph, live);
                }
            }

            if (block->phi_count > 0) {
                size_t phi_index;

                for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                    const ValueSsaPhi *phi = &block->phis[phi_index];
                    size_t value_index;

                    for (value_index = 0; value_index < graph->value_count; ++value_index) {
                        if (live[value_index]) {
                            value_ssa_interference_add_edge(graph, phi->result_id, value_index);
                        }
                    }
                }

                for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                    live[block->phis[phi_index].result_id] = 1;
                }
                value_ssa_interference_add_live_clique(graph, live);
            }
        }
    }

    ok = 1;

cleanup:
    free(live);
    value_ssa_cfg_analysis_free(&owned_cfg);
    value_ssa_liveness_analysis_free(&owned_liveness);
    if (!ok) {
        value_ssa_interference_graph_free(graph);
    }
    return ok;
}

int value_ssa_compute_copy_affinity_graph(const ValueSsaFunction *function,
    const ValueSsaInterferenceGraph *interference_graph,
    ValueSsaCopyAffinityGraph *graph,
    ValueSsaError *error) {
    ValueSsaInterferenceGraph owned_interference;
    const ValueSsaInterferenceGraph *active_interference = interference_graph;
    int ok = 0;

    if (!function || !graph) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-210: invalid copy-affinity contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-211: cannot compute copy affinity for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    value_ssa_copy_affinity_graph_free(graph);
    graph->value_count = function->next_value_id;
    if (graph->value_count == 0) {
        return 1;
    }

    value_ssa_interference_graph_init(&owned_interference);
    if (!active_interference) {
        if (!value_ssa_compute_interference_graph(function, NULL, NULL, &owned_interference, error)) {
            goto cleanup;
        }
        active_interference = &owned_interference;
    } else if (active_interference->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-212: copy-affinity interference value_count=%zu does not match function value_count=%zu",
            active_interference->value_count,
            function->next_value_id);
        goto cleanup;
    }

    graph->weights = (size_t *)calloc(graph->value_count * graph->value_count, sizeof(size_t));
    if (!graph->weights) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-213: out of memory while building copy affinity graph");
        goto cleanup;
    }

    {
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                size_t dst;
                size_t src;

                if (instruction->kind != VALUE_SSA_INSTR_MOV || !instruction->has_result ||
                    instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.mov_value.kind != VALUE_SSA_VALUE_ID) {
                    continue;
                }

                dst = instruction->result.value_id;
                src = instruction->as.mov_value.value_id;
                if (dst == src ||
                    active_interference->interferes[dst * active_interference->value_count + src]) {
                    continue;
                }

                ++graph->weights[dst * graph->value_count + src];
                ++graph->weights[src * graph->value_count + dst];
            }
        }
    }

    ok = 1;

cleanup:
    value_ssa_interference_graph_free(&owned_interference);
    if (!ok) {
        value_ssa_copy_affinity_graph_free(graph);
    }
    return ok;
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
