#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

static int value_ssa_alloc_rewrite_value_use(ValueSsaInstruction *instruction,
    ValueSsaUseRole role,
    size_t operand_index,
    ValueSsaValueRef replacement,
    ValueSsaError *error) {
    if (!instruction) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-029: invalid spill-use rewrite");
        return 0;
    }

    switch (role) {
    case VALUE_SSA_USE_ROLE_MOV_VALUE:
        instruction->as.mov_value = replacement;
        return 1;
    case VALUE_SSA_USE_ROLE_BINARY_LHS:
        instruction->as.binary.lhs = replacement;
        return 1;
    case VALUE_SSA_USE_ROLE_BINARY_RHS:
        instruction->as.binary.rhs = replacement;
        return 1;
    case VALUE_SSA_USE_ROLE_CALL_ARG:
        if (operand_index >= instruction->as.call.arg_count) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-030: invalid spill call-arg rewrite");
            return 0;
        }
        instruction->as.call.args[operand_index] = replacement;
        return 1;
    case VALUE_SSA_USE_ROLE_STORE_VALUE:
        instruction->as.store.value = replacement;
        return 1;
    case VALUE_SSA_USE_ROLE_LOAD_INDIRECT_ADDR:
        instruction->as.load_indirect_addr = replacement;
        return 1;
    case VALUE_SSA_USE_ROLE_STORE_INDIRECT_ADDR:
        instruction->as.store_indirect.addr = replacement;
        return 1;
    case VALUE_SSA_USE_ROLE_STORE_INDIRECT_VALUE:
        instruction->as.store_indirect.value = replacement;
        return 1;
    default:
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-031: unsupported spill use role");
        return 0;
    }
}

static int value_ssa_alloc_rewrite_terminator_use(ValueSsaBasicBlock *block,
    ValueSsaUseRole role,
    ValueSsaValueRef replacement,
    ValueSsaError *error) {
    if (!block || !block->has_terminator) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-036: invalid spill terminator rewrite");
        return 0;
    }

    switch (role) {
    case VALUE_SSA_USE_ROLE_RETURN_VALUE:
        if (block->terminator.kind != VALUE_SSA_TERM_RETURN) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-037: invalid spill return rewrite");
            return 0;
        }
        block->terminator.as.return_value = replacement;
        return 1;
    case VALUE_SSA_USE_ROLE_BRANCH_CONDITION:
        if (block->terminator.kind != VALUE_SSA_TERM_BRANCH) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-038: invalid spill branch rewrite");
            return 0;
        }
        block->terminator.as.branch.condition = replacement;
        return 1;
    default:
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-039: unsupported spill terminator role");
        return 0;
    }
}

static int value_ssa_alloc_rewrite_phi_input(ValueSsaBasicBlock *block,
    size_t phi_index,
    size_t input_index,
    ValueSsaValueRef replacement,
    ValueSsaError *error) {
    if (!block || phi_index >= block->phi_count || input_index >= block->phis[phi_index].input_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-040: invalid spill phi rewrite");
        return 0;
    }

    block->phis[phi_index].inputs[input_index].value = replacement;
    return 1;
}

static int value_ssa_alloc_ensure_spill_local(ValueSsaFunction *function,
    size_t spill_slot,
    size_t *out_spill_local_id,
    ValueSsaError *error) {
    size_t local_index;
    char expected_name[64];

    if (!function || !out_spill_local_id) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-063: invalid spill-local ensure contract");
        return 0;
    }

    snprintf(expected_name, sizeof(expected_name), "spill.%zu", spill_slot);
    for (local_index = 0; local_index < function->local_count; ++local_index) {
        const ValueSsaLocal *local = &function->locals[local_index];

        if (!local->source_name || strcmp(local->source_name, expected_name) != 0) {
            continue;
        }
        *out_spill_local_id = local_index;
        return 1;
    }

    if (!value_ssa_function_append_local(function, expected_name, 0, out_spill_local_id, error)) {
        return 0;
    }
    return 1;
}

static int value_ssa_alloc_try_rewrite_mov_use_in_place(ValueSsaFunction *function,
    size_t spill_local_id,
    const ValueSsaUseSite *use,
    int *out_rewrote,
    ValueSsaError *error) {
    ValueSsaInstruction *instruction;

    if (!function || !use || !out_rewrote) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-061: invalid in-place mov spill rewrite");
        return 0;
    }

    *out_rewrote = 0;
    if (use->kind != VALUE_SSA_USE_INSTR) {
        return 1;
    }
    if (use->role != VALUE_SSA_USE_ROLE_MOV_VALUE || use->block_id >= function->block_count ||
        use->instruction_index >= function->blocks[use->block_id].instruction_count) {
        return 1;
    }

    instruction = &function->blocks[use->block_id].instructions[use->instruction_index];
    if (instruction->kind != VALUE_SSA_INSTR_MOV || !instruction->has_result ||
        instruction->result.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }

    instruction->kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction->as.load_slot = value_ssa_slot_local(spill_local_id);
    *out_rewrote = 1;
    return 1;
}

static int value_ssa_alloc_value_refs_equal(ValueSsaValueRef lhs, ValueSsaValueRef rhs) {
    if (lhs.kind != rhs.kind) {
        return 0;
    }
    if (lhs.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        return lhs.immediate == rhs.immediate;
    }
    return lhs.value_id == rhs.value_id;
}

static int value_ssa_alloc_find_split_child_mov_in_block(const ValueSsaBasicBlock *block,
    size_t parent_value_id,
    size_t *out_split_value_id) {
    size_t instruction_index;

    if (out_split_value_id) {
        *out_split_value_id = (size_t)-1;
    }
    if (!block || !out_split_value_id) {
        return 0;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        if (instruction->kind != VALUE_SSA_INSTR_MOV || !instruction->has_result ||
            instruction->result.kind != VALUE_SSA_VALUE_ID ||
            instruction->as.mov_value.kind != VALUE_SSA_VALUE_ID ||
            instruction->as.mov_value.value_id != parent_value_id) {
            continue;
        }

        *out_split_value_id = instruction->result.value_id;
        return 1;
    }

    return 0;
}

static int value_ssa_alloc_find_unique_predecessor_block(const ValueSsaFunction *function,
    size_t block_id,
    size_t *out_predecessor_block_id) {
    size_t candidate_block_id;
    size_t found_predecessor_block_id = (size_t)-1;

    if (out_predecessor_block_id) {
        *out_predecessor_block_id = (size_t)-1;
    }
    if (!function || !out_predecessor_block_id || block_id >= function->block_count) {
        return 0;
    }

    for (candidate_block_id = 0; candidate_block_id < function->block_count; ++candidate_block_id) {
        const ValueSsaBasicBlock *candidate = &function->blocks[candidate_block_id];
        int reaches_block = 0;

        if (!candidate->has_terminator) {
            continue;
        }
        switch (candidate->terminator.kind) {
        case VALUE_SSA_TERM_JUMP:
            reaches_block = candidate->terminator.as.jump_target == block_id;
            break;
        case VALUE_SSA_TERM_BRANCH:
            reaches_block = candidate->terminator.as.branch.then_target == block_id ||
                candidate->terminator.as.branch.else_target == block_id;
            break;
        case VALUE_SSA_TERM_RETURN:
        default:
            reaches_block = 0;
            break;
        }
        if (!reaches_block) {
            continue;
        }
        if (found_predecessor_block_id != (size_t)-1) {
            return 0;
        }
        found_predecessor_block_id = candidate_block_id;
    }

    if (found_predecessor_block_id == (size_t)-1) {
        return 0;
    }
    *out_predecessor_block_id = found_predecessor_block_id;
    return 1;
}

static int value_ssa_alloc_insert_local_split_child_for_block_uses(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *def_use,
    size_t value_id,
    size_t block_id,
    size_t def_block_id,
    size_t def_instruction_index,
    size_t *out_split_value_id,
    int *out_rewrote_any,
    ValueSsaError *error) {
    size_t use_index;
    size_t first_use_index = (size_t)-1;
    size_t predecessor_use_count = 0;
    ValueSsaInstruction split_instruction;
    ValueSsaBasicBlock *block;
    size_t split_value_id;

    if (out_split_value_id) {
        *out_split_value_id = (size_t)-1;
    }
    if (out_rewrote_any) {
        *out_rewrote_any = 0;
    }
    if (!function || !def_use || !out_split_value_id || !out_rewrote_any || block_id >= function->block_count ||
        value_id >= def_use->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-222: invalid local split-child rewrite contract");
        return 0;
    }

    for (use_index = def_use->use_offsets[value_id];
         use_index < def_use->use_offsets[value_id] + def_use->use_counts[value_id];
         ++use_index) {
        const ValueSsaUseSite *use = &def_use->use_sites[use_index];
        size_t use_position;

        if (use->block_id != block_id) {
            continue;
        }
        if (use->kind != VALUE_SSA_USE_INSTR && use->kind != VALUE_SSA_USE_TERM) {
            continue;
        }

        use_position = use->kind == VALUE_SSA_USE_INSTR
            ? use->instruction_index
            : function->blocks[block_id].instruction_count;
        if (block_id == def_block_id &&
            def_instruction_index != (size_t)-1 &&
            use_position <= def_instruction_index) {
            continue;
        }
        if (first_use_index == (size_t)-1 || use_position < first_use_index) {
            first_use_index = use_position;
        }
        ++predecessor_use_count;
    }

    if (predecessor_use_count == 0 || first_use_index == (size_t)-1) {
        return 1;
    }

    split_value_id = value_ssa_function_allocate_value(function);
    if (split_value_id == (size_t)-1) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-223: out of value ids for predecessor local split child");
        return 0;
    }

    memset(&split_instruction, 0, sizeof(split_instruction));
    split_instruction.kind = VALUE_SSA_INSTR_MOV;
    split_instruction.has_result = 1;
    split_instruction.result = value_ssa_value_id(split_value_id);
    split_instruction.as.mov_value = value_ssa_value_id(value_id);

    block = &function->blocks[block_id];
    if (!value_ssa_alloc_insert_instruction(block, first_use_index, &split_instruction, error)) {
        return 0;
    }

    for (use_index = def_use->use_offsets[value_id];
         use_index < def_use->use_offsets[value_id] + def_use->use_counts[value_id];
         ++use_index) {
        const ValueSsaUseSite *use = &def_use->use_sites[use_index];
        size_t rewrite_instruction_index;
        size_t use_position;

        if (use->block_id != block_id) {
            continue;
        }
        if (use->kind != VALUE_SSA_USE_INSTR && use->kind != VALUE_SSA_USE_TERM) {
            continue;
        }

        use_position = use->kind == VALUE_SSA_USE_INSTR
            ? use->instruction_index
            : block->instruction_count;
        if (block_id == def_block_id &&
            def_instruction_index != (size_t)-1 &&
            use_position <= def_instruction_index) {
            continue;
        }

        if (use->kind == VALUE_SSA_USE_INSTR) {
            rewrite_instruction_index = use->instruction_index >= first_use_index
                ? use->instruction_index + 1
                : use->instruction_index;
            if (rewrite_instruction_index >= block->instruction_count) {
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-224: invalid predecessor local split child use");
                return 0;
            }
            if (!value_ssa_alloc_rewrite_value_use(
                    &block->instructions[rewrite_instruction_index],
                    use->role,
                    use->operand_index,
                    value_ssa_value_id(split_value_id),
                    error)) {
                return 0;
            }
            continue;
        }

        if (!value_ssa_alloc_rewrite_terminator_use(
                block, use->role, value_ssa_value_id(split_value_id), error)) {
            return 0;
        }
    }

    *out_split_value_id = split_value_id;
    *out_rewrote_any = 1;
    return 1;
}

