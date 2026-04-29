#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa.c"
#else

void value_ssa_allocation_prep_init(ValueSsaAllocationPrep *prep) {
    if (!prep) {
        return;
    }

    prep->value_count = 0;
    prep->def_block_ids = NULL;
    prep->use_counts = NULL;
    prep->use_loop_depth_sums = NULL;
    prep->use_call_density_sums = NULL;
    prep->live_block_counts = NULL;
    prep->loop_depth_sums = NULL;
    prep->call_density_sums = NULL;
    prep->call_crossing_counts = NULL;
    prep->single_block_live_ranges = NULL;
    prep->live_block_offsets = NULL;
    prep->live_blocks = NULL;
    prep->live_blocks_count = 0;
    prep->interference_degrees = NULL;
    prep->affinity_sums = NULL;
    prep->move_related = NULL;
    prep->rematerializable = NULL;
    prep->split_child_values = NULL;
    prep->split_child_depths = NULL;
    prep->split_parent_value_ids = NULL;
    prep->split_family_root_value_ids = NULL;
    prep->candidates = NULL;
    prep->candidate_count = 0;
}

void value_ssa_allocation_prep_free(ValueSsaAllocationPrep *prep) {
    if (!prep) {
        return;
    }

    free(prep->def_block_ids);
    free(prep->use_counts);
    free(prep->use_loop_depth_sums);
    free(prep->use_call_density_sums);
    free(prep->live_block_counts);
    free(prep->loop_depth_sums);
    free(prep->call_density_sums);
    free(prep->call_crossing_counts);
    free(prep->single_block_live_ranges);
    free(prep->live_block_offsets);
    free(prep->live_blocks);
    free(prep->interference_degrees);
    free(prep->affinity_sums);
    free(prep->move_related);
    free(prep->rematerializable);
    free(prep->split_child_values);
    free(prep->split_child_depths);
    free(prep->split_parent_value_ids);
    free(prep->split_family_root_value_ids);
    free(prep->candidates);
    value_ssa_allocation_prep_init(prep);
}

static int value_ssa_compare_copy_affinity_candidates(const void *lhs_ptr, const void *rhs_ptr) {
    const ValueSsaCopyAffinityCandidate *lhs = (const ValueSsaCopyAffinityCandidate *)lhs_ptr;
    const ValueSsaCopyAffinityCandidate *rhs = (const ValueSsaCopyAffinityCandidate *)rhs_ptr;

    if (lhs->weight > rhs->weight) {
        return -1;
    }
    if (lhs->weight < rhs->weight) {
        return 1;
    }
    if (lhs->lhs < rhs->lhs) {
        return -1;
    }
    if (lhs->lhs > rhs->lhs) {
        return 1;
    }
    if (lhs->rhs < rhs->rhs) {
        return -1;
    }
    if (lhs->rhs > rhs->rhs) {
        return 1;
    }
    return 0;
}

static int value_ssa_alloc_collect_natural_loop_blocks(const ValueSsaCfgAnalysis *cfg,
    size_t header_block_id,
    size_t backedge_pred_block_id,
    unsigned char *loop_marks,
    size_t *stack) {
    size_t stack_size = 0;
    size_t block_count;

    if (!cfg || !loop_marks || !stack || header_block_id >= cfg->block_count || backedge_pred_block_id >= cfg->block_count) {
        return 0;
    }

    block_count = cfg->block_count;
    memset(loop_marks, 0, block_count * sizeof(unsigned char));
    loop_marks[header_block_id] = 1u;
    loop_marks[backedge_pred_block_id] = 1u;
    if (backedge_pred_block_id != header_block_id) {
        stack[stack_size++] = backedge_pred_block_id;
    }

    while (stack_size > 0) {
        size_t block_id = stack[--stack_size];
        size_t pred_block_id;

        for (pred_block_id = 0; pred_block_id < block_count; ++pred_block_id) {
            if (!cfg->reachable[pred_block_id] || !cfg->predecessor_matrix[pred_block_id * block_count + block_id]) {
                continue;
            }
            if (pred_block_id != header_block_id && !cfg->dominates[header_block_id * block_count + pred_block_id]) {
                continue;
            }
            if (loop_marks[pred_block_id]) {
                continue;
            }
            loop_marks[pred_block_id] = 1u;
            stack[stack_size++] = pred_block_id;
        }
    }

    return 1;
}

