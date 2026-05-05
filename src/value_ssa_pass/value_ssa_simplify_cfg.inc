#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

static void value_ssa_remove_phi_predecessor_from_block(ValueSsaBasicBlock *block, size_t predecessor_block_id) {
    size_t phi_index;

    if (!block) {
        return;
    }

    for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
        ValueSsaPhi *phi = &block->phis[phi_index];
        size_t read_index;
        size_t write_index = 0;

        for (read_index = 0; read_index < phi->input_count; ++read_index) {
            if (phi->inputs[read_index].predecessor_block_id == predecessor_block_id) {
                continue;
            }

            if (write_index != read_index) {
                phi->inputs[write_index] = phi->inputs[read_index];
            }
            ++write_index;
        }

        phi->input_count = write_index;
    }
}

static void value_ssa_replace_phi_predecessor_in_block(ValueSsaBasicBlock *block,
    size_t old_predecessor_block_id,
    size_t new_predecessor_block_id) {
    size_t phi_index;

    if (!block) {
        return;
    }

    for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
        ValueSsaPhi *phi = &block->phis[phi_index];
        size_t input_index;

        for (input_index = 0; input_index < phi->input_count; ++input_index) {
            if (phi->inputs[input_index].predecessor_block_id == old_predecessor_block_id) {
                phi->inputs[input_index].predecessor_block_id = new_predecessor_block_id;
            }
        }
    }
}

static void value_ssa_remap_value_ref(ValueSsaValueRef *value, const size_t *old_to_new_value) {
    if (!value || !old_to_new_value || value->kind != VALUE_SSA_VALUE_ID) {
        return;
    }

    value->value_id = old_to_new_value[value->value_id];
}

static int value_ssa_block_is_empty_return(const ValueSsaBasicBlock *block, ValueSsaValueRef *out_value) {
    if (!block || !out_value ||
        block->phi_count != 0 ||
        block->instruction_count != 0 ||
        !block->has_terminator ||
        block->terminator.kind != VALUE_SSA_TERM_RETURN) {
        return 0;
    }
    if (!block->terminator.has_return_value) {
        return 0;
    }

    *out_value = block->terminator.as.return_value;
    return 1;
}

static int value_ssa_block_is_empty_jump(const ValueSsaBasicBlock *block, size_t *out_target) {
    if (!block || !out_target ||
        block->phi_count != 0 ||
        block->instruction_count != 0 ||
        !block->has_terminator ||
        block->terminator.kind != VALUE_SSA_TERM_JUMP) {
        return 0;
    }

    *out_target = block->terminator.as.jump_target;
    return 1;
}

static int value_ssa_resolve_empty_jump_target(const ValueSsaFunction *function,
    size_t source_block_id,
    size_t target_block_id,
    size_t *out_target) {
    size_t steps = 0;
    size_t current = target_block_id;

    if (!function || !out_target || target_block_id >= function->block_count) {
        return 0;
    }

    while (steps < function->block_count) {
        size_t next;

        if (!value_ssa_block_is_empty_jump(&function->blocks[current], &next) ||
            next >= function->block_count ||
            next == current ||
            next == source_block_id ||
            function->blocks[next].phi_count != 0) {
            break;
        }

        current = next;
        ++steps;
    }

    *out_target = current;
    return current != target_block_id;
}

static int value_ssa_rewrite_empty_jump_targets(ValueSsaFunction *function, ValueSsaError *error) {
    size_t block_index;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-124: invalid CFG simplify contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t rewritten_target;

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_JUMP:
            if (value_ssa_resolve_empty_jump_target(function,
                    block->id,
                    block->terminator.as.jump_target,
                    &rewritten_target)) {
                block->terminator.as.jump_target = rewritten_target;
            }
            break;
        case VALUE_SSA_TERM_BRANCH:
            if (value_ssa_resolve_empty_jump_target(function,
                    block->id,
                    block->terminator.as.branch.then_target,
                    &rewritten_target)) {
                block->terminator.as.branch.then_target = rewritten_target;
            }
            if (value_ssa_resolve_empty_jump_target(function,
                    block->id,
                    block->terminator.as.branch.else_target,
                    &rewritten_target)) {
                block->terminator.as.branch.else_target = rewritten_target;
            }
            break;
        case VALUE_SSA_TERM_RETURN:
        default:
            break;
        }
    }

    return 1;
}