static int value_ssa_alloc_redirect_phi_predecessor(ValueSsaBasicBlock *block,
    size_t old_predecessor_block_id,
    size_t new_predecessor_block_id,
    size_t current_phi_index,
    size_t current_input_index,
    ValueSsaValueRef current_old_value,
    ValueSsaValueRef current_replacement,
    ValueSsaBasicBlock *materialization_block,
    ValueSsaFunction *function,
    ValueSsaError *error) {
    size_t phi_index;

    if (!block || !materialization_block || !function) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-054: invalid phi predecessor redirect");
        return 0;
    }

    for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
        ValueSsaPhi *phi = &block->phis[phi_index];
        size_t input_index;
        int found = 0;

        for (input_index = 0; input_index < phi->input_count; ++input_index) {
            ValueSsaValueRef old_value;
            ValueSsaInstruction move_instruction;
            size_t move_value_id;

            if (phi->inputs[input_index].predecessor_block_id != old_predecessor_block_id) {
                continue;
            }
            old_value = phi->inputs[input_index].value;
            phi->inputs[input_index].predecessor_block_id = new_predecessor_block_id;
            if ((phi_index == current_phi_index && input_index == current_input_index) ||
                value_ssa_alloc_value_refs_equal(old_value, current_old_value)) {
                phi->inputs[input_index].value = current_replacement;
                found = 1;
                break;
            }

            move_value_id = value_ssa_function_allocate_value(function);
            if (move_value_id == (size_t)-1) {
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-058: out of value ids for split-edge phi glue");
                return 0;
            }
            memset(&move_instruction, 0, sizeof(move_instruction));
            move_instruction.kind = VALUE_SSA_INSTR_MOV;
            move_instruction.has_result = 1;
            move_instruction.result = value_ssa_value_id(move_value_id);
            move_instruction.as.mov_value = old_value;
            if (!value_ssa_alloc_insert_instruction(
                    materialization_block, materialization_block->instruction_count, &move_instruction, error)) {
                return 0;
            }
            phi->inputs[input_index].value = value_ssa_value_id(move_value_id);
            found = 1;
            break;
        }

        if (!found) {
            value_ssa_alloc_set_error(
                error, 0, 0, "VALUE-SSA-ALLOC-055: phi predecessor redirect missed phi input");
            return 0;
        }
    }

    return 1;
}

static int value_ssa_alloc_block_is_reusable_split_edge(const ValueSsaBasicBlock *block,
    size_t successor_block_id) {
    size_t instruction_index;
    int has_spill_load = 0;

    if (!block || block->phi_count != 0 || !block->has_terminator ||
        block->terminator.kind != VALUE_SSA_TERM_JUMP ||
        block->terminator.as.jump_target != successor_block_id ||
        block->instruction_count == 0) {
        return 0;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        if (!instruction->has_result) {
            return 0;
        }
        if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
            instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL) {
            has_spill_load = 1;
            continue;
        }
        if (instruction->kind == VALUE_SSA_INSTR_MOV) {
            continue;
        }
        return 0;
    }

    return has_spill_load;
}

static int value_ssa_alloc_block_is_reusable_split_child_edge(const ValueSsaBasicBlock *block,
    size_t successor_block_id) {
    size_t instruction_index;

    if (!block || block->phi_count != 0 || !block->has_terminator ||
        block->terminator.kind != VALUE_SSA_TERM_JUMP ||
        block->terminator.as.jump_target != successor_block_id) {
        return 0;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        if (!instruction->has_result || instruction->kind != VALUE_SSA_INSTR_MOV ||
            instruction->as.mov_value.kind != VALUE_SSA_VALUE_ID) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_alloc_find_existing_split_edge(const ValueSsaFunction *function,
    const ValueSsaBasicBlock *predecessor_block,
    size_t successor_block_id,
    size_t *out_split_block_id) {
    size_t candidate_block_id = (size_t)-1;

    if (!function || !predecessor_block || !out_split_block_id ||
        !predecessor_block->has_terminator || predecessor_block->terminator.kind != VALUE_SSA_TERM_BRANCH) {
        return 0;
    }

    if (predecessor_block->terminator.as.branch.then_target < function->block_count) {
        size_t then_target = predecessor_block->terminator.as.branch.then_target;
        if (value_ssa_alloc_block_is_reusable_split_edge(&function->blocks[then_target], successor_block_id)) {
            candidate_block_id = then_target;
        }
    }
    if (predecessor_block->terminator.as.branch.else_target < function->block_count) {
        size_t else_target = predecessor_block->terminator.as.branch.else_target;
        if (value_ssa_alloc_block_is_reusable_split_edge(&function->blocks[else_target], successor_block_id)) {
            if (candidate_block_id != (size_t)-1 && candidate_block_id != else_target) {
                return 0;
            }
            candidate_block_id = else_target;
        }
    }

    if (candidate_block_id == (size_t)-1) {
        return 0;
    }
    *out_split_block_id = candidate_block_id;
    return 1;
}

static int value_ssa_alloc_find_existing_split_child_edge(const ValueSsaFunction *function,
    const ValueSsaBasicBlock *predecessor_block,
    size_t successor_block_id,
    size_t *out_split_block_id) {
    size_t candidate_block_id = (size_t)-1;

    if (!function || !predecessor_block || !out_split_block_id ||
        !predecessor_block->has_terminator || predecessor_block->terminator.kind != VALUE_SSA_TERM_BRANCH) {
        return 0;
    }

    if (predecessor_block->terminator.as.branch.then_target < function->block_count) {
        size_t then_target = predecessor_block->terminator.as.branch.then_target;
        if (value_ssa_alloc_block_is_reusable_split_child_edge(&function->blocks[then_target], successor_block_id)) {
            candidate_block_id = then_target;
        }
    }
    if (predecessor_block->terminator.as.branch.else_target < function->block_count) {
        size_t else_target = predecessor_block->terminator.as.branch.else_target;
        if (value_ssa_alloc_block_is_reusable_split_child_edge(&function->blocks[else_target], successor_block_id)) {
            if (candidate_block_id != (size_t)-1 && candidate_block_id != else_target) {
                return 0;
            }
            candidate_block_id = else_target;
        }
    }

    if (candidate_block_id == (size_t)-1) {
        return 0;
    }
    *out_split_block_id = candidate_block_id;
    return 1;
}

static int value_ssa_alloc_append_reload_to_split_edge(ValueSsaFunction *function,
    size_t split_block_id,
    size_t successor_block_id,
    const ValueSsaInstruction *load_instruction,
    size_t reload_value_id,
    size_t phi_index,
    size_t input_index,
    int *out_did_reuse,
    ValueSsaError *error) {
    ValueSsaBasicBlock *split_block;
    ValueSsaBasicBlock *successor_block;
    ValueSsaValueRef target_old_value;
    size_t existing_instruction_index;
    ValueSsaInstruction move_instruction;

    if (!function || !load_instruction || !out_did_reuse ||
        split_block_id >= function->block_count || successor_block_id >= function->block_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-057: invalid spill edge reuse contract");
        return 0;
    }

    *out_did_reuse = 0;
    split_block = &function->blocks[split_block_id];
    if (!value_ssa_alloc_block_is_reusable_split_edge(split_block, successor_block_id)) {
        return 1;
    }

    successor_block = &function->blocks[successor_block_id];
    if (phi_index >= successor_block->phi_count ||
        input_index >= successor_block->phis[phi_index].input_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-060: invalid reused split-edge phi input");
        return 0;
    }
    target_old_value = successor_block->phis[phi_index].inputs[input_index].value;
    for (existing_instruction_index = 0; existing_instruction_index < split_block->instruction_count; ++existing_instruction_index) {
        const ValueSsaInstruction *instruction = &split_block->instructions[existing_instruction_index];

        if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
            continue;
        }
        if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
            load_instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
            instruction->as.load_slot.kind == load_instruction->as.load_slot.kind &&
            instruction->as.load_slot.id == load_instruction->as.load_slot.id) {
            successor_block->phis[phi_index].inputs[input_index].predecessor_block_id = split_block_id;
            if (!value_ssa_alloc_rewrite_phi_input(successor_block,
                    phi_index,
                    input_index,
                    value_ssa_value_id(instruction->result.value_id),
                    error)) {
                return 0;
            }
            *out_did_reuse = 1;
            return 1;
        }
        if (instruction->kind == VALUE_SSA_INSTR_MOV &&
            value_ssa_alloc_value_refs_equal(instruction->as.mov_value, target_old_value)) {
            memset(&move_instruction, 0, sizeof(move_instruction));
            move_instruction.kind = VALUE_SSA_INSTR_MOV;
            move_instruction.has_result = 1;
            move_instruction.result = value_ssa_value_id(reload_value_id);
            move_instruction.as.mov_value = value_ssa_value_id(instruction->result.value_id);
            if (!value_ssa_alloc_insert_instruction(split_block, split_block->instruction_count, &move_instruction, error)) {
                return 0;
            }
            successor_block->phis[phi_index].inputs[input_index].predecessor_block_id = split_block_id;
            if (!value_ssa_alloc_rewrite_phi_input(successor_block,
                    phi_index,
                    input_index,
                    value_ssa_value_id(reload_value_id),
                    error)) {
                return 0;
            }
            *out_did_reuse = 1;
            return 1;
        }
    }

    if (!value_ssa_alloc_insert_instruction(split_block, split_block->instruction_count, load_instruction, error)) {
        return 0;
    }
    successor_block->phis[phi_index].inputs[input_index].predecessor_block_id = split_block_id;
    if (!value_ssa_alloc_rewrite_phi_input(
            successor_block, phi_index, input_index, value_ssa_value_id(reload_value_id), error)) {
        return 0;
    }

    *out_did_reuse = 1;
    return 1;
}