static int value_ssa_alloc_compute_block_loop_depths(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg,
    size_t *out_block_loop_depths) {
    unsigned char *loop_marks = NULL;
    size_t *stack = NULL;
    size_t block_id;
    int ok = 0;

    if (!function || !cfg || !out_block_loop_depths || cfg->block_count != function->block_count) {
        return 0;
    }

    memset(out_block_loop_depths, 0, function->block_count * sizeof(size_t));
    if (function->block_count == 0) {
        return 1;
    }

    loop_marks = (unsigned char *)calloc(function->block_count, sizeof(unsigned char));
    stack = (size_t *)malloc(function->block_count * sizeof(size_t));
    if (!loop_marks || !stack) {
        goto cleanup;
    }

    for (block_id = 0; block_id < function->block_count; ++block_id) {
        const ValueSsaBasicBlock *block = &function->blocks[block_id];
        size_t successors[2];
        size_t successor_count = 0;
        size_t succ_index;

        if (!cfg->reachable[block_id] || !block->has_terminator) {
            continue;
        }
        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_JUMP:
            successors[successor_count++] = block->terminator.as.jump_target;
            break;
        case VALUE_SSA_TERM_BRANCH:
            successors[successor_count++] = block->terminator.as.branch.then_target;
            if (block->terminator.as.branch.else_target != block->terminator.as.branch.then_target) {
                successors[successor_count++] = block->terminator.as.branch.else_target;
            }
            break;
        case VALUE_SSA_TERM_RETURN:
        default:
            break;
        }

        for (succ_index = 0; succ_index < successor_count; ++succ_index) {
            size_t succ_block_id = successors[succ_index];
            size_t loop_block_id;

            if (succ_block_id >= function->block_count || !cfg->reachable[succ_block_id]) {
                continue;
            }
            if (!cfg->dominates[succ_block_id * function->block_count + block_id]) {
                continue;
            }
            if (!value_ssa_alloc_collect_natural_loop_blocks(cfg, succ_block_id, block_id, loop_marks, stack)) {
                goto cleanup;
            }
            for (loop_block_id = 0; loop_block_id < function->block_count; ++loop_block_id) {
                if (loop_marks[loop_block_id]) {
                    ++out_block_loop_depths[loop_block_id];
                }
            }
        }
    }

    ok = 1;

cleanup:
    free(loop_marks);
    free(stack);
    return ok;
}

static int value_ssa_alloc_compute_block_call_counts(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg,
    size_t *out_block_call_counts) {
    size_t block_id;

    if (!function || !cfg || !out_block_call_counts || cfg->block_count != function->block_count) {
        return 0;
    }

    memset(out_block_call_counts, 0, function->block_count * sizeof(size_t));
    for (block_id = 0; block_id < function->block_count; ++block_id) {
        const ValueSsaBasicBlock *block = &function->blocks[block_id];
        size_t instruction_index;

        if (!cfg->reachable[block_id]) {
            continue;
        }
        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            if (block->instructions[instruction_index].kind == VALUE_SSA_INSTR_CALL) {
                ++out_block_call_counts[block_id];
            }
        }
    }

    return 1;
}