static int value_ssa_rewrite_trivial_branches(ValueSsaFunction *function, ValueSsaError *error) {
    size_t block_index;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-124: invalid CFG simplify contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        ValueSsaBasicBlock *block = &function->blocks[block_index];

        if (block->terminator.kind != VALUE_SSA_TERM_BRANCH) {
            continue;
        }

        if (block->terminator.as.branch.then_target == block->terminator.as.branch.else_target) {
            block->terminator.kind = VALUE_SSA_TERM_JUMP;
            block->terminator.as.jump_target = block->terminator.as.branch.then_target;
            continue;
        }

        if (block->terminator.as.branch.condition.kind == VALUE_SSA_VALUE_IMMEDIATE) {
            size_t taken_target = block->terminator.as.branch.condition.immediate != 0
                ? block->terminator.as.branch.then_target
                : block->terminator.as.branch.else_target;
            size_t untaken_target = block->terminator.as.branch.condition.immediate != 0
                ? block->terminator.as.branch.else_target
                : block->terminator.as.branch.then_target;

            if (untaken_target < function->block_count) {
                value_ssa_remove_phi_predecessor_from_block(&function->blocks[untaken_target], block->id);
            }

            block->terminator.kind = VALUE_SSA_TERM_JUMP;
            block->terminator.as.jump_target = taken_target;
            continue;
        }

        if (block->terminator.as.branch.then_target < function->block_count &&
            block->terminator.as.branch.else_target < function->block_count) {
            ValueSsaValueRef then_value;
            ValueSsaValueRef else_value;
            const ValueSsaBasicBlock *then_block = &function->blocks[block->terminator.as.branch.then_target];
            const ValueSsaBasicBlock *else_block = &function->blocks[block->terminator.as.branch.else_target];

            if (value_ssa_block_is_empty_return(then_block, &then_value) &&
                value_ssa_block_is_empty_return(else_block, &else_value) &&
                value_ssa_value_refs_equal(then_value, else_value)) {
                block->terminator.kind = VALUE_SSA_TERM_RETURN;
                block->terminator.has_return_value = 1;
                block->terminator.as.return_value = then_value;
            }
        }
    }

    return 1;
}

static int value_ssa_rewrite_trivial_jumps(ValueSsaFunction *function, ValueSsaError *error) {
    size_t block_index;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-124: invalid CFG simplify contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        ValueSsaBasicBlock *block = &function->blocks[block_index];
        ValueSsaValueRef return_value;

        if (block->terminator.kind != VALUE_SSA_TERM_JUMP ||
            block->terminator.as.jump_target >= function->block_count ||
            !value_ssa_block_is_empty_return(&function->blocks[block->terminator.as.jump_target], &return_value)) {
            continue;
        }

        block->terminator.kind = VALUE_SSA_TERM_RETURN;
        block->terminator.has_return_value = 1;
        block->terminator.as.return_value = return_value;
    }

    return 1;
}

static int value_ssa_merge_single_predecessor_jump_blocks(ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    int *out_changed,
    ValueSsaError *error) {
    size_t block_index;

    if (!function || !analysis || !out_changed) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-124: invalid CFG simplify contract");
        return 0;
    }

    *out_changed = 0;
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t target_id;
        ValueSsaBasicBlock *target;
        size_t needed_capacity;

        if (block->terminator.kind != VALUE_SSA_TERM_JUMP) {
            continue;
        }

        target_id = block->terminator.as.jump_target;
        if (target_id >= function->block_count || target_id == block->id ||
            analysis->predecessor_counts[target_id] != 1) {
            continue;
        }

        target = &function->blocks[target_id];
        if (target->phi_count != 0) {
            continue;
        }

        needed_capacity = block->instruction_count + target->instruction_count;
        if (needed_capacity > block->instruction_capacity) {
            ValueSsaInstruction *grown =
                (ValueSsaInstruction *)realloc(block->instructions, needed_capacity * sizeof(ValueSsaInstruction));

            if (!grown) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-130: out of memory while merging bb.%zu into bb.%zu in function '%s'",
                    target->id,
                    block->id,
                    function->name ? function->name : "<anon>");
                return 0;
            }

            block->instructions = grown;
            block->instruction_capacity = needed_capacity;
        }

        if (target->instruction_count > 0) {
            memcpy(&block->instructions[block->instruction_count],
                target->instructions,
                target->instruction_count * sizeof(ValueSsaInstruction));
            block->instruction_count += target->instruction_count;
        }

        switch (target->terminator.kind) {
        case VALUE_SSA_TERM_JUMP:
            if (target->terminator.as.jump_target < function->block_count) {
                value_ssa_replace_phi_predecessor_in_block(&function->blocks[target->terminator.as.jump_target],
                    target->id,
                    block->id);
            }
            break;
        case VALUE_SSA_TERM_BRANCH:
            if (target->terminator.as.branch.then_target < function->block_count) {
                value_ssa_replace_phi_predecessor_in_block(&function->blocks[target->terminator.as.branch.then_target],
                    target->id,
                    block->id);
            }
            if (target->terminator.as.branch.else_target < function->block_count &&
                target->terminator.as.branch.else_target != target->terminator.as.branch.then_target) {
                value_ssa_replace_phi_predecessor_in_block(&function->blocks[target->terminator.as.branch.else_target],
                    target->id,
                    block->id);
            }
            break;
        case VALUE_SSA_TERM_RETURN:
        default:
            break;
        }

        block->terminator = target->terminator;

        target->instruction_count = 0;
        target->phi_count = 0;
        target->has_terminator = 0;
        target->terminator.kind = VALUE_SSA_TERM_RETURN;
        target->terminator.has_return_value = 1;
        target->terminator.as.return_value = value_ssa_value_immediate(0);

        *out_changed = 1;
        return 1;
    }

    return 1;
}