static int value_ssa_alloc_split_branch_edge_for_phi_reload(ValueSsaFunction *function,
    size_t predecessor_block_id,
    size_t successor_block_id,
    const ValueSsaInstruction *load_instruction,
    size_t reload_value_id,
    size_t phi_index,
    size_t input_index,
    int *out_did_split,
    ValueSsaError *error) {
    ValueSsaBasicBlock *predecessor_block;
    ValueSsaBasicBlock *successor_block;
    ValueSsaBasicBlock *split_block = NULL;
    size_t split_block_id = 0;
    size_t existing_split_block_id = 0;
    ValueSsaValueRef current_old_value;
    int did_reuse = 0;

    if (!function || !load_instruction || !out_did_split || predecessor_block_id >= function->block_count ||
        successor_block_id >= function->block_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-056: invalid spill edge split contract");
        return 0;
    }

    *out_did_split = 0;
    predecessor_block = &function->blocks[predecessor_block_id];
    if (value_ssa_alloc_find_existing_split_edge(
            function, predecessor_block, successor_block_id, &existing_split_block_id)) {
        if (!value_ssa_alloc_append_reload_to_split_edge(function,
                existing_split_block_id,
                successor_block_id,
                load_instruction,
                reload_value_id,
                phi_index,
                input_index,
                &did_reuse,
                error)) {
            return 0;
        }
        if (did_reuse) {
            *out_did_split = 1;
            return 1;
        }
    }

    if (!predecessor_block->has_terminator || predecessor_block->terminator.kind != VALUE_SSA_TERM_BRANCH) {
        return 1;
    }
    if (predecessor_block->terminator.as.branch.then_target == predecessor_block->terminator.as.branch.else_target) {
        return 1;
    }
    if (predecessor_block->terminator.as.branch.then_target != successor_block_id &&
        predecessor_block->terminator.as.branch.else_target != successor_block_id) {
        return 1;
    }
    successor_block = &function->blocks[successor_block_id];
    if (phi_index >= successor_block->phi_count ||
        input_index >= successor_block->phis[phi_index].input_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-059: invalid split-edge phi input");
        return 0;
    }
    current_old_value = successor_block->phis[phi_index].inputs[input_index].value;

    if (!value_ssa_function_append_block(function, NULL, &split_block, error)) {
        return 0;
    }
    split_block_id = function->block_count - 1;
    predecessor_block = &function->blocks[predecessor_block_id];
    successor_block = &function->blocks[successor_block_id];
    split_block = &function->blocks[split_block_id];

    if (predecessor_block->terminator.as.branch.then_target == successor_block_id) {
        predecessor_block->terminator.as.branch.then_target = split_block_id;
    } else {
        predecessor_block->terminator.as.branch.else_target = split_block_id;
    }

    if (!value_ssa_alloc_insert_instruction(split_block, 0, load_instruction, error) ||
        !value_ssa_block_set_jump(split_block, successor_block_id, error) ||
        !value_ssa_alloc_redirect_phi_predecessor(successor_block,
            predecessor_block_id,
            split_block_id,
            phi_index,
            input_index,
            current_old_value,
            value_ssa_value_id(reload_value_id),
            split_block,
            function,
            error)) {
        return 0;
    }

    *out_did_split = 1;
    return 1;
}

static int value_ssa_alloc_try_reuse_reload_in_block(const ValueSsaFunction *function,
    size_t block_id,
    size_t spill_local_id,
    size_t before_instruction_index,
    int *out_reused,
    ValueSsaValueRef *out_replacement,
    int *out_blocked_by_store,
    ValueSsaError *error) {
    const ValueSsaBasicBlock *block;
    size_t instruction_index;

    if (!function || !out_reused || !out_replacement || block_id >= function->block_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-064: invalid spill reload-reuse query");
        return 0;
    }

    block = &function->blocks[block_id];
    if (before_instruction_index > block->instruction_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-065: invalid spill reload-reuse boundary");
        return 0;
    }

    *out_reused = 0;
    if (out_blocked_by_store) {
        *out_blocked_by_store = 0;
    }
    memset(out_replacement, 0, sizeof(*out_replacement));
    for (instruction_index = before_instruction_index; instruction_index > 0; --instruction_index) {
        const ValueSsaInstruction *instruction = &block->instructions[instruction_index - 1];

        if (instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL &&
            instruction->as.store.slot.kind == VALUE_SSA_SLOT_LOCAL &&
            instruction->as.store.slot.id == spill_local_id) {
            if (out_blocked_by_store) {
                *out_blocked_by_store = 1;
            }
            return 1;
        }
        if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
            instruction->has_result &&
            instruction->result.kind == VALUE_SSA_VALUE_ID &&
            instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            instruction->as.load_slot.id == spill_local_id) {
            *out_reused = 1;
            *out_replacement = value_ssa_value_id(instruction->result.value_id);
            return 1;
        }
    }

    return 1;
}

static int value_ssa_alloc_block_has_spill_store_before(const ValueSsaFunction *function,
    size_t block_id,
    size_t spill_local_id,
    size_t before_instruction_index,
    int *out_has_store,
    ValueSsaError *error) {
    const ValueSsaBasicBlock *block;
    size_t instruction_index;

    if (!function || !out_has_store || block_id >= function->block_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-084: invalid spill-store scan block");
        return 0;
    }

    block = &function->blocks[block_id];
    if (before_instruction_index > block->instruction_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-085: invalid spill-store scan boundary");
        return 0;
    }

    *out_has_store = 0;
    for (instruction_index = 0; instruction_index < before_instruction_index; ++instruction_index) {
        const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        if (instruction->kind != VALUE_SSA_INSTR_STORE_LOCAL ||
            instruction->as.store.slot.kind != VALUE_SSA_SLOT_LOCAL ||
            instruction->as.store.slot.id != spill_local_id) {
            continue;
        }
        *out_has_store = 1;
        return 1;
    }

    return 1;
}

static int value_ssa_alloc_reload_reaches_block_without_spill_store(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg,
    size_t reload_block_id,
    size_t target_block_id,
    size_t spill_local_id,
    size_t target_before_instruction_index,
    int *out_reaches,
    ValueSsaError *error) {
    unsigned char *visited = NULL;
    size_t *stack = NULL;
    size_t stack_size = 0;
    int ok = 0;

    if (!function || !cfg || !out_reaches || reload_block_id >= function->block_count ||
        target_block_id >= function->block_count || reload_block_id >= cfg->block_count ||
        target_block_id >= cfg->block_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-086: invalid dominating spill reload query");
        return 0;
    }

    *out_reaches = 0;
    if (!cfg->dominates || !cfg->predecessor_matrix || !cfg->predecessor_counts) {
        return 1;
    }
    if (!cfg->dominates[reload_block_id * cfg->block_count + target_block_id]) {
        return 1;
    }

    visited = (unsigned char *)calloc(cfg->block_count ? cfg->block_count : 1, sizeof(unsigned char));
    stack = (size_t *)malloc((cfg->block_count ? cfg->block_count : 1) * sizeof(size_t));
    if (!visited || !stack) {
        free(visited);
        free(stack);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-087: out of memory tracing dominating spill reload");
        return 0;
    }

    stack[stack_size++] = target_block_id;
    while (stack_size > 0) {
        size_t block_id = stack[--stack_size];
        size_t predecessor_block_id;
        int has_store = 0;
        size_t before_instruction_index = function->blocks[block_id].instruction_count;

        if (visited[block_id]) {
            continue;
        }
        visited[block_id] = 1;

        if (block_id == target_block_id) {
            before_instruction_index = target_before_instruction_index;
        }
        if (block_id != reload_block_id) {
            if (!value_ssa_alloc_block_has_spill_store_before(
                    function, block_id, spill_local_id, before_instruction_index, &has_store, error)) {
                goto cleanup;
            }
            if (has_store) {
                ok = 1;
                goto cleanup;
            }
        }

        if (block_id == reload_block_id) {
            continue;
        }

        for (predecessor_block_id = 0; predecessor_block_id < cfg->block_count; ++predecessor_block_id) {
            if (!cfg->predecessor_matrix[predecessor_block_id * cfg->block_count + block_id]) {
                continue;
            }
            if (predecessor_block_id != reload_block_id &&
                !cfg->dominates[reload_block_id * cfg->block_count + predecessor_block_id]) {
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-088: dominating spill reload escaped dominance region");
                goto cleanup;
            }
            if (!visited[predecessor_block_id]) {
                stack[stack_size++] = predecessor_block_id;
            }
        }
    }

    *out_reaches = 1;
    ok = 1;

cleanup:
    free(stack);
    free(visited);
    return ok;
}

static int value_ssa_alloc_try_reuse_reload_from_unique_predecessor_chain(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg,
    size_t block_id,
    size_t spill_local_id,
    size_t before_instruction_index,
    int *out_reused,
    ValueSsaValueRef *out_replacement,
    ValueSsaError *error) {
    size_t current_block_id;

    if (!function || !cfg || !out_reused || !out_replacement || block_id >= function->block_count ||
        block_id >= cfg->block_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-066: invalid predecessor reload-reuse query");
        return 0;
    }

    *out_reused = 0;
    memset(out_replacement, 0, sizeof(*out_replacement));
    if (!cfg->predecessor_counts || !cfg->predecessor_matrix) {
        return 1;
    }

    current_block_id = block_id;
    if (current_block_id < function->block_count) {
        int blocked_by_store = 0;

        if (!value_ssa_alloc_block_has_spill_store_before(
                function,
                current_block_id,
                spill_local_id,
                before_instruction_index,
                &blocked_by_store,
                error)) {
            return 0;
        }
        if (blocked_by_store) {
            return 1;
        }
    }
    while (current_block_id < cfg->block_count && cfg->predecessor_counts[current_block_id] == 1) {
        size_t predecessor_block_id = (size_t)-1;
        size_t candidate_block_id;
        int blocked_by_store = 0;

        for (candidate_block_id = 0; candidate_block_id < cfg->block_count; ++candidate_block_id) {
            if (!cfg->predecessor_matrix[candidate_block_id * cfg->block_count + current_block_id]) {
                continue;
            }
            predecessor_block_id = candidate_block_id;
            break;
        }
        if (predecessor_block_id == (size_t)-1) {
            return 1;
        }

        if (!value_ssa_alloc_try_reuse_reload_in_block(
                function,
                predecessor_block_id,
                spill_local_id,
                function->blocks[predecessor_block_id].instruction_count,
                out_reused,
                out_replacement,
                &blocked_by_store,
                error)) {
            return 0;
        }
        if (*out_reused) {
            return 1;
        }
        if (blocked_by_store) {
            return 1;
        }

        current_block_id = predecessor_block_id;
    }

    return 1;
}