static int value_ssa_alloc_compute_value_call_crossing_counts(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg,
    const ValueSsaLivenessAnalysis *liveness,
    size_t *out_call_crossing_counts) {
    unsigned char *live = NULL;
    size_t block_id;
    int ok = 0;

    if (!function || !cfg || !liveness || !out_call_crossing_counts || cfg->block_count != function->block_count ||
        liveness->block_count != function->block_count || liveness->value_count != function->next_value_id) {
        return 0;
    }

    memset(out_call_crossing_counts, 0, (function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    if (function->next_value_id == 0) {
        return 1;
    }

    live = (unsigned char *)calloc(function->next_value_id, sizeof(unsigned char));
    if (!live) {
        goto cleanup;
    }

    for (block_id = 0; block_id < function->block_count; ++block_id) {
        const ValueSsaBasicBlock *block = &function->blocks[block_id];
        size_t instruction_index;
        size_t value_id;

        if (!cfg->reachable[block_id]) {
            continue;
        }

        memcpy(live,
            &liveness->live_out[block_id * function->next_value_id],
            function->next_value_id * sizeof(unsigned char));
        value_ssa_interference_add_terminator_uses(&block->terminator, function->next_value_id, live);

        for (instruction_index = block->instruction_count; instruction_index > 0; --instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index - 1];
            size_t result_value_id = (size_t)-1;

            if (instruction->kind == VALUE_SSA_INSTR_CALL && instruction->has_result &&
                instruction->result.kind == VALUE_SSA_VALUE_ID && instruction->result.value_id < function->next_value_id) {
                result_value_id = instruction->result.value_id;
            }

            if (instruction->kind == VALUE_SSA_INSTR_CALL) {
                for (value_id = 0; value_id < function->next_value_id; ++value_id) {
                    if (!live[value_id] || value_id == result_value_id) {
                        continue;
                    }
                    ++out_call_crossing_counts[value_id];
                }
            }

            if (instruction->has_result && instruction->result.kind == VALUE_SSA_VALUE_ID &&
                instruction->result.value_id < function->next_value_id) {
                live[instruction->result.value_id] = 0;
            }
            value_ssa_interference_add_instruction_uses(instruction, function->next_value_id, live);
        }
    }

    ok = 1;

cleanup:
    free(live);
    return ok;
}

static int value_ssa_alloc_compute_value_use_loop_depth_sums(const ValueSsaDefUseAnalysis *def_use,
    const size_t *block_loop_depths,
    size_t block_count,
    size_t *out_use_loop_depth_sums) {
    size_t value_id;

    if (!def_use || !block_loop_depths || !out_use_loop_depth_sums) {
        return 0;
    }

    memset(out_use_loop_depth_sums, 0, (def_use->value_count ? def_use->value_count : 1) * sizeof(size_t));
    for (value_id = 0; value_id < def_use->value_count; ++value_id) {
        size_t use_index;
        size_t use_begin = def_use->use_offsets ? def_use->use_offsets[value_id] : 0;
        size_t use_count = def_use->use_counts ? def_use->use_counts[value_id] : 0;

        for (use_index = 0; use_index < use_count; ++use_index) {
            const ValueSsaUseSite *use_site;

            if (!def_use->use_sites) {
                return 0;
            }
            use_site = &def_use->use_sites[use_begin + use_index];
            if (use_site->block_id >= block_count) {
                return 0;
            }
            out_use_loop_depth_sums[value_id] += block_loop_depths[use_site->block_id];
        }
    }

    return 1;
}

static int value_ssa_alloc_compute_value_use_call_density_sums(const ValueSsaDefUseAnalysis *def_use,
    const size_t *block_call_counts,
    size_t block_count,
    size_t *out_use_call_density_sums) {
    size_t value_id;

    if (!def_use || !block_call_counts || !out_use_call_density_sums) {
        return 0;
    }

    memset(out_use_call_density_sums, 0, (def_use->value_count ? def_use->value_count : 1) * sizeof(size_t));
    for (value_id = 0; value_id < def_use->value_count; ++value_id) {
        size_t use_index;
        size_t use_begin = def_use->use_offsets ? def_use->use_offsets[value_id] : 0;
        size_t use_count = def_use->use_counts ? def_use->use_counts[value_id] : 0;

        for (use_index = 0; use_index < use_count; ++use_index) {
            const ValueSsaUseSite *use_site;

            if (!def_use->use_sites) {
                return 0;
            }
            use_site = &def_use->use_sites[use_begin + use_index];
            if (use_site->block_id >= block_count) {
                return 0;
            }
            out_use_call_density_sums[value_id] += block_call_counts[use_site->block_id];
        }
    }

    return 1;
}

int value_ssa_compute_allocation_prep(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *def_use_analysis,
    const ValueSsaLivenessAnalysis *liveness_analysis,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *copy_affinity_graph,
    ValueSsaAllocationPrep *prep,
    ValueSsaError *error) {
    ValueSsaDefUseAnalysis owned_def_use;
    ValueSsaCfgAnalysis owned_cfg;
    ValueSsaLivenessAnalysis owned_liveness;
    ValueSsaInterferenceGraph owned_interference;
    ValueSsaCopyAffinityGraph owned_affinity;
    const ValueSsaDefUseAnalysis *active_def_use = def_use_analysis;
    const ValueSsaCfgAnalysis *active_cfg = NULL;
    const ValueSsaLivenessAnalysis *active_liveness = liveness_analysis;
    const ValueSsaInterferenceGraph *active_interference = interference_graph;
    const ValueSsaCopyAffinityGraph *active_affinity = copy_affinity_graph;
    unsigned char *covered_blocks = NULL;
    size_t *block_loop_depths = NULL;
    size_t *block_call_counts = NULL;
    int ok = 0;

    if (!function || !prep) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-214: invalid allocation-prep contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-215: cannot compute allocation prep for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    value_ssa_allocation_prep_free(prep);
    prep->value_count = function->next_value_id;
    if (prep->value_count == 0) {
        return 1;
    }

    value_ssa_def_use_analysis_init(&owned_def_use);
    value_ssa_cfg_analysis_init(&owned_cfg);
    value_ssa_liveness_analysis_init(&owned_liveness);
    value_ssa_interference_graph_init(&owned_interference);
    value_ssa_copy_affinity_graph_init(&owned_affinity);

    if (!value_ssa_compute_cfg_analysis(function, &owned_cfg, error)) {
        goto cleanup;
    }
    active_cfg = &owned_cfg;

    if (!active_def_use) {
        if (!value_ssa_compute_def_use_analysis(function, &owned_def_use, error)) {
            goto cleanup;
        }
        active_def_use = &owned_def_use;
    } else if (active_def_use->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-216: allocation-prep def-use value_count=%zu does not match function value_count=%zu",
            active_def_use->value_count,
            function->next_value_id);
        goto cleanup;
    }

    if (!active_liveness) {
        if (!value_ssa_compute_liveness_analysis(function, NULL, &owned_liveness, error)) {
            goto cleanup;
        }
        active_liveness = &owned_liveness;
    } else if (active_liveness->block_count != function->block_count ||
        active_liveness->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-217: allocation-prep liveness shape (%zu blocks, %zu values) does not match function (%zu blocks, %zu values)",
            active_liveness->block_count,
            active_liveness->value_count,
            function->block_count,
            function->next_value_id);
        goto cleanup;
    }

    if (!active_interference) {
        if (!value_ssa_compute_interference_graph(
                function, NULL, active_liveness, &owned_interference, error)) {
            goto cleanup;
        }
        active_interference = &owned_interference;
    } else if (active_interference->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-218: allocation-prep interference value_count=%zu does not match function value_count=%zu",
            active_interference->value_count,
            function->next_value_id);
        goto cleanup;
    }

    if (!active_affinity) {
        if (!value_ssa_compute_copy_affinity_graph(
                function, active_interference, &owned_affinity, error)) {
            goto cleanup;
        }
        active_affinity = &owned_affinity;
    } else if (active_affinity->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-219: allocation-prep affinity value_count=%zu does not match function value_count=%zu",
            active_affinity->value_count,
            function->next_value_id);
        goto cleanup;
    }

    prep->def_block_ids = (size_t *)malloc(prep->value_count * sizeof(size_t));
    prep->use_counts = (size_t *)malloc(prep->value_count * sizeof(size_t));
    prep->use_loop_depth_sums = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->use_call_density_sums = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->call_density_sums = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->call_crossing_counts = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->single_block_live_ranges = (unsigned char *)calloc(prep->value_count, sizeof(unsigned char));
    prep->live_block_offsets = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(prep->value_count, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(prep->value_count, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(prep->value_count, sizeof(unsigned char));
    prep->split_child_depths = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->split_parent_value_ids = (size_t *)malloc(prep->value_count * sizeof(size_t));
    prep->split_family_root_value_ids = (size_t *)malloc(prep->value_count * sizeof(size_t));
    covered_blocks = (unsigned char *)calloc(prep->value_count * function->block_count, sizeof(unsigned char));
    if (!prep->def_block_ids || !prep->use_counts || !prep->use_loop_depth_sums ||
        !prep->use_call_density_sums || !prep->live_block_counts ||
        !prep->loop_depth_sums ||
        !prep->call_density_sums || !prep->call_crossing_counts || !prep->single_block_live_ranges ||
        !prep->live_block_offsets ||
        !prep->interference_degrees || !prep->affinity_sums ||
        !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !prep->split_child_depths || !prep->split_parent_value_ids || !prep->split_family_root_value_ids ||
        !covered_blocks) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-220: out of memory while preparing allocation summary");
        goto cleanup;
    }
    block_loop_depths = (size_t *)calloc(function->block_count ? function->block_count : 1, sizeof(size_t));
    block_call_counts = (size_t *)calloc(function->block_count ? function->block_count : 1, sizeof(size_t));
    if (!block_loop_depths || !block_call_counts) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-220: out of memory while preparing allocation summary");
        goto cleanup;
    }
    if (!value_ssa_alloc_compute_block_loop_depths(function, active_cfg, block_loop_depths)) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-220: out of memory while preparing allocation summary");
        goto cleanup;
    }
    if (!value_ssa_alloc_compute_block_call_counts(function, active_cfg, block_call_counts)) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-220: out of memory while preparing allocation summary");
        goto cleanup;
    }
    if (!value_ssa_alloc_compute_value_call_crossing_counts(
            function, active_cfg, active_liveness, prep->call_crossing_counts)) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-220: out of memory while preparing allocation summary");
        goto cleanup;
    }
    if (!value_ssa_alloc_compute_value_use_loop_depth_sums(
            active_def_use, block_loop_depths, function->block_count, prep->use_loop_depth_sums)) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-220: out of memory while preparing allocation summary");
        goto cleanup;
    }
    if (!value_ssa_alloc_compute_value_use_call_density_sums(
            active_def_use, block_call_counts, function->block_count, prep->use_call_density_sums)) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-220: out of memory while preparing allocation summary");
        goto cleanup;
    }

    {
        size_t value_index;
        size_t total_live_blocks = 0;

        for (value_index = 0; value_index < prep->value_count; ++value_index) {
            size_t block_index;
            size_t degree = 0;
            size_t affinity_sum = 0;
            size_t live_block_count = 0;
            size_t loop_depth_sum = 0;
            size_t call_density_sum = 0;

            prep->def_block_ids[value_index] = active_def_use->has_def[value_index]
                ? active_def_use->def_block_ids[value_index]
                : (size_t)-1;
            prep->use_counts[value_index] = active_def_use->use_counts[value_index];

            if (prep->def_block_ids[value_index] != (size_t)-1 &&
                prep->def_block_ids[value_index] < function->block_count) {
                covered_blocks[value_index * function->block_count + prep->def_block_ids[value_index]] = 1;
            }

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                size_t slot = block_index * prep->value_count + value_index;
                size_t cover_slot = value_index * function->block_count + block_index;

                if (active_liveness->live_in[slot] || active_liveness->live_out[slot]) {
                    covered_blocks[cover_slot] = 1;
                }

                if (covered_blocks[cover_slot]) {
                    ++live_block_count;
                    loop_depth_sum += block_loop_depths[block_index];
                    call_density_sum += block_call_counts[block_index];
                }
            }

            for (block_index = 0; block_index < prep->value_count; ++block_index) {
                degree += active_interference->interferes[value_index * prep->value_count + block_index];
                affinity_sum += active_affinity->weights[value_index * prep->value_count + block_index];
            }

            prep->live_block_counts[value_index] = live_block_count;
            prep->loop_depth_sums[value_index] = loop_depth_sum;
            prep->call_density_sums[value_index] = call_density_sum;
            prep->single_block_live_ranges[value_index] = live_block_count <= 1 ? 1u : 0u;
            prep->interference_degrees[value_index] = degree;
            prep->affinity_sums[value_index] = affinity_sum;
            prep->move_related[value_index] = affinity_sum > 0 ? 1u : 0u;
            prep->rematerializable[value_index] = 0u;
            prep->split_child_values[value_index] = 0u;
            prep->split_child_depths[value_index] = 0;
            prep->split_parent_value_ids[value_index] = (size_t)-1;
            prep->split_family_root_value_ids[value_index] = value_index;
            if (active_def_use->has_def[value_index] &&
                active_def_use->def_instruction_indices[value_index] != (size_t)-1 &&
                prep->def_block_ids[value_index] < function->block_count) {
                const ValueSsaInstruction *def_instruction =
                    &function->blocks[prep->def_block_ids[value_index]]
                         .instructions[active_def_use->def_instruction_indices[value_index]];
                if (def_instruction->kind == VALUE_SSA_INSTR_MOV &&
                    def_instruction->as.mov_value.kind == VALUE_SSA_VALUE_IMMEDIATE) {
                    prep->rematerializable[value_index] = 1u;
                }
                if (def_instruction->kind == VALUE_SSA_INSTR_MOV &&
                    def_instruction->as.mov_value.kind == VALUE_SSA_VALUE_ID) {
                    size_t parent_value_id = def_instruction->as.mov_value.value_id;

                    prep->split_child_values[value_index] = 1u;
                    prep->split_child_depths[value_index] =
                        parent_value_id < prep->value_count && prep->split_child_depths
                        ? prep->split_child_depths[parent_value_id] + 1
                        : 1;
                    prep->split_parent_value_ids[value_index] = parent_value_id;
                    if (parent_value_id < prep->value_count && prep->split_family_root_value_ids) {
                        prep->split_family_root_value_ids[value_index] =
                            prep->split_family_root_value_ids[parent_value_id];
                    } else {
                        prep->split_family_root_value_ids[value_index] = parent_value_id;
                    }
                }
            }
            prep->live_block_offsets[value_index] = total_live_blocks;
            total_live_blocks += live_block_count;
        }

        prep->live_blocks_count = total_live_blocks;
        if (total_live_blocks > 0) {
            prep->live_blocks = (size_t *)malloc(total_live_blocks * sizeof(size_t));
            if (!prep->live_blocks) {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-221: out of memory while materializing live-block lists");
                goto cleanup;
            }

            for (value_index = 0; value_index < prep->value_count; ++value_index) {
                size_t block_index;
                size_t write_index = prep->live_block_offsets[value_index];

                for (block_index = 0; block_index < function->block_count; ++block_index) {
                    if (covered_blocks[value_index * function->block_count + block_index]) {
                        prep->live_blocks[write_index++] = block_index;
                    }
                }
            }
        }
    }

    {
        size_t lhs;

        for (lhs = 0; lhs < prep->value_count; ++lhs) {
            size_t rhs;

            for (rhs = lhs + 1; rhs < prep->value_count; ++rhs) {
                if (active_affinity->weights[lhs * prep->value_count + rhs] > 0) {
                    ++prep->candidate_count;
                }
            }
        }
    }

    if (prep->candidate_count > 0) {
        size_t lhs;
        size_t write_index = 0;

        prep->candidates = (ValueSsaCopyAffinityCandidate *)calloc(
            prep->candidate_count, sizeof(ValueSsaCopyAffinityCandidate));
        if (!prep->candidates) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-222: out of memory while materializing affinity candidates");
            goto cleanup;
        }

        for (lhs = 0; lhs < prep->value_count; ++lhs) {
            size_t rhs;

            for (rhs = lhs + 1; rhs < prep->value_count; ++rhs) {
                size_t weight = active_affinity->weights[lhs * prep->value_count + rhs];

                if (weight == 0) {
                    continue;
                }

                prep->candidates[write_index].lhs = lhs;
                prep->candidates[write_index].rhs = rhs;
                prep->candidates[write_index].weight = weight;
                ++write_index;
            }
        }

        qsort(prep->candidates,
            prep->candidate_count,
            sizeof(ValueSsaCopyAffinityCandidate),
            value_ssa_compare_copy_affinity_candidates);
    }

    ok = 1;

cleanup:
    free(block_call_counts);
    free(block_loop_depths);
    free(covered_blocks);
    value_ssa_def_use_analysis_free(&owned_def_use);
    value_ssa_cfg_analysis_free(&owned_cfg);
    value_ssa_liveness_analysis_free(&owned_liveness);
    value_ssa_interference_graph_free(&owned_interference);
    value_ssa_copy_affinity_graph_free(&owned_affinity);
    if (!ok) {
        value_ssa_allocation_prep_free(prep);
    }
    return ok;
}

#endif
