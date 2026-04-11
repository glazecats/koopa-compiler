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
            case VALUE_SSA_INSTR_STORE_LOCAL:
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                value_ssa_record_use_count(instruction->as.store.value, analysis->use_counts);
                break;
            case VALUE_SSA_INSTR_LOAD_LOCAL:
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                break;
            default:
                break;
            }
        }

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            value_ssa_record_use_count(block->terminator.as.return_value, analysis->use_counts);
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
            case VALUE_SSA_INSTR_LOAD_LOCAL:
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                break;
            default:
                break;
            }
        }

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
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