static int value_ssa_alloc_try_reuse_reload_from_dominating_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg,
    size_t block_id,
    size_t spill_local_id,
    size_t before_instruction_index,
    int *out_reused,
    ValueSsaValueRef *out_replacement,
    ValueSsaError *error) {
    size_t current_block_id;

    if (!function || !cfg || !out_reused || !out_replacement || block_id >= function->block_count ||
        block_id >= cfg->block_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-089: invalid dominating spill reload candidate query");
        return 0;
    }

    *out_reused = 0;
    memset(out_replacement, 0, sizeof(*out_replacement));
    if (!cfg->immediate_dominator) {
        return 1;
    }

    current_block_id = block_id;
    while (current_block_id < cfg->block_count &&
           cfg->immediate_dominator[current_block_id] != (size_t)-1 &&
           cfg->immediate_dominator[current_block_id] < cfg->block_count) {
        int candidate_reused = 0;
        int blocked_by_store = 0;
        int reaches_block = 0;
        ValueSsaValueRef candidate_replacement;

        current_block_id = cfg->immediate_dominator[current_block_id];
        memset(&candidate_replacement, 0, sizeof(candidate_replacement));
        if (!value_ssa_alloc_try_reuse_reload_in_block(
                function,
                current_block_id,
                spill_local_id,
                function->blocks[current_block_id].instruction_count,
                &candidate_reused,
                &candidate_replacement,
                &blocked_by_store,
                error)) {
            return 0;
        }
        if (!candidate_reused) {
            if (blocked_by_store) {
                return 1;
            }
            continue;
        }
        if (!value_ssa_alloc_reload_reaches_block_without_spill_store(function,
                cfg,
                current_block_id,
                block_id,
                spill_local_id,
                before_instruction_index,
                &reaches_block,
                error)) {
            return 0;
        }
        if (!reaches_block) {
            continue;
        }

        *out_reused = 1;
        *out_replacement = candidate_replacement;
        return 1;
    }

    return 1;
}