static int value_ssa_compact_reachable_blocks(ValueSsaFunction *function, ValueSsaError *error) {
    unsigned char *reachable = NULL;
    size_t *old_to_new = NULL;
    ValueSsaBasicBlock *new_blocks = NULL;
    size_t reachable_count = 0;
    size_t old_block_count;
    size_t old_index;
    size_t new_index = 0;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-124: invalid CFG simplify contract");
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }

    old_block_count = function->block_count;
    reachable = (unsigned char *)calloc(old_block_count, sizeof(unsigned char));
    old_to_new = (size_t *)malloc(old_block_count * sizeof(size_t));
    if (!reachable || !old_to_new) {
        free(reachable);
        free(old_to_new);
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-125: out of memory while computing reachable blocks for function '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    if (!value_ssa_mark_reachable_blocks(function, reachable, error)) {
        free(reachable);
        free(old_to_new);
        return 0;
    }

    for (old_index = 0; old_index < old_block_count; ++old_index) {
        old_to_new[old_index] = (size_t)-1;
        if (reachable[old_index]) {
            ++reachable_count;
        }
    }

    if (reachable_count == old_block_count) {
        free(reachable);
        free(old_to_new);
        return 1;
    }

    new_blocks = (ValueSsaBasicBlock *)calloc(reachable_count, sizeof(ValueSsaBasicBlock));
    if (!new_blocks) {
        free(reachable);
        free(old_to_new);
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-126: out of memory while compacting reachable blocks for function '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    for (old_index = 0; old_index < old_block_count; ++old_index) {
        if (!reachable[old_index]) {
            value_ssa_basic_block_free(&function->blocks[old_index]);
            continue;
        }

        old_to_new[old_index] = new_index;
        new_blocks[new_index] = function->blocks[old_index];
        new_blocks[new_index].id = new_index;
        ++new_index;
    }

    free(function->blocks);
    function->blocks = new_blocks;
    function->block_count = reachable_count;
    function->block_capacity = reachable_count;

    for (new_index = 0; new_index < function->block_count; ++new_index) {
        ValueSsaBasicBlock *block = &function->blocks[new_index];
        size_t phi_index;

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            ValueSsaPhi *phi = &block->phis[phi_index];
            size_t input_read_index;
            size_t input_write_index = 0;

            for (input_read_index = 0; input_read_index < phi->input_count; ++input_read_index) {
                size_t predecessor_block_id = phi->inputs[input_read_index].predecessor_block_id;

                if (predecessor_block_id >= old_block_count || old_to_new[predecessor_block_id] == (size_t)-1) {
                    continue;
                }

                phi->inputs[input_write_index] = phi->inputs[input_read_index];
                phi->inputs[input_write_index].predecessor_block_id = old_to_new[predecessor_block_id];
                ++input_write_index;
            }

            phi->input_count = input_write_index;
        }

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_JUMP:
            if (block->terminator.as.jump_target >= old_block_count ||
                old_to_new[block->terminator.as.jump_target] == (size_t)-1) {
                free(reachable);
                free(old_to_new);
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-127: CFG simplify lost jump target bb.%zu in function '%s'",
                    block->terminator.as.jump_target,
                    function->name ? function->name : "<anon>");
                return 0;
            }
            block->terminator.as.jump_target = old_to_new[block->terminator.as.jump_target];
            break;
        case VALUE_SSA_TERM_BRANCH:
            if (block->terminator.as.branch.then_target >= old_block_count ||
                block->terminator.as.branch.else_target >= old_block_count ||
                old_to_new[block->terminator.as.branch.then_target] == (size_t)-1 ||
                old_to_new[block->terminator.as.branch.else_target] == (size_t)-1) {
                free(reachable);
                free(old_to_new);
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-128: CFG simplify lost branch successor in function '%s'",
                    function->name ? function->name : "<anon>");
                return 0;
            }
            block->terminator.as.branch.then_target = old_to_new[block->terminator.as.branch.then_target];
            block->terminator.as.branch.else_target = old_to_new[block->terminator.as.branch.else_target];
            break;
        case VALUE_SSA_TERM_RETURN:
        default:
            break;
        }
    }

    free(reachable);
    free(old_to_new);
    return 1;
}