static int value_ssa_alloc_insert_reload_for_current_use(ValueSsaFunction *function,
    size_t spill_local_id,
    const ValueSsaUseSite *use,
    ValueSsaError *error) {
    ValueSsaCfgAnalysis cfg;
    ValueSsaInstruction load_instruction;
    ValueSsaValueRef reusable_reload;
    size_t reload_value_id;
    size_t insert_index;
    int cfg_ready = 0;
    int reused_reload = 0;
    int rewrote_in_place = 0;

    if (!function || !use) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-044: invalid spill reload insertion");
        return 0;
    }
    value_ssa_cfg_analysis_init(&cfg);
    if (use->kind == VALUE_SSA_USE_INSTR) {
        if (!value_ssa_alloc_try_reuse_reload_in_block(
                function,
                use->block_id,
                spill_local_id,
                use->instruction_index,
                &reused_reload,
                &reusable_reload,
                NULL,
                error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }
        if (reused_reload) {
            ValueSsaBasicBlock *use_block = &function->blocks[use->block_id];

            reused_reload = value_ssa_alloc_rewrite_value_use(
                &use_block->instructions[use->instruction_index],
                use->role,
                use->operand_index,
                reusable_reload,
                error);
            value_ssa_cfg_analysis_free(&cfg);
            return reused_reload;
        }
        if (!value_ssa_compute_cfg_analysis(function, &cfg, error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }
        cfg_ready = 1;
        if (!value_ssa_alloc_try_reuse_reload_from_unique_predecessor_chain(
                function,
                &cfg,
                use->block_id,
                spill_local_id,
                use->instruction_index,
                &reused_reload,
                &reusable_reload,
                error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }
        if (reused_reload) {
            ValueSsaBasicBlock *use_block = &function->blocks[use->block_id];

            reused_reload = value_ssa_alloc_rewrite_value_use(
                &use_block->instructions[use->instruction_index],
                use->role,
                use->operand_index,
                reusable_reload,
                error);
            value_ssa_cfg_analysis_free(&cfg);
            return reused_reload;
        }
        if (!value_ssa_alloc_try_reuse_reload_from_dominating_block(
                function,
                &cfg,
                use->block_id,
                spill_local_id,
                use->instruction_index,
                &reused_reload,
                &reusable_reload,
                error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }
        if (reused_reload) {
            ValueSsaBasicBlock *use_block = &function->blocks[use->block_id];

            reused_reload = value_ssa_alloc_rewrite_value_use(
                &use_block->instructions[use->instruction_index],
                use->role,
                use->operand_index,
                reusable_reload,
                error);
            value_ssa_cfg_analysis_free(&cfg);
            return reused_reload;
        }
    } else if (use->kind == VALUE_SSA_USE_TERM) {
        if (!value_ssa_alloc_try_reuse_reload_in_block(
                function,
                use->block_id,
                spill_local_id,
                function->blocks[use->block_id].instruction_count,
                &reused_reload,
                &reusable_reload,
                NULL,
                error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }
        if (reused_reload) {
            reused_reload = value_ssa_alloc_rewrite_terminator_use(
                function->blocks + use->block_id, use->role, reusable_reload, error);
            value_ssa_cfg_analysis_free(&cfg);
            return reused_reload;
        }
        if (!value_ssa_compute_cfg_analysis(function, &cfg, error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }
        cfg_ready = 1;
        if (!value_ssa_alloc_try_reuse_reload_from_unique_predecessor_chain(
                function,
                &cfg,
                use->block_id,
                spill_local_id,
                function->blocks[use->block_id].instruction_count,
                &reused_reload,
                &reusable_reload,
                error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }
        if (reused_reload) {
            reused_reload = value_ssa_alloc_rewrite_terminator_use(
                function->blocks + use->block_id, use->role, reusable_reload, error);
            value_ssa_cfg_analysis_free(&cfg);
            return reused_reload;
        }
        if (!value_ssa_alloc_try_reuse_reload_from_dominating_block(
                function,
                &cfg,
                use->block_id,
                spill_local_id,
                function->blocks[use->block_id].instruction_count,
                &reused_reload,
                &reusable_reload,
                error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }
        if (reused_reload) {
            reused_reload = value_ssa_alloc_rewrite_terminator_use(
                function->blocks + use->block_id, use->role, reusable_reload, error);
            value_ssa_cfg_analysis_free(&cfg);
            return reused_reload;
        }
    } else if (use->kind == VALUE_SSA_USE_PHI) {
        ValueSsaBasicBlock *use_block = &function->blocks[use->block_id];
        size_t predecessor_block_id;

        if (use->phi_index >= use_block->phi_count || use->operand_index >= use_block->phis[use->phi_index].input_count) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-041: invalid spill phi use");
            return 0;
        }

        predecessor_block_id = use_block->phis[use->phi_index].inputs[use->operand_index].predecessor_block_id;
        if (predecessor_block_id >= function->block_count) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-042: invalid spill phi predecessor");
            return 0;
        }
        if (!value_ssa_alloc_try_reuse_reload_in_block(
                function,
                predecessor_block_id,
                spill_local_id,
                function->blocks[predecessor_block_id].instruction_count,
                &reused_reload,
                &reusable_reload,
                NULL,
                error)) {
            return 0;
        }
        if (reused_reload) {
            return value_ssa_alloc_rewrite_phi_input(
                use_block, use->phi_index, use->operand_index, reusable_reload, error);
        }
        if (!value_ssa_compute_cfg_analysis(function, &cfg, error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }
        cfg_ready = 1;
        if (!value_ssa_alloc_try_reuse_reload_from_dominating_block(
                function,
                &cfg,
                predecessor_block_id,
                spill_local_id,
                function->blocks[predecessor_block_id].instruction_count,
                &reused_reload,
                &reusable_reload,
                error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }
        if (reused_reload) {
            reused_reload = value_ssa_alloc_rewrite_phi_input(
                use_block, use->phi_index, use->operand_index, reusable_reload, error);
            value_ssa_cfg_analysis_free(&cfg);
            return reused_reload;
        }
    }
    if (!value_ssa_alloc_try_rewrite_mov_use_in_place(function, spill_local_id, use, &rewrote_in_place, error)) {
        value_ssa_cfg_analysis_free(&cfg);
        return 0;
    }
    if (rewrote_in_place) {
        value_ssa_cfg_analysis_free(&cfg);
        return 1;
    }

    reload_value_id = value_ssa_function_allocate_value(function);
    if (reload_value_id == (size_t)-1) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-035: out of value ids for spill reload");
        return 0;
    }

    memset(&load_instruction, 0, sizeof(load_instruction));
    load_instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    load_instruction.has_result = 1;
    load_instruction.result = value_ssa_value_id(reload_value_id);
    load_instruction.as.load_slot = value_ssa_slot_local(spill_local_id);
    if (use->kind == VALUE_SSA_USE_INSTR) {
        ValueSsaBasicBlock *use_block = &function->blocks[use->block_id];

        insert_index = use->instruction_index;
        if (!value_ssa_alloc_insert_instruction(use_block, insert_index, &load_instruction, error) ||
            !value_ssa_alloc_rewrite_value_use(
                &use_block->instructions[insert_index + 1],
                use->role,
                use->operand_index,
                value_ssa_value_id(reload_value_id),
                error)) {
            value_ssa_cfg_analysis_free(&cfg);
            return 0;
        }
    } else {
        ValueSsaBasicBlock *use_block = &function->blocks[use->block_id];

        if (use->kind == VALUE_SSA_USE_TERM) {
            insert_index = use_block->instruction_count;
            if (!value_ssa_alloc_insert_instruction(use_block, insert_index, &load_instruction, error) ||
                !value_ssa_alloc_rewrite_terminator_use(
                    use_block, use->role, value_ssa_value_id(reload_value_id), error)) {
                value_ssa_cfg_analysis_free(&cfg);
                return 0;
            }
        } else {
            size_t predecessor_block_id;
            ValueSsaBasicBlock *predecessor_block;
            int did_split = 0;

            if (use->phi_index >= use_block->phi_count || use->operand_index >= use_block->phis[use->phi_index].input_count) {
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-041: invalid spill phi use");
                return 0;
            }

            predecessor_block_id = use_block->phis[use->phi_index].inputs[use->operand_index].predecessor_block_id;
            if (predecessor_block_id >= function->block_count) {
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-042: invalid spill phi predecessor");
                return 0;
            }
            if (!value_ssa_alloc_split_branch_edge_for_phi_reload(function,
                    predecessor_block_id,
                    use->block_id,
                    &load_instruction,
                    reload_value_id,
                    use->phi_index,
                    use->operand_index,
                    &did_split,
                    error)) {
                value_ssa_cfg_analysis_free(&cfg);
                return 0;
            }
            if (did_split) {
                value_ssa_cfg_analysis_free(&cfg);
                return 1;
            }

            use_block = &function->blocks[use->block_id];
            predecessor_block = &function->blocks[predecessor_block_id];
            insert_index = predecessor_block->instruction_count;
            if (!value_ssa_alloc_insert_instruction(predecessor_block, insert_index, &load_instruction, error) ||
                !value_ssa_alloc_rewrite_phi_input(
                    use_block, use->phi_index, use->operand_index, value_ssa_value_id(reload_value_id), error)) {
                value_ssa_cfg_analysis_free(&cfg);
                return 0;
            }
        }
    }

    if (cfg_ready) {
        value_ssa_cfg_analysis_free(&cfg);
    } else {
        value_ssa_cfg_analysis_free(&cfg);
    }
    return 1;
}

static int value_ssa_alloc_use_is_spill_store_self_use(const ValueSsaFunction *function,
    const ValueSsaUseSite *use,
    size_t value_id,
    size_t spill_local_id,
    int *out_is_self_use,
    ValueSsaError *error) {
    const ValueSsaInstruction *instruction;

    if (!function || !use || !out_is_self_use) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-051: invalid spill self-use query");
        return 0;
    }

    *out_is_self_use = 0;
    if (use->kind != VALUE_SSA_USE_INSTR || use->role != VALUE_SSA_USE_ROLE_STORE_VALUE) {
        return 1;
    }
    if (use->block_id >= function->block_count ||
        use->instruction_index >= function->blocks[use->block_id].instruction_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-052: invalid spill self-use site");
        return 0;
    }

    instruction = &function->blocks[use->block_id].instructions[use->instruction_index];
    *out_is_self_use = instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL &&
                       instruction->as.store.slot.kind == VALUE_SSA_SLOT_LOCAL &&
                       instruction->as.store.slot.id == spill_local_id &&
                       instruction->as.store.value.kind == VALUE_SSA_VALUE_ID &&
                       instruction->as.store.value.value_id == value_id;
    return 1;
}

static int value_ssa_alloc_find_next_spill_rewrite_use(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *def_use,
    size_t value_id,
    size_t spill_local_id,
    int *out_has_use,
    ValueSsaUseSite *out_use,
    ValueSsaError *error) {
    size_t use_index;

    if (!function || !def_use || !out_has_use || !out_use || value_id >= def_use->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-053: invalid spill use scan");
        return 0;
    }

    *out_has_use = 0;
    memset(out_use, 0, sizeof(*out_use));
    for (use_index = def_use->use_offsets[value_id];
         use_index < def_use->use_offsets[value_id] + def_use->use_counts[value_id];
         ++use_index) {
        const ValueSsaUseSite *use = &def_use->use_sites[use_index];
        int is_self_use = 0;

        if (use->kind != VALUE_SSA_USE_INSTR && use->kind != VALUE_SSA_USE_TERM && use->kind != VALUE_SSA_USE_PHI) {
            value_ssa_alloc_set_error(
                error, 0, 0, "VALUE-SSA-ALLOC-034: spill rewrite only supports instruction/terminator/phi uses");
            return 0;
        }

        if (!value_ssa_alloc_use_is_spill_store_self_use(
                function, use, value_id, spill_local_id, &is_self_use, error)) {
            return 0;
        }
        if (is_self_use) {
            continue;
        }

        *out_has_use = 1;
        *out_use = *use;
        return 1;
    }

    return 1;
}

static int value_ssa_alloc_rewrite_spilled_value(ValueSsaFunction *function,
    size_t value_id,
    size_t spill_slot,
    ValueSsaError *error) {
    ValueSsaDefUseAnalysis def_use;
    ValueSsaBasicBlock *block;
    ValueSsaInstruction store_instruction;
    size_t spill_local_id;
    size_t store_index;
    int is_phi_def = 0;

    value_ssa_def_use_analysis_init(&def_use);
    if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
        value_ssa_def_use_analysis_free(&def_use);
        return 0;
    }
    if (value_id >= def_use.value_count || !def_use.has_def[value_id] || def_use.def_block_ids[value_id] == (size_t)-1 ||
        (def_use.def_phi_indices[value_id] == (size_t)-1 && def_use.def_instruction_indices[value_id] == (size_t)-1)) {
        size_t def_block_id = value_id < def_use.value_count ? def_use.def_block_ids[value_id] : (size_t)-1;
        size_t def_phi_index = value_id < def_use.value_count ? def_use.def_phi_indices[value_id] : (size_t)-1;
        size_t def_instruction_index = value_id < def_use.value_count ? def_use.def_instruction_indices[value_id] : (size_t)-1;

        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_alloc_set_error(
            error,
            0,
            0,
            "VALUE-SSA-ALLOC-033: spill rewrite supports instruction- or phi-defined values only "
            "(ssa.%zu block=%zu phi=%zu instr=%zu)",
            value_id,
            def_block_id,
            def_phi_index,
            def_instruction_index);
        return 0;
    }

    block = &function->blocks[def_use.def_block_ids[value_id]];
    is_phi_def = def_use.def_phi_indices[value_id] != (size_t)-1;
    store_index = is_phi_def ? 0 : def_use.def_instruction_indices[value_id] + 1;
    if (!value_ssa_alloc_ensure_spill_local(function, spill_slot, &spill_local_id, error)) {
        value_ssa_def_use_analysis_free(&def_use);
        return 0;
    }

    memset(&store_instruction, 0, sizeof(store_instruction));
    store_instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    store_instruction.as.store.slot = value_ssa_slot_local(spill_local_id);
    store_instruction.as.store.value = value_ssa_value_id(value_id);
    if (!value_ssa_alloc_insert_instruction(block, store_index, &store_instruction, error)) {
        value_ssa_def_use_analysis_free(&def_use);
        return 0;
    }
    value_ssa_def_use_analysis_free(&def_use);

    for (;;) {
        ValueSsaUseSite use;
        int has_use = 0;

        value_ssa_def_use_analysis_init(&def_use);
        if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }
        if (!value_ssa_alloc_find_next_spill_rewrite_use(
                function, &def_use, value_id, spill_local_id, &has_use, &use, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }
        value_ssa_def_use_analysis_free(&def_use);
        if (!has_use) {
            break;
        }

        if (!value_ssa_alloc_insert_reload_for_current_use(function, spill_local_id, &use, error)) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_alloc_result_value_is_rewriteable_spill(const ValueSsaFunction *function,
    const ValueSsaAllocationResult *function_result,
    size_t spill_local_floor,
    size_t value_id,
    int *out_rewriteable,
    ValueSsaError *error) {
    ValueSsaDefUseAnalysis def_use;
    const ValueSsaInstruction *def_instruction;
    size_t ancestor_value_id;
    size_t use_index;

    if (!function || !function_result || !out_rewriteable || value_id >= function_result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-045: invalid rewriteable-spill query");
        return 0;
    }

    *out_rewriteable = 0;
    if (!function_result->spill_flags[value_id] || function_result->spill_slots[value_id] == (size_t)-1 ||
        value_id >= function->next_value_id) {
        return 1;
    }

    value_ssa_def_use_analysis_init(&def_use);
    if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
        value_ssa_def_use_analysis_free(&def_use);
        return 0;
    }

    if (value_id >= def_use.value_count || !def_use.has_def[value_id] || def_use.def_block_ids[value_id] == (size_t)-1 ||
        (def_use.def_phi_indices[value_id] == (size_t)-1 && def_use.def_instruction_indices[value_id] == (size_t)-1)) {
        value_ssa_def_use_analysis_free(&def_use);
        return 1;
    }

    if (def_use.def_instruction_indices[value_id] != (size_t)-1) {
        def_instruction = &function->blocks[def_use.def_block_ids[value_id]].instructions[def_use.def_instruction_indices[value_id]];
        if (def_instruction->kind == VALUE_SSA_INSTR_MOV &&
            def_instruction->has_result &&
            def_instruction->result.kind == VALUE_SSA_VALUE_ID &&
            def_instruction->as.mov_value.kind == VALUE_SSA_VALUE_ID) {
            ancestor_value_id = def_instruction->as.mov_value.value_id;
            while (ancestor_value_id < value_id && ancestor_value_id < function_result->value_count &&
                ancestor_value_id < def_use.value_count && def_use.has_def[ancestor_value_id] &&
                def_use.def_instruction_indices[ancestor_value_id] != (size_t)-1) {
                const ValueSsaInstruction *ancestor_instruction =
                    &function->blocks[def_use.def_block_ids[ancestor_value_id]]
                         .instructions[def_use.def_instruction_indices[ancestor_value_id]];
                if (function_result->spill_flags[ancestor_value_id] &&
                    function_result->spill_slots[ancestor_value_id] != (size_t)-1) {
                    value_ssa_def_use_analysis_free(&def_use);
                    return 1;
                }
                if (ancestor_instruction->kind != VALUE_SSA_INSTR_MOV ||
                    !ancestor_instruction->has_result ||
                    ancestor_instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    ancestor_instruction->as.mov_value.kind != VALUE_SSA_VALUE_ID ||
                    ancestor_instruction->as.mov_value.value_id >= ancestor_value_id) {
                    break;
                }
                ancestor_value_id = ancestor_instruction->as.mov_value.value_id;
            }
            if (ancestor_value_id < function_result->value_count &&
                function_result->spill_flags[ancestor_value_id] &&
                function_result->spill_slots[ancestor_value_id] != (size_t)-1) {
                value_ssa_def_use_analysis_free(&def_use);
                return 1;
            }
        }
        if (def_instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
            def_instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            def_instruction->as.load_slot.id >= spill_local_floor) {
            value_ssa_def_use_analysis_free(&def_use);
            return 1;
        }
    }

    for (use_index = def_use.use_offsets[value_id];
         use_index < def_use.use_offsets[value_id] + def_use.use_counts[value_id];
         ++use_index) {
        const ValueSsaUseSite *use = &def_use.use_sites[use_index];

        if (use->kind != VALUE_SSA_USE_INSTR) {
            continue;
        }

        if (use->block_id >= function->block_count ||
            use->instruction_index >= function->blocks[use->block_id].instruction_count) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-046: invalid rewritten-spill use site");
            return 0;
        }

        {
            const ValueSsaInstruction *use_instruction =
                &function->blocks[use->block_id].instructions[use->instruction_index];
            if (use_instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL &&
                use_instruction->as.store.slot.kind == VALUE_SSA_SLOT_LOCAL &&
                use_instruction->as.store.slot.id >= spill_local_floor) {
                value_ssa_def_use_analysis_free(&def_use);
                return 1;
            }
        }
    }

    *out_rewriteable = 1;
    value_ssa_def_use_analysis_free(&def_use);
    return 1;
}

static int value_ssa_alloc_rewrite_program_rewriteable_spills(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    const size_t *spill_local_floors,
    unsigned char *out_rewritten_functions,
    int *out_rewrote_any,
    ValueSsaError *error) {
    size_t function_index;
    int rewrote_any = 0;

    if (!program || !result || !spill_local_floors || !out_rewrote_any || result->function_count != program->function_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-045: invalid rewriteable-spill contract");
        return 0;
    }

    *out_rewrote_any = 0;
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        const ValueSsaAllocationResult *function_result = &result->function_results[function_index];
        size_t value_id;

        if (!function->has_body) {
            continue;
        }

        for (value_id = 0; value_id < function_result->value_count; ++value_id) {
            int rewriteable = 0;

            if (!value_ssa_alloc_result_value_is_rewriteable_spill(
                    function, function_result, spill_local_floors[function_index], value_id, &rewriteable, error)) {
                return 0;
            }
            if (!rewriteable) {
                continue;
            }
            if (!value_ssa_alloc_rewrite_spilled_value(
                    function, value_id, function_result->spill_slots[value_id], error)) {
                return 0;
            }
            if (out_rewritten_functions) {
                out_rewritten_functions[function_index] = 1u;
            }
            rewrote_any = 1;
        }
    }

    if (!rewrote_any) {
        *out_rewrote_any = 0;
        return value_ssa_verify_program(program, error);
    }

    *out_rewrote_any = 1;
    return value_ssa_verify_program(program, error);
}

static int value_ssa_alloc_run_program_rewrite_stage(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    const size_t *spill_local_floors,
    unsigned char *out_rewritten_functions,
    int *out_split_rewrote_any,
    int *out_spill_rewrote_any,
    int *out_rewrote_any,
    int *out_canonicalized_after_rewrite,
    ValueSsaError *error) {
    int split_rewrote_any = 0;
    int spill_rewrote_any = 0;
    int uses_indirect_memory = 0;
    size_t function_index;
    int trace_timing = value_ssa_alloc_trace_enabled();
    double total_start = trace_timing ? value_ssa_alloc_now_s() : 0.0;
    double stage_start = total_start;

    if (out_split_rewrote_any) {
        *out_split_rewrote_any = 0;
    }
    if (out_spill_rewrote_any) {
        *out_spill_rewrote_any = 0;
    }
    if (out_rewrote_any) {
        *out_rewrote_any = 0;
    }
    if (out_canonicalized_after_rewrite) {
        *out_canonicalized_after_rewrite = 0;
    }
    if (!program || !result || !spill_local_floors || !out_split_rewrote_any || !out_spill_rewrote_any ||
        !out_rewrote_any || !out_canonicalized_after_rewrite) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-215: invalid rewrite-stage contract");
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits_with_change_map(
            program, result, out_rewritten_functions, &split_rewrote_any, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_alloc_trace_timing("rewrite-stage block-local-split", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }
    if (!value_ssa_alloc_rewrite_program_rewriteable_spills(
            program, result, spill_local_floors, out_rewritten_functions, &spill_rewrote_any, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_alloc_trace_timing("rewrite-stage spill-rewrite", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    *out_split_rewrote_any = split_rewrote_any;
    *out_spill_rewrote_any = spill_rewrote_any;
    *out_rewrote_any = split_rewrote_any || spill_rewrote_any;
    if (!*out_rewrote_any) {
        if (trace_timing) {
            value_ssa_alloc_trace_timing("rewrite-stage total", value_ssa_alloc_now_s() - total_start);
        }
        return 1;
    }
    if (!spill_rewrote_any) {
        if (trace_timing) {
            value_ssa_alloc_trace_timing("rewrite-stage total", value_ssa_alloc_now_s() - total_start);
        }
        return 1;
    }

    for (function_index = 0; function_index < program->function_count && !uses_indirect_memory; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count && !uses_indirect_memory; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                switch (block->instructions[instruction_index].kind) {
                case VALUE_SSA_INSTR_ADDR_LOCAL:
                case VALUE_SSA_INSTR_ADDR_GLOBAL:
                case VALUE_SSA_INSTR_LOAD_INDIRECT:
                case VALUE_SSA_INSTR_STORE_INDIRECT:
                    uses_indirect_memory = 1;
                    break;
                default:
                    break;
                }
            }
        }
    }

    if (uses_indirect_memory) {
        if (trace_timing) {
            value_ssa_alloc_trace_timing("rewrite-stage indirect-memory-scan", value_ssa_alloc_now_s() - stage_start);
            value_ssa_alloc_trace_timing("rewrite-stage total", value_ssa_alloc_now_s() - total_start);
        }
        return 1;
    }

    if (!value_ssa_canonicalize_program(program, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_alloc_trace_timing("rewrite-stage post-rewrite-canonicalize", value_ssa_alloc_now_s() - stage_start);
        value_ssa_alloc_trace_timing("rewrite-stage total", value_ssa_alloc_now_s() - total_start);
    }
    *out_canonicalized_after_rewrite = 1;
    return 1;
}

static int value_ssa_alloc_rewrite_block_local_spill_split_for_value(ValueSsaFunction *function,
    const ValueSsaAllocationResult *function_result,
    size_t value_id,
    int *out_rewrote,
    ValueSsaError *error) {
    ValueSsaDefUseAnalysis def_use;
    size_t block_id;
    size_t def_block_id;
    size_t def_instruction_index;
    int rewrote_any = 0;

    if (out_rewrote) {
        *out_rewrote = 0;
    }
    if (!function || !function_result || !out_rewrote || value_id >= function_result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-211: invalid block-local spill split contract");
        return 0;
    }
    if (!function_result->spill_flags[value_id] || value_id >= function->next_value_id) {
        return 1;
    }

    value_ssa_def_use_analysis_init(&def_use);
    if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
        value_ssa_def_use_analysis_free(&def_use);
        return 0;
    }
    if (value_id >= def_use.value_count || !def_use.has_def[value_id] || def_use.def_block_ids[value_id] == (size_t)-1) {
        value_ssa_def_use_analysis_free(&def_use);
        return 1;
    }
    def_block_id = def_use.def_block_ids[value_id];
    def_instruction_index = def_use.def_instruction_indices[value_id];
    if (def_instruction_index != (size_t)-1) {
        const ValueSsaInstruction *def_instruction = &function->blocks[def_block_id].instructions[def_instruction_index];
        if (def_instruction->kind == VALUE_SSA_INSTR_MOV &&
            def_instruction->has_result &&
            def_instruction->result.kind == VALUE_SSA_VALUE_ID &&
            def_instruction->as.mov_value.kind == VALUE_SSA_VALUE_ID) {
            value_ssa_def_use_analysis_free(&def_use);
            return 1;
        }
    }

    for (block_id = 0; block_id < function->block_count; ++block_id) {
        size_t use_index;
        size_t first_use_index = (size_t)-1;
        size_t use_count_in_block = 0;
        size_t split_value_id;
        ValueSsaInstruction split_instruction;
        ValueSsaBasicBlock *block;
        size_t inserted_instruction_index;

        for (use_index = def_use.use_offsets[value_id];
             use_index < def_use.use_offsets[value_id] + def_use.use_counts[value_id];
             ++use_index) {
            const ValueSsaUseSite *use = &def_use.use_sites[use_index];
            size_t use_position;

            if (use->block_id != block_id) {
                continue;
            }
            if (use->kind != VALUE_SSA_USE_INSTR && use->kind != VALUE_SSA_USE_TERM) {
                continue;
            }

            use_position = use->kind == VALUE_SSA_USE_INSTR
                ? use->instruction_index
                : function->blocks[block_id].instruction_count;
            if (block_id == def_block_id &&
                def_instruction_index != (size_t)-1 &&
                use_position <= def_instruction_index) {
                continue;
            }
            if (first_use_index == (size_t)-1 || use_position < first_use_index) {
                first_use_index = use_position;
            }
            ++use_count_in_block;
        }
        if (use_count_in_block < 2 || first_use_index == (size_t)-1) {
            continue;
        }

        split_value_id = value_ssa_function_allocate_value(function);
        if (split_value_id == (size_t)-1) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-212: out of value ids for block-local spill split");
            return 0;
        }

        memset(&split_instruction, 0, sizeof(split_instruction));
        split_instruction.kind = VALUE_SSA_INSTR_MOV;
        split_instruction.has_result = 1;
        split_instruction.result = value_ssa_value_id(split_value_id);
        split_instruction.as.mov_value = value_ssa_value_id(value_id);

        block = &function->blocks[block_id];
        inserted_instruction_index = first_use_index;
        if (!value_ssa_alloc_insert_instruction(block, inserted_instruction_index, &split_instruction, error)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }

        for (use_index = def_use.use_offsets[value_id];
             use_index < def_use.use_offsets[value_id] + def_use.use_counts[value_id];
             ++use_index) {
            const ValueSsaUseSite *use = &def_use.use_sites[use_index];
            size_t rewrite_instruction_index;
            size_t use_position;

            if (use->block_id != block_id) {
                continue;
            }
            if (use->kind != VALUE_SSA_USE_INSTR && use->kind != VALUE_SSA_USE_TERM) {
                continue;
            }
            use_position = use->kind == VALUE_SSA_USE_INSTR
                ? use->instruction_index
                : block->instruction_count;
            if (block_id == def_block_id &&
                def_instruction_index != (size_t)-1 &&
                use_position <= def_instruction_index) {
                continue;
            }
            if (use->kind == VALUE_SSA_USE_INSTR) {
                rewrite_instruction_index = use->instruction_index >= inserted_instruction_index
                    ? use->instruction_index + 1
                    : use->instruction_index;
                if (rewrite_instruction_index >= block->instruction_count) {
                    value_ssa_def_use_analysis_free(&def_use);
                    value_ssa_alloc_set_error(
                        error, 0, 0, "VALUE-SSA-ALLOC-213: invalid rewritten block-local split use");
                    return 0;
                }
                if (!value_ssa_alloc_rewrite_value_use(
                        &block->instructions[rewrite_instruction_index],
                        use->role,
                        use->operand_index,
                        value_ssa_value_id(split_value_id),
                        error)) {
                    value_ssa_def_use_analysis_free(&def_use);
                    return 0;
                }
                continue;
            }
            if (use->kind == VALUE_SSA_USE_TERM) {
                if (!value_ssa_alloc_rewrite_terminator_use(
                        block, use->role, value_ssa_value_id(split_value_id), error)) {
                    value_ssa_def_use_analysis_free(&def_use);
                    return 0;
                }
                continue;
            }
        }

        rewrote_any = 1;
    }

    *out_rewrote = rewrote_any;
    value_ssa_def_use_analysis_free(&def_use);
    return 1;
}

static int value_ssa_alloc_count_spilled_values_on_phi_edge(const ValueSsaFunction *function,
    const ValueSsaAllocationResult *function_result,
    size_t predecessor_block_id,
    size_t successor_block_id,
    size_t *out_spilled_value_count,
    ValueSsaError *error) {
    unsigned char *seen_values = NULL;
    const ValueSsaBasicBlock *successor_block;
    size_t phi_index;
    size_t count = 0;
    int ok = 0;

    if (out_spilled_value_count) {
        *out_spilled_value_count = 0;
    }
    if (!function || !function_result || !out_spilled_value_count ||
        predecessor_block_id >= function->block_count || successor_block_id >= function->block_count ||
        !function_result->spill_flags) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-220: invalid phi-edge spill-family count query");
        return 0;
    }

    successor_block = &function->blocks[successor_block_id];
    seen_values = (unsigned char *)calloc(function_result->value_count ? function_result->value_count : 1, sizeof(unsigned char));
    if (!seen_values) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-221: out of memory counting phi-edge spilled values");
        return 0;
    }

    for (phi_index = 0; phi_index < successor_block->phi_count; ++phi_index) {
        const ValueSsaPhi *phi = &successor_block->phis[phi_index];
        size_t input_index;

        for (input_index = 0; input_index < phi->input_count; ++input_index) {
            const ValueSsaPhiInput *input = &phi->inputs[input_index];
            size_t input_value_id;

            if (input->predecessor_block_id != predecessor_block_id || input->value.kind != VALUE_SSA_VALUE_ID) {
                continue;
            }
            input_value_id = input->value.value_id;
            if (input_value_id >= function_result->value_count || !function_result->spill_flags[input_value_id] ||
                seen_values[input_value_id]) {
                continue;
            }
            seen_values[input_value_id] = 1u;
            ++count;
        }
    }

    *out_spilled_value_count = count;
    ok = 1;

    free(seen_values);
    return ok;
}

static int value_ssa_alloc_rewrite_edge_phi_spill_split_for_value(ValueSsaFunction *function,
    const ValueSsaAllocationResult *function_result,
    size_t value_id,
    int *out_rewrote,
    ValueSsaError *error) {
    ValueSsaDefUseAnalysis def_use;
    size_t use_index_begin;
    size_t use_index_end;
    size_t use_index;
    int rewrote_any = 0;

    if (out_rewrote) {
        *out_rewrote = 0;
    }
    if (!function || !function_result || !out_rewrote || value_id >= function_result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-215: invalid edge-shaped spill split contract");
        return 0;
    }
    if (!function_result->spill_flags[value_id] || value_id >= function->next_value_id) {
        return 1;
    }

    value_ssa_def_use_analysis_init(&def_use);
    if (!value_ssa_compute_def_use_analysis(function, &def_use, error)) {
        value_ssa_def_use_analysis_free(&def_use);
        return 0;
    }
    if (value_id >= def_use.value_count) {
        value_ssa_def_use_analysis_free(&def_use);
        return 1;
    }

    use_index_begin = def_use.use_offsets[value_id];
    use_index_end = use_index_begin + def_use.use_counts[value_id];
    for (use_index = use_index_begin; use_index < use_index_end; ++use_index) {
        const ValueSsaUseSite *seed_use = &def_use.use_sites[use_index];
        size_t successor_block_id;
        size_t predecessor_block_id;
        size_t peer_index;
        size_t phi_use_count_on_edge = 0;
        size_t spilled_value_count_on_edge = 0;
        size_t split_value_id = (size_t)-1;
        ValueSsaInstruction split_instruction;
        size_t existing_split_block_id = (size_t)-1;
        int predecessor_is_split_child_edge = 0;
        int predecessor_tail_use_found = 0;

        if (seed_use->kind != VALUE_SSA_USE_PHI || seed_use->role != VALUE_SSA_USE_ROLE_PHI_INPUT) {
            continue;
        }
        if (seed_use->block_id >= function->block_count ||
            seed_use->phi_index >= function->blocks[seed_use->block_id].phi_count ||
            seed_use->operand_index >= function->blocks[seed_use->block_id].phis[seed_use->phi_index].input_count) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-216: invalid edge-shaped spill split phi use");
            return 0;
        }

        successor_block_id = seed_use->block_id;
        predecessor_block_id =
            function->blocks[successor_block_id].phis[seed_use->phi_index].inputs[seed_use->operand_index].predecessor_block_id;
        if (predecessor_block_id >= function->block_count) {
            value_ssa_def_use_analysis_free(&def_use);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-217: invalid edge-shaped spill split predecessor");
            return 0;
        }
        predecessor_is_split_child_edge = value_ssa_alloc_block_is_reusable_split_child_edge(
            &function->blocks[predecessor_block_id], successor_block_id);
        if (predecessor_is_split_child_edge) {
            const ValueSsaPhiInput *current_input =
                &function->blocks[successor_block_id].phis[seed_use->phi_index].inputs[seed_use->operand_index];

            if (current_input->value.kind != VALUE_SSA_VALUE_ID || current_input->value.value_id != value_id) {
                continue;
            }
        }
        if (!value_ssa_alloc_count_spilled_values_on_phi_edge(function,
                function_result,
                predecessor_block_id,
                successor_block_id,
                &spilled_value_count_on_edge,
                error)) {
            value_ssa_def_use_analysis_free(&def_use);
            return 0;
        }

        for (peer_index = use_index_begin; peer_index < use_index_end; ++peer_index) {
            const ValueSsaUseSite *peer_use = &def_use.use_sites[peer_index];
            size_t peer_predecessor_block_id;

            if (peer_use->kind != VALUE_SSA_USE_PHI || peer_use->role != VALUE_SSA_USE_ROLE_PHI_INPUT ||
                peer_use->block_id != successor_block_id) {
                continue;
            }
            if (peer_use->phi_index >= function->blocks[successor_block_id].phi_count ||
                peer_use->operand_index >= function->blocks[successor_block_id].phis[peer_use->phi_index].input_count) {
                value_ssa_def_use_analysis_free(&def_use);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-218: invalid peer edge-shaped spill split phi use");
                return 0;
            }
            peer_predecessor_block_id = function->blocks[successor_block_id]
                                            .phis[peer_use->phi_index]
                                            .inputs[peer_use->operand_index]
                                            .predecessor_block_id;
            if (peer_predecessor_block_id == predecessor_block_id) {
                ++phi_use_count_on_edge;
            }
        }

        for (peer_index = use_index_begin; peer_index < use_index_end; ++peer_index) {
            const ValueSsaUseSite *peer_use = &def_use.use_sites[peer_index];

            if (peer_use->block_id != predecessor_block_id) {
                continue;
            }
            if (peer_use->kind == VALUE_SSA_USE_INSTR || peer_use->kind == VALUE_SSA_USE_TERM) {
                predecessor_tail_use_found = 1;
                break;
            }
        }

        if (function->blocks[predecessor_block_id].has_terminator &&
            function->blocks[predecessor_block_id].terminator.kind == VALUE_SSA_TERM_JUMP &&
            function->blocks[predecessor_block_id].terminator.as.jump_target == successor_block_id) {
            ValueSsaBasicBlock *predecessor_block = &function->blocks[predecessor_block_id];
            size_t local_split_value_id = (size_t)-1;
            int rewrote_local_predecessor_uses = 0;

            if (phi_use_count_on_edge < 2 &&
                spilled_value_count_on_edge < 2 &&
                !predecessor_tail_use_found &&
                !(phi_use_count_on_edge >= 1 && predecessor_is_split_child_edge)) {
                continue;
            }

            if (predecessor_tail_use_found) {
                if (!value_ssa_alloc_insert_local_split_child_for_block_uses(function,
                        &def_use,
                        value_id,
                        predecessor_block_id,
                        def_use.def_block_ids[value_id],
                        def_use.def_instruction_indices[value_id],
                        &local_split_value_id,
                        &rewrote_local_predecessor_uses,
                        error)) {
                    value_ssa_def_use_analysis_free(&def_use);
                    return 0;
                }
                predecessor_block = &function->blocks[predecessor_block_id];
            }

            if (rewrote_local_predecessor_uses) {
                split_value_id = local_split_value_id;
            } else if (!value_ssa_alloc_find_split_child_mov_in_block(predecessor_block, value_id, &split_value_id)) {
                size_t upstream_predecessor_block_id = (size_t)-1;

                if (predecessor_is_split_child_edge &&
                    value_ssa_alloc_find_unique_predecessor_block(
                        function, predecessor_block_id, &upstream_predecessor_block_id) &&
                    upstream_predecessor_block_id < function->block_count &&
                    value_ssa_alloc_find_split_child_mov_in_block(
                        &function->blocks[upstream_predecessor_block_id], value_id, &split_value_id)) {
                    /* Reuse the already-available local split child from the unique upstream predecessor. */
                } else {
                split_value_id = value_ssa_function_allocate_value(function);
                if (split_value_id == (size_t)-1) {
                    value_ssa_def_use_analysis_free(&def_use);
                    value_ssa_alloc_set_error(
                        error, 0, 0, "VALUE-SSA-ALLOC-219: out of value ids for edge-shaped spill split");
                    return 0;
                }

                memset(&split_instruction, 0, sizeof(split_instruction));
                split_instruction.kind = VALUE_SSA_INSTR_MOV;
                split_instruction.has_result = 1;
                split_instruction.result = value_ssa_value_id(split_value_id);
                split_instruction.as.mov_value = value_ssa_value_id(value_id);

                if (!value_ssa_alloc_insert_instruction(
                        predecessor_block, predecessor_block->instruction_count, &split_instruction, error)) {
                    value_ssa_def_use_analysis_free(&def_use);
                    return 0;
                }
                }
            }
            for (peer_index = use_index_begin; peer_index < use_index_end; ++peer_index) {
                const ValueSsaUseSite *peer_use = &def_use.use_sites[peer_index];
                size_t peer_predecessor_block_id;

                if (peer_use->kind != VALUE_SSA_USE_PHI || peer_use->role != VALUE_SSA_USE_ROLE_PHI_INPUT ||
                    peer_use->block_id != successor_block_id) {
                    continue;
                }
                peer_predecessor_block_id = function->blocks[successor_block_id]
                                                .phis[peer_use->phi_index]
                                                .inputs[peer_use->operand_index]
                                                .predecessor_block_id;
                if (peer_predecessor_block_id != predecessor_block_id) {
                    continue;
                }
                if (!value_ssa_alloc_rewrite_phi_input(
                        &function->blocks[successor_block_id],
                        peer_use->phi_index,
                        peer_use->operand_index,
                        value_ssa_value_id(split_value_id),
                        error)) {
                    value_ssa_def_use_analysis_free(&def_use);
                    return 0;
                }
            }
            rewrote_any = rewrote_any || rewrote_local_predecessor_uses || 1;
            continue;
        }

        if (function->blocks[predecessor_block_id].has_terminator &&
            function->blocks[predecessor_block_id].terminator.kind == VALUE_SSA_TERM_BRANCH &&
            function->blocks[predecessor_block_id].terminator.as.branch.then_target !=
                function->blocks[predecessor_block_id].terminator.as.branch.else_target) {
            ValueSsaBasicBlock *predecessor_block = &function->blocks[predecessor_block_id];
            ValueSsaBasicBlock *successor_block = &function->blocks[successor_block_id];
            ValueSsaBasicBlock *split_block = NULL;
            size_t split_block_id;
            size_t predecessor_phi_index;
            size_t existing_instruction_index;
            int has_existing_split_child_edge = 0;
            int branch_targets_successor = 0;
            size_t rewrite_from_predecessor_block_id;

            predecessor_block = &function->blocks[predecessor_block_id];
            branch_targets_successor =
                predecessor_block->terminator.as.branch.then_target == successor_block_id ||
                predecessor_block->terminator.as.branch.else_target == successor_block_id;
            has_existing_split_child_edge = value_ssa_alloc_find_existing_split_child_edge(
                function, predecessor_block, successor_block_id, &existing_split_block_id);
            if (!branch_targets_successor && !has_existing_split_child_edge) {
                continue;
            }
            if (phi_use_count_on_edge < 2 &&
                spilled_value_count_on_edge < 2 &&
                !predecessor_tail_use_found &&
                !has_existing_split_child_edge) {
                continue;
            }
            if (has_existing_split_child_edge) {
                split_block_id = existing_split_block_id;
                successor_block = &function->blocks[successor_block_id];
                split_block = &function->blocks[split_block_id];
                rewrite_from_predecessor_block_id = split_block_id;

                for (existing_instruction_index = 0; existing_instruction_index < split_block->instruction_count;
                     ++existing_instruction_index) {
                    const ValueSsaInstruction *instruction = &split_block->instructions[existing_instruction_index];

                    if (!instruction->has_result || instruction->kind != VALUE_SSA_INSTR_MOV) {
                        continue;
                    }
                    if (instruction->as.mov_value.kind == VALUE_SSA_VALUE_ID &&
                        instruction->as.mov_value.value_id == value_id) {
                        split_value_id = instruction->result.value_id;
                        break;
                    }
                }

                if (existing_instruction_index == split_block->instruction_count) {
                    split_value_id = value_ssa_function_allocate_value(function);
                    if (split_value_id == (size_t)-1) {
                        value_ssa_def_use_analysis_free(&def_use);
                        value_ssa_alloc_set_error(
                            error, 0, 0, "VALUE-SSA-ALLOC-219: out of value ids for edge-shaped spill split");
                        return 0;
                    }
                    memset(&split_instruction, 0, sizeof(split_instruction));
                    split_instruction.kind = VALUE_SSA_INSTR_MOV;
                    split_instruction.has_result = 1;
                    split_instruction.result = value_ssa_value_id(split_value_id);
                    split_instruction.as.mov_value = value_ssa_value_id(value_id);
                    if (!value_ssa_alloc_insert_instruction(
                            split_block, split_block->instruction_count, &split_instruction, error)) {
                        value_ssa_def_use_analysis_free(&def_use);
                        return 0;
                    }
                }
            } else {
                if (!value_ssa_function_append_block(function, NULL, &split_block, error)) {
                    value_ssa_def_use_analysis_free(&def_use);
                    return 0;
                }
                split_block_id = function->block_count - 1;
                predecessor_block = &function->blocks[predecessor_block_id];
                successor_block = &function->blocks[successor_block_id];
                split_block = &function->blocks[split_block_id];
                rewrite_from_predecessor_block_id = predecessor_block_id;

                if (predecessor_block->terminator.as.branch.then_target == successor_block_id) {
                    predecessor_block->terminator.as.branch.then_target = split_block_id;
                } else {
                    predecessor_block->terminator.as.branch.else_target = split_block_id;
                }

                split_value_id = value_ssa_function_allocate_value(function);
                if (split_value_id == (size_t)-1) {
                    value_ssa_def_use_analysis_free(&def_use);
                    value_ssa_alloc_set_error(
                        error, 0, 0, "VALUE-SSA-ALLOC-219: out of value ids for edge-shaped spill split");
                    return 0;
                }
                memset(&split_instruction, 0, sizeof(split_instruction));
                split_instruction.kind = VALUE_SSA_INSTR_MOV;
                split_instruction.has_result = 1;
                split_instruction.result = value_ssa_value_id(split_value_id);
                split_instruction.as.mov_value = value_ssa_value_id(value_id);
                if (!value_ssa_alloc_insert_instruction(split_block, 0, &split_instruction, error) ||
                    !value_ssa_block_set_jump(split_block, successor_block_id, error)) {
                    value_ssa_def_use_analysis_free(&def_use);
                    return 0;
                }
            }

            for (predecessor_phi_index = 0; predecessor_phi_index < successor_block->phi_count; ++predecessor_phi_index) {
                ValueSsaPhi *phi = &successor_block->phis[predecessor_phi_index];
                size_t input_index;

                for (input_index = 0; input_index < phi->input_count; ++input_index) {
                    if (phi->inputs[input_index].predecessor_block_id != rewrite_from_predecessor_block_id) {
                        continue;
                    }
                    phi->inputs[input_index].predecessor_block_id = split_block_id;
                    if (phi->inputs[input_index].value.kind == VALUE_SSA_VALUE_ID &&
                        phi->inputs[input_index].value.value_id == value_id) {
                        phi->inputs[input_index].value = value_ssa_value_id(split_value_id);
                    }
                }
            }

            rewrote_any = 1;
            continue;
        }
    }

    *out_rewrote = rewrote_any;
    value_ssa_def_use_analysis_free(&def_use);
    return 1;
}

static int value_ssa_rewrite_program_block_local_spill_splits_with_change_map(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    unsigned char *out_rewritten_functions,
    int *out_rewrote_any,
    ValueSsaError *error) {
    size_t function_index;
    int rewrote_any = 0;

    if (out_rewrote_any) {
        *out_rewrote_any = 0;
    }
    if (!program || !result || result->function_count != program->function_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-214: invalid block-local spill split contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        const ValueSsaAllocationResult *function_result = &result->function_results[function_index];
        size_t value_id;
        int function_rewrote_any = 0;

        if (!function->has_body) {
            continue;
        }
        for (value_id = 0; value_id < function_result->value_count; ++value_id) {
            int rewrote = 0;
            int rewrote_local = 0;
            int rewrote_edge = 0;

            if (!value_ssa_alloc_rewrite_block_local_spill_split_for_value(
                    function, function_result, value_id, &rewrote_local, error)) {
                return 0;
            }
            if (!value_ssa_alloc_rewrite_edge_phi_spill_split_for_value(
                    function, function_result, value_id, &rewrote_edge, error)) {
                return 0;
            }
            rewrote = rewrote_local || rewrote_edge;
            function_rewrote_any = function_rewrote_any || rewrote;
            rewrote_any = rewrote_any || rewrote;
        }
        if (out_rewritten_functions && function_rewrote_any) {
            out_rewritten_functions[function_index] = 1u;
        }
    }

    if (out_rewrote_any) {
        *out_rewrote_any = rewrote_any;
    }
    return value_ssa_verify_program(program, error);
}

int value_ssa_rewrite_program_block_local_spill_splits_with_change_flag(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    int *out_rewrote_any,
    ValueSsaError *error) {
    return value_ssa_rewrite_program_block_local_spill_splits_with_change_map(
        program, result, NULL, out_rewrote_any, error);
}

int value_ssa_rewrite_program_block_local_spill_splits(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    ValueSsaError *error) {
    return value_ssa_rewrite_program_block_local_spill_splits_with_change_flag(program, result, NULL, error);
}

int value_ssa_rewrite_program_block_local_spill_splits_and_canonicalize(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    ValueSsaError *error) {
    if (!value_ssa_rewrite_program_block_local_spill_splits(program, result, error)) {
        return 0;
    }

    return value_ssa_canonicalize_program(program, error);
}

int value_ssa_rewrite_program_single_block_spills(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    ValueSsaError *error) {
    size_t function_index;

    if (!program || !result || result->function_count != program->function_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-032: invalid spill rewrite contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        const ValueSsaAllocationResult *function_result = &result->function_results[function_index];
        size_t spill_local_floor = function->local_count;
        size_t value_id;

        if (!function->has_body) {
            continue;
        }

        for (value_id = 0; value_id < function_result->value_count; ++value_id) {
            int rewriteable = 0;

            if (!value_ssa_alloc_result_value_is_rewriteable_spill(
                    function, function_result, spill_local_floor, value_id, &rewriteable, error)) {
                return 0;
            }
            if (!rewriteable) {
                continue;
            }
            if (!value_ssa_alloc_rewrite_spilled_value(
                    function, value_id, function_result->spill_slots[value_id], error)) {
                return 0;
            }
        }
    }

    return value_ssa_verify_program(program, error);
}

int value_ssa_rewrite_program_single_block_spills_and_canonicalize(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    ValueSsaError *error) {
    if (!value_ssa_rewrite_program_single_block_spills(program, result, error)) {
        return 0;
    }

    return value_ssa_canonicalize_program(program, error);
}

#endif