static int value_ssa_compact_value_ids(ValueSsaFunction *function, ValueSsaError *error) {
    size_t *old_to_new_value = NULL;
    size_t next_value_id = 0;
    size_t block_index;

    if (!function) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-124: invalid CFG simplify contract");
        return 0;
    }
    if (!function->has_body || function->next_value_id == 0) {
        return 1;
    }

    old_to_new_value = (size_t *)malloc(function->next_value_id * sizeof(size_t));
    if (!old_to_new_value) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-129: out of memory while compacting SSA values for function '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    for (block_index = 0; block_index < function->next_value_id; ++block_index) {
        old_to_new_value[block_index] = (size_t)-1;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t phi_index;
        size_t instruction_index;

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            old_to_new_value[block->phis[phi_index].result_id] = next_value_id++;
        }
        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->has_result && instruction->result.kind == VALUE_SSA_VALUE_ID) {
                old_to_new_value[instruction->result.value_id] = next_value_id++;
            }
        }
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t phi_index;
        size_t instruction_index;

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            ValueSsaPhi *phi = &block->phis[phi_index];
            size_t input_index;

            phi->result_id = old_to_new_value[phi->result_id];
            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                value_ssa_remap_value_ref(&phi->inputs[input_index].value, old_to_new_value);
            }
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            ValueSsaInstruction *instruction = &block->instructions[instruction_index];
            size_t arg_index;

            if (instruction->has_result && instruction->result.kind == VALUE_SSA_VALUE_ID) {
                instruction->result.value_id = old_to_new_value[instruction->result.value_id];
            }

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_MOV:
                value_ssa_remap_value_ref(&instruction->as.mov_value, old_to_new_value);
                break;
            case VALUE_SSA_INSTR_BINARY:
                value_ssa_remap_value_ref(&instruction->as.binary.lhs, old_to_new_value);
                value_ssa_remap_value_ref(&instruction->as.binary.rhs, old_to_new_value);
                break;
            case VALUE_SSA_INSTR_CALL:
                for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    value_ssa_remap_value_ref(&instruction->as.call.args[arg_index], old_to_new_value);
                }
                break;
            case VALUE_SSA_INSTR_LOAD_INDIRECT:
                value_ssa_remap_value_ref(&instruction->as.load_indirect_addr, old_to_new_value);
                break;
            case VALUE_SSA_INSTR_STORE_LOCAL:
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                value_ssa_remap_value_ref(&instruction->as.store.value, old_to_new_value);
                break;
            case VALUE_SSA_INSTR_STORE_INDIRECT:
                value_ssa_remap_value_ref(&instruction->as.store_indirect.addr, old_to_new_value);
                value_ssa_remap_value_ref(&instruction->as.store_indirect.value, old_to_new_value);
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
            value_ssa_remap_value_ref(&block->terminator.as.return_value, old_to_new_value);
            break;
        case VALUE_SSA_TERM_BRANCH:
            value_ssa_remap_value_ref(&block->terminator.as.branch.condition, old_to_new_value);
            break;
        case VALUE_SSA_TERM_JUMP:
        default:
            break;
        }
    }

    function->next_value_id = next_value_id;
    free(old_to_new_value);
    return 1;
}

int value_ssa_simplify_cfg(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-124: invalid CFG simplify contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        int changed = 0;

        if (!function->has_body || function->block_count == 0) {
            continue;
        }

        do {
            ValueSsaCfgAnalysis analysis;

            changed = 0;
            if (!value_ssa_rewrite_empty_jump_targets(function, error) ||
                !value_ssa_rewrite_trivial_branches(function, error) ||
                !value_ssa_rewrite_trivial_jumps(function, error)) {
                return 0;
            }

            value_ssa_cfg_analysis_init(&analysis);
            if (!value_ssa_compute_cfg_analysis(function, &analysis, error) ||
                !value_ssa_merge_single_predecessor_jump_blocks(function, &analysis, &changed, error)) {
                value_ssa_cfg_analysis_free(&analysis);
                return 0;
            }
            value_ssa_cfg_analysis_free(&analysis);
        } while (changed);

        if (!value_ssa_compact_reachable_blocks(function, error) || !value_ssa_compact_value_ids(function, error)) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

#endif
