#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

static MachineIrOperand machine_ir_resolve_local_copy_alias(size_t resource_count,
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    const unsigned char *alias_valid,
    const MachineIrOperand *alias_values,
    MachineIrOperand operand);

static void machine_ir_invalidate_local_copy_aliases(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t resource_count,
    unsigned char *alias_valid,
    const MachineIrOperand *alias_values,
    MachineIrOperand killed_operand);

static void machine_ir_invalidate_caller_clobbered_aliases(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t resource_count,
    unsigned char *alias_valid,
    const MachineIrOperand *alias_values);

static int machine_ir_compute_predecessor_count_map(const MachineIrFunction *function,
    size_t **out_predecessor_counts,
    MachineIrError *error) {
    size_t *predecessor_counts = NULL;
    size_t block_index;

    if (out_predecessor_counts) {
        *out_predecessor_counts = NULL;
    }
    if (!function || !out_predecessor_counts) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-541: invalid predecessor-count map contract");
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }

    predecessor_counts = (size_t *)calloc(function->block_count, sizeof(size_t));
    if (!predecessor_counts) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-542: out of memory building predecessor-count map");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineIrBasicBlock *block = &function->blocks[block_index];

        if (!block->has_terminator) {
            continue;
        }
        switch (block->terminator.kind) {
            case MACHINE_IR_TERM_JUMP:
                if (block->terminator.as.jump_target < function->block_count) {
                    ++predecessor_counts[block->terminator.as.jump_target];
                }
                break;
            case MACHINE_IR_TERM_BRANCH: {
                if (block->terminator.as.branch.then_target < function->block_count) {
                    ++predecessor_counts[block->terminator.as.branch.then_target];
                }
                if (block->terminator.as.branch.else_target < function->block_count &&
                    block->terminator.as.branch.else_target != block->terminator.as.branch.then_target) {
                    ++predecessor_counts[block->terminator.as.branch.else_target];
                }
                break;
            }
            default:
                break;
        }
    }

    *out_predecessor_counts = predecessor_counts;
    return 1;
}

static int machine_ir_block_reaches_target(const MachineIrBasicBlock *block,
    size_t target_block_id) {
    if (!block || !block->has_terminator) {
        return 0;
    }
    switch (block->terminator.kind) {
        case MACHINE_IR_TERM_JUMP:
            return block->terminator.as.jump_target == target_block_id;
        case MACHINE_IR_TERM_BRANCH:
            return block->terminator.as.branch.then_target == target_block_id ||
                block->terminator.as.branch.else_target == target_block_id;
        default:
            return 0;
    }
}

static void machine_ir_compute_block_entry_aliases(const MachineIrFunction *function,
    size_t block_id,
    size_t resource_count,
    const size_t *predecessor_counts,
    const unsigned char *block_exit_alias_valid,
    const MachineIrOperand *block_exit_alias_values,
    unsigned char *out_alias_valid,
    MachineIrOperand *out_alias_values) {
    size_t resource_index;
    size_t predecessor_block_id;

    if (!function || !predecessor_counts || !block_exit_alias_valid || !block_exit_alias_values ||
        !out_alias_valid || !out_alias_values || block_id >= function->block_count) {
        return;
    }

    memset(out_alias_valid, 0, resource_count * sizeof(unsigned char));
    memset(out_alias_values, 0, resource_count * sizeof(MachineIrOperand));
    if (block_id == 0 || predecessor_counts[block_id] == 0) {
        return;
    }

    for (resource_index = 0; resource_index < resource_count; ++resource_index) {
        int have_candidate = 0;
        MachineIrOperand candidate = machine_ir_operand_none();
        int all_match = 1;

        for (predecessor_block_id = 0; predecessor_block_id < function->block_count; ++predecessor_block_id) {
            const MachineIrBasicBlock *predecessor = &function->blocks[predecessor_block_id];
            const unsigned char *pred_valid = block_exit_alias_valid + (predecessor_block_id * resource_count);
            const MachineIrOperand *pred_values = block_exit_alias_values + (predecessor_block_id * resource_count);

            if (!machine_ir_block_reaches_target(predecessor, block_id)) {
                continue;
            }
            if (!pred_valid[resource_index]) {
                all_match = 0;
                break;
            }
            if (!have_candidate) {
                candidate = pred_values[resource_index];
                have_candidate = 1;
                continue;
            }
            if (!machine_ir_operands_equal(candidate, pred_values[resource_index])) {
                all_match = 0;
                break;
            }
        }

        if (have_candidate && all_match) {
            out_alias_valid[resource_index] = 1u;
            out_alias_values[resource_index] = candidate;
        }
    }
}

static void machine_ir_invalidate_caller_clobbered_aliases(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t resource_count,
    unsigned char *alias_valid,
    const MachineIrOperand *alias_values) {
    size_t register_index;

    if (!program || !function || !alias_valid || !alias_values) {
        return;
    }

    for (register_index = 0; register_index < program->register_bank.register_count; ++register_index) {
        if (!program->register_bank.registers[register_index].caller_clobbered) {
            continue;
        }
        machine_ir_invalidate_local_copy_aliases(
            program,
            function,
            resource_count,
            alias_valid,
            alias_values,
            machine_ir_operand_register(register_index));
    }
}

static int machine_ir_instruction_is_removable_dead_def(const MachineIrInstruction *instruction) {
    if (!instruction || !instruction->has_result) {
        return 0;
    }

    switch (instruction->kind) {
        case MACHINE_IR_INSTR_MOV:
        case MACHINE_IR_INSTR_LOAD_LOCAL:
        case MACHINE_IR_INSTR_LOAD_GLOBAL:
            return 1;
        case MACHINE_IR_INSTR_BINARY:
            switch (instruction->as.binary.op) {
                case MACHINE_IR_BINARY_DIV:
                case MACHINE_IR_BINARY_MOD:
                case MACHINE_IR_BINARY_SHIFT_LEFT:
                case MACHINE_IR_BINARY_SHIFT_RIGHT:
                    return 0;
                default:
                    return 1;
            }
        default:
            return 0;
    }
}

static int machine_ir_alias_states_equal(size_t resource_count,
    const unsigned char *lhs_valid,
    const MachineIrOperand *lhs_values,
    const unsigned char *rhs_valid,
    const MachineIrOperand *rhs_values) {
    size_t resource_index;

    for (resource_index = 0; resource_index < resource_count; ++resource_index) {
        if (lhs_valid[resource_index] != rhs_valid[resource_index]) {
            return 0;
        }
        if (lhs_valid[resource_index] &&
            !machine_ir_operands_equal(lhs_values[resource_index], rhs_values[resource_index])) {
            return 0;
        }
    }
    return 1;
}

static int machine_ir_compute_block_exit_aliases(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t resource_count,
    const size_t *predecessor_counts,
    unsigned char *block_exit_alias_valid,
    MachineIrOperand *block_exit_alias_values,
    MachineIrError *error) {
    unsigned char *entry_alias_valid = NULL;
    MachineIrOperand *entry_alias_values = NULL;
    unsigned char *local_alias_valid = NULL;
    MachineIrOperand *local_alias_values = NULL;
    int changed = 1;
    size_t block_index;

    if (!program || !function || !predecessor_counts || !block_exit_alias_valid || !block_exit_alias_values) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-543: invalid block-exit alias computation contract");
        return 0;
    }

    entry_alias_valid = (unsigned char *)calloc(resource_count, sizeof(unsigned char));
    entry_alias_values = (MachineIrOperand *)calloc(resource_count, sizeof(MachineIrOperand));
    local_alias_valid = (unsigned char *)calloc(resource_count, sizeof(unsigned char));
    local_alias_values = (MachineIrOperand *)calloc(resource_count, sizeof(MachineIrOperand));
    if (!entry_alias_valid || !entry_alias_values || !local_alias_valid || !local_alias_values) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-544: out of memory computing block-exit aliases");
        free(local_alias_values);
        free(local_alias_valid);
        free(entry_alias_values);
        free(entry_alias_valid);
        return 0;
    }

    while (changed) {
        changed = 0;
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineIrBasicBlock *block = &function->blocks[block_index];
            unsigned char *exit_valid = block_exit_alias_valid + (block_index * resource_count);
            MachineIrOperand *exit_values = block_exit_alias_values + (block_index * resource_count);
            size_t instruction_index;
            size_t result_index;

            machine_ir_compute_block_entry_aliases(
                function,
                block_index,
                resource_count,
                predecessor_counts,
                block_exit_alias_valid,
                block_exit_alias_values,
                entry_alias_valid,
                entry_alias_values);

            memcpy(local_alias_valid, entry_alias_valid, resource_count * sizeof(unsigned char));
            memcpy(local_alias_values, entry_alias_values, resource_count * sizeof(MachineIrOperand));

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const MachineIrInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->has_result &&
                    machine_ir_operand_get_resource_index(program, function, instruction->result, &result_index)) {
                    machine_ir_invalidate_local_copy_aliases(
                        program, function, resource_count, local_alias_valid, local_alias_values, instruction->result);
                }

                if (instruction->kind == MACHINE_IR_INSTR_MOV &&
                    instruction->has_result &&
                    machine_ir_operand_get_resource_index(program, function, instruction->result, &result_index) &&
                    !machine_ir_operands_equal(instruction->result, instruction->as.mov_value)) {
                    local_alias_valid[result_index] = 1u;
                    local_alias_values[result_index] =
                        machine_ir_resolve_local_copy_alias(resource_count,
                            program,
                            function,
                            local_alias_valid,
                            local_alias_values,
                            instruction->as.mov_value);
                }
                if (instruction->kind == MACHINE_IR_INSTR_CALL) {
                    machine_ir_invalidate_caller_clobbered_aliases(
                        program,
                        function,
                        resource_count,
                        local_alias_valid,
                        local_alias_values);
                }
            }

            if (!machine_ir_alias_states_equal(
                    resource_count,
                    exit_valid,
                    exit_values,
                    local_alias_valid,
                    local_alias_values)) {
                memcpy(exit_valid, local_alias_valid, resource_count * sizeof(unsigned char));
                memcpy(exit_values, local_alias_values, resource_count * sizeof(MachineIrOperand));
                changed = 1;
            }
        }
    }

    free(local_alias_values);
    free(local_alias_valid);
    free(entry_alias_values);
    free(entry_alias_valid);
    return 1;
}

static void machine_ir_mark_operand_resource(const MachineIrProgram *program,
    const MachineIrFunction *function,
    unsigned char *set,
    MachineIrOperand operand) {
    size_t resource_index;

    if (!set) {
        return;
    }
    if (machine_ir_operand_get_resource_index(program, function, operand, &resource_index)) {
        set[resource_index] = 1u;
    }
}

static MachineIrOperand machine_ir_resolve_local_copy_alias(size_t resource_count,
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    const unsigned char *alias_valid,
    const MachineIrOperand *alias_values,
    MachineIrOperand operand) {
    size_t hop_count = 0;

    while (hop_count < resource_count) {
        size_t resource_index = machine_ir_operand_resource_slot(program, function, operand);

        if (resource_index == (size_t)-1 ||
            !alias_valid ||
            !alias_values ||
            resource_index >= resource_count ||
            !alias_valid[resource_index]) {
            return operand;
        }
        operand = alias_values[resource_index];
        ++hop_count;
    }

    return operand;
}

static void machine_ir_invalidate_local_copy_aliases(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t resource_count,
    unsigned char *alias_valid,
    const MachineIrOperand *alias_values,
    MachineIrOperand killed_operand) {
    size_t killed_index;
    size_t resource_index;

    if (!alias_valid || !alias_values) {
        return;
    }
    if (!machine_ir_operand_get_resource_index(program, function, killed_operand, &killed_index)) {
        return;
    }

    if (killed_index < resource_count) {
        alias_valid[killed_index] = 0;
    }
    for (resource_index = 0; resource_index < resource_count; ++resource_index) {
        if (!alias_valid[resource_index]) {
            continue;
        }
        if (machine_ir_operands_equal(alias_values[resource_index], killed_operand)) {
            alias_valid[resource_index] = 0;
        }
    }
}

static void machine_ir_rewrite_instruction_operands_with_local_aliases(size_t resource_count,
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    const unsigned char *alias_valid,
    const MachineIrOperand *alias_values,
    MachineIrInstruction *instruction,
    int *out_changed) {
    MachineIrOperand before;
    MachineIrOperand after;
    size_t arg_index;

    if (!instruction) {
        return;
    }

    switch (instruction->kind) {
        case MACHINE_IR_INSTR_MOV:
            before = instruction->as.mov_value;
            after = machine_ir_resolve_local_copy_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_ir_operands_equal(before, after)) {
                instruction->as.mov_value = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        case MACHINE_IR_INSTR_BINARY:
            before = instruction->as.binary.lhs;
            after = machine_ir_resolve_local_copy_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_ir_operands_equal(before, after)) {
                instruction->as.binary.lhs = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            before = instruction->as.binary.rhs;
            after = machine_ir_resolve_local_copy_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_ir_operands_equal(before, after)) {
                instruction->as.binary.rhs = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        case MACHINE_IR_INSTR_CALL:
            for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                before = instruction->as.call.args[arg_index];
                after = machine_ir_resolve_local_copy_alias(
                    resource_count, program, function, alias_valid, alias_values, before);
                if (!machine_ir_operands_equal(before, after)) {
                    instruction->as.call.args[arg_index] = after;
                    if (out_changed) {
                        *out_changed = 1;
                    }
                }
            }
            break;
        case MACHINE_IR_INSTR_STORE_LOCAL:
        case MACHINE_IR_INSTR_STORE_GLOBAL:
            before = instruction->as.store.value;
            after = machine_ir_resolve_local_copy_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_ir_operands_equal(before, after)) {
                instruction->as.store.value = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        default:
            break;
    }
}

static void machine_ir_rewrite_terminator_operands_with_local_aliases(size_t resource_count,
    const MachineIrProgram *program,
    const MachineIrFunction *function,
    const unsigned char *alias_valid,
    const MachineIrOperand *alias_values,
    MachineIrTerminator *terminator,
    int *out_changed) {
    MachineIrOperand before;
    MachineIrOperand after;

    if (!terminator) {
        return;
    }

    switch (terminator->kind) {
        case MACHINE_IR_TERM_RETURN:
            before = terminator->as.return_value;
            after = machine_ir_resolve_local_copy_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_ir_operands_equal(before, after)) {
                terminator->as.return_value = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        case MACHINE_IR_TERM_BRANCH:
            before = terminator->as.branch.condition;
            after = machine_ir_resolve_local_copy_alias(
                resource_count, program, function, alias_valid, alias_values, before);
            if (!machine_ir_operands_equal(before, after)) {
                terminator->as.branch.condition = after;
                if (out_changed) {
                    *out_changed = 1;
                }
            }
            break;
        default:
            break;
    }
}

static int machine_ir_instruction_is_preserved_edge_copy(const MachineIrFunction *function,
    const MachineIrBasicBlock *block,
    size_t instruction_index,
    const size_t *predecessor_counts) {
    size_t successor_a = (size_t)-1;
    size_t successor_b = (size_t)-1;
    size_t trailing_index;

    if (!function || !block || !predecessor_counts || instruction_index >= block->instruction_count) {
        return 0;
    }
    if (block->instructions[instruction_index].kind != MACHINE_IR_INSTR_MOV) {
        return 0;
    }
    if (!block->has_terminator) {
        return 0;
    }

    switch (block->terminator.kind) {
        case MACHINE_IR_TERM_JUMP:
            successor_a = block->terminator.as.jump_target;
            break;
        case MACHINE_IR_TERM_BRANCH:
            successor_a = block->terminator.as.branch.then_target;
            successor_b = block->terminator.as.branch.else_target;
            break;
        default:
            return 0;
    }

    if ((successor_a >= function->block_count || predecessor_counts[successor_a] <= 1u) &&
        (successor_b >= function->block_count || predecessor_counts[successor_b] <= 1u)) {
        return 0;
    }
    for (trailing_index = instruction_index + 1u; trailing_index < block->instruction_count; ++trailing_index) {
        if (block->instructions[trailing_index].kind != MACHINE_IR_INSTR_MOV) {
            return 0;
        }
    }
    return 1;
}

static void machine_ir_collect_block_use_def(const MachineIrProgram *program,
    const MachineIrFunction *function,
    const MachineIrBasicBlock *block,
    size_t resource_count,
    unsigned char *use_set,
    unsigned char *def_set) {
    unsigned char *seen_defs = NULL;
    size_t instruction_index;

    if (!program || !function || !block || resource_count == 0 || !use_set || !def_set) {
        return;
    }

    seen_defs = (unsigned char *)calloc(resource_count, sizeof(unsigned char));
    if (!seen_defs) {
        return;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        const MachineIrInstruction *instruction = &block->instructions[instruction_index];
        size_t resource_index;
        size_t arg_index;

        switch (instruction->kind) {
            case MACHINE_IR_INSTR_MOV:
                if (machine_ir_operand_get_resource_index(
                        program, function, instruction->as.mov_value, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            case MACHINE_IR_INSTR_BINARY:
                if (machine_ir_operand_get_resource_index(
                        program, function, instruction->as.binary.lhs, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                if (machine_ir_operand_get_resource_index(
                        program, function, instruction->as.binary.rhs, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            case MACHINE_IR_INSTR_CALL:
                for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    if (machine_ir_operand_get_resource_index(
                            program, function, instruction->as.call.args[arg_index], &resource_index) &&
                        !seen_defs[resource_index]) {
                        use_set[resource_index] = 1u;
                    }
                }
                for (resource_index = 0; resource_index < program->register_bank.register_count; ++resource_index) {
                    if (!program->register_bank.registers[resource_index].caller_clobbered) {
                        continue;
                    }
                    seen_defs[resource_index] = 1u;
                    def_set[resource_index] = 1u;
                }
                break;
            case MACHINE_IR_INSTR_STORE_LOCAL:
            case MACHINE_IR_INSTR_STORE_GLOBAL:
                if (machine_ir_operand_get_resource_index(
                        program, function, instruction->as.store.value, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            case MACHINE_IR_INSTR_LOAD_INDIRECT:
                if (machine_ir_operand_get_resource_index(
                        program, function, instruction->as.load_indirect_addr, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            case MACHINE_IR_INSTR_STORE_INDIRECT:
                if (machine_ir_operand_get_resource_index(
                        program, function, instruction->as.store_indirect.addr, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                if (machine_ir_operand_get_resource_index(
                        program, function, instruction->as.store_indirect.value, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            default:
                break;
        }

        if (instruction->has_result &&
            machine_ir_operand_get_resource_index(program, function, instruction->result, &resource_index)) {
            seen_defs[resource_index] = 1u;
            def_set[resource_index] = 1u;
        }
    }

    if (block->has_terminator) {
        size_t resource_index;

        switch (block->terminator.kind) {
            case MACHINE_IR_TERM_RETURN:
                if (machine_ir_operand_get_resource_index(
                        program, function, block->terminator.as.return_value, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            case MACHINE_IR_TERM_BRANCH:
                if (machine_ir_operand_get_resource_index(
                        program, function, block->terminator.as.branch.condition, &resource_index) &&
                    !seen_defs[resource_index]) {
                    use_set[resource_index] = 1u;
                }
                break;
            default:
                break;
        }
    }

    free(seen_defs);
}

static int machine_ir_compute_block_live_out(const MachineIrProgram *program,
    const MachineIrFunction *function,
    size_t resource_count,
    unsigned char **out_live_out_sets,
    MachineIrError *error) {
    unsigned char *use_sets = NULL;
    unsigned char *def_sets = NULL;
    unsigned char *live_in_sets = NULL;
    unsigned char *live_out_sets = NULL;
    int changed = 1;
    size_t block_index;

    if (out_live_out_sets) {
        *out_live_out_sets = NULL;
    }
    if (!program || !function || !out_live_out_sets) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-534: invalid block-liveness cleanup contract");
        return 0;
    }
    if (function->block_count == 0 || resource_count == 0) {
        return 1;
    }

    use_sets = (unsigned char *)calloc(function->block_count * resource_count, sizeof(unsigned char));
    def_sets = (unsigned char *)calloc(function->block_count * resource_count, sizeof(unsigned char));
    live_in_sets = (unsigned char *)calloc(function->block_count * resource_count, sizeof(unsigned char));
    live_out_sets = (unsigned char *)calloc(function->block_count * resource_count, sizeof(unsigned char));
    if (!use_sets || !def_sets || !live_in_sets || !live_out_sets) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-535: out of memory computing machine-ir block liveness");
        free(live_out_sets);
        free(live_in_sets);
        free(def_sets);
        free(use_sets);
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        machine_ir_collect_block_use_def(program,
            function,
            &function->blocks[block_index],
            resource_count,
            use_sets + (block_index * resource_count),
            def_sets + (block_index * resource_count));
    }

    while (changed) {
        changed = 0;
        for (block_index = function->block_count; block_index-- > 0;) {
            const MachineIrBasicBlock *block = &function->blocks[block_index];
            unsigned char *live_in = live_in_sets + (block_index * resource_count);
            unsigned char *live_out = live_out_sets + (block_index * resource_count);
            size_t resource_index;
            size_t successor_a = (size_t)-1;
            size_t successor_b = (size_t)-1;

            if (block->has_terminator) {
                switch (block->terminator.kind) {
                    case MACHINE_IR_TERM_JUMP:
                        successor_a = block->terminator.as.jump_target;
                        break;
                    case MACHINE_IR_TERM_BRANCH:
                        successor_a = block->terminator.as.branch.then_target;
                        successor_b = block->terminator.as.branch.else_target;
                        break;
                    default:
                        break;
                }
            }

            for (resource_index = 0; resource_index < resource_count; ++resource_index) {
                unsigned char new_live_out = 0;
                unsigned char new_live_in;

                if (successor_a != (size_t)-1) {
                    new_live_out |= live_in_sets[(successor_a * resource_count) + resource_index];
                }
                if (successor_b != (size_t)-1) {
                    new_live_out |= live_in_sets[(successor_b * resource_count) + resource_index];
                }

                new_live_in = use_sets[(block_index * resource_count) + resource_index];
                if (!def_sets[(block_index * resource_count) + resource_index] && new_live_out) {
                    new_live_in = 1u;
                }

                if (live_out[resource_index] != new_live_out) {
                    live_out[resource_index] = new_live_out;
                    changed = 1;
                }
                if (live_in[resource_index] != new_live_in) {
                    live_in[resource_index] = new_live_in;
                    changed = 1;
                }
            }
        }
    }

    free(live_in_sets);
    free(def_sets);
    free(use_sets);
    *out_live_out_sets = live_out_sets;
    return 1;
}

static int machine_ir_cleanup_local_copies_in_function(MachineIrProgram *program,
    MachineIrFunction *function,
    int *out_changed,
    MachineIrError *error) {
    size_t resource_count;
    unsigned char *live_out_sets = NULL;
    unsigned char *alias_valid = NULL;
    MachineIrOperand *alias_values = NULL;
    unsigned char *block_exit_alias_valid = NULL;
    MachineIrOperand *block_exit_alias_values = NULL;
    size_t *predecessor_counts = NULL;
    size_t block_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!program || !function) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-536: invalid local-copy cleanup contract");
        return 0;
    }

    resource_count = program->register_bank.register_count + function->spill_slot_count;
    if (function->block_count == 0 || resource_count == 0) {
        return 1;
    }

    if (!machine_ir_compute_block_live_out(program, function, resource_count, &live_out_sets, error)) {
        return 0;
    }
    if (!machine_ir_compute_predecessor_count_map(function, &predecessor_counts, error)) {
        free(live_out_sets);
        return 0;
    }

    alias_valid = (unsigned char *)calloc(resource_count, sizeof(unsigned char));
    alias_values = (MachineIrOperand *)calloc(resource_count, sizeof(MachineIrOperand));
    block_exit_alias_valid =
        (unsigned char *)calloc(function->block_count * resource_count, sizeof(unsigned char));
    block_exit_alias_values =
        (MachineIrOperand *)calloc(function->block_count * resource_count, sizeof(MachineIrOperand));
    if (!alias_valid || !alias_values || !block_exit_alias_valid || !block_exit_alias_values) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-537: out of memory preparing local-copy cleanup");
        free(block_exit_alias_values);
        free(block_exit_alias_valid);
        free(alias_values);
        free(alias_valid);
        free(predecessor_counts);
        free(live_out_sets);
        return 0;
    }

    if (!machine_ir_compute_block_exit_aliases(
            program,
            function,
            resource_count,
            predecessor_counts,
            block_exit_alias_valid,
            block_exit_alias_values,
            error)) {
        free(block_exit_alias_values);
        free(block_exit_alias_valid);
        free(alias_values);
        free(alias_valid);
        free(predecessor_counts);
        free(live_out_sets);
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        MachineIrBasicBlock *block = &function->blocks[block_index];
        unsigned char *needed = NULL;
        unsigned char *delete_flags = NULL;
        size_t instruction_index;
        size_t resource_index;
        size_t write_index;

        machine_ir_compute_block_entry_aliases(
            function,
            block_index,
            resource_count,
            predecessor_counts,
            block_exit_alias_valid,
            block_exit_alias_values,
            alias_valid,
            alias_values);
        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            MachineIrInstruction *instruction = &block->instructions[instruction_index];
            size_t result_index;

            machine_ir_rewrite_instruction_operands_with_local_aliases(
                resource_count, program, function, alias_valid, alias_values, instruction, out_changed);

            if (instruction->has_result &&
                machine_ir_operand_get_resource_index(program, function, instruction->result, &result_index)) {
                machine_ir_invalidate_local_copy_aliases(
                    program, function, resource_count, alias_valid, alias_values, instruction->result);
            }

            if (instruction->kind == MACHINE_IR_INSTR_MOV &&
                instruction->has_result &&
                machine_ir_operand_get_resource_index(program, function, instruction->result, &result_index) &&
                !machine_ir_operands_equal(instruction->result, instruction->as.mov_value)) {
                alias_valid[result_index] = 1u;
                alias_values[result_index] =
                    machine_ir_resolve_local_copy_alias(resource_count,
                        program,
                        function,
                        alias_valid,
                        alias_values,
                        instruction->as.mov_value);
            }
            if (instruction->kind == MACHINE_IR_INSTR_CALL) {
                machine_ir_invalidate_caller_clobbered_aliases(
                    program,
                    function,
                    resource_count,
                    alias_valid,
                    alias_values);
            }
        }
        machine_ir_rewrite_terminator_operands_with_local_aliases(
            resource_count, program, function, alias_valid, alias_values, &block->terminator, out_changed);

        needed = (unsigned char *)calloc(resource_count, sizeof(unsigned char));
        delete_flags = block->instruction_count > 0
            ? (unsigned char *)calloc(block->instruction_count, sizeof(unsigned char))
            : NULL;
        if (!needed || (block->instruction_count > 0 && !delete_flags)) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-538: out of memory preparing dead-move cleanup");
            free(delete_flags);
            free(needed);
            free(predecessor_counts);
            free(alias_values);
            free(alias_valid);
            free(live_out_sets);
            return 0;
        }

        memcpy(needed,
            live_out_sets + (block_index * resource_count),
            resource_count * sizeof(unsigned char));
        if (block->has_terminator) {
            switch (block->terminator.kind) {
                case MACHINE_IR_TERM_RETURN:
                    machine_ir_mark_operand_resource(program, function, needed, block->terminator.as.return_value);
                    break;
                case MACHINE_IR_TERM_BRANCH:
                    machine_ir_mark_operand_resource(program, function, needed, block->terminator.as.branch.condition);
                    break;
                default:
                    break;
            }
        }

        for (instruction_index = block->instruction_count; instruction_index-- > 0;) {
            MachineIrInstruction *instruction = &block->instructions[instruction_index];
            size_t result_index = (size_t)-1;
            int removable_dead_def = machine_ir_instruction_is_removable_dead_def(instruction);
            int preserve_edge_copy = machine_ir_instruction_is_preserved_edge_copy(
                function,
                block,
                instruction_index,
                predecessor_counts);

            if (!preserve_edge_copy &&
                instruction->kind == MACHINE_IR_INSTR_MOV &&
                instruction->has_result &&
                machine_ir_operands_equal(instruction->result, instruction->as.mov_value)) {
                delete_flags[instruction_index] = 1u;
                if (out_changed) {
                    *out_changed = 1;
                }
                continue;
            }

            if (instruction->has_result &&
                machine_ir_operand_get_resource_index(program, function, instruction->result, &result_index) &&
                removable_dead_def &&
                !preserve_edge_copy &&
                !needed[result_index]) {
                delete_flags[instruction_index] = 1u;
                if (out_changed) {
                    *out_changed = 1;
                }
                continue;
            }

            if (instruction->has_result &&
                machine_ir_operand_get_resource_index(program, function, instruction->result, &result_index)) {
                needed[result_index] = 0u;
            }

            switch (instruction->kind) {
                case MACHINE_IR_INSTR_MOV:
                    machine_ir_mark_operand_resource(program, function, needed, instruction->as.mov_value);
                    break;
                case MACHINE_IR_INSTR_BINARY:
                    machine_ir_mark_operand_resource(program, function, needed, instruction->as.binary.lhs);
                    machine_ir_mark_operand_resource(program, function, needed, instruction->as.binary.rhs);
                    break;
                case MACHINE_IR_INSTR_CALL:
                    for (resource_index = 0; resource_index < instruction->as.call.arg_count; ++resource_index) {
                        machine_ir_mark_operand_resource(
                            program, function, needed, instruction->as.call.args[resource_index]);
                    }
                    break;
                case MACHINE_IR_INSTR_LOAD_INDIRECT:
                    machine_ir_mark_operand_resource(
                        program, function, needed, instruction->as.load_indirect_addr);
                    break;
                case MACHINE_IR_INSTR_STORE_INDIRECT:
                    machine_ir_mark_operand_resource(
                        program, function, needed, instruction->as.store_indirect.addr);
                    machine_ir_mark_operand_resource(
                        program, function, needed, instruction->as.store_indirect.value);
                    break;
                case MACHINE_IR_INSTR_STORE_LOCAL:
                case MACHINE_IR_INSTR_STORE_GLOBAL:
                    machine_ir_mark_operand_resource(program, function, needed, instruction->as.store.value);
                    break;
                default:
                    break;
            }
        }

        write_index = 0;
        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            if (delete_flags[instruction_index]) {
                machine_ir_instruction_free(&block->instructions[instruction_index]);
                memset(&block->instructions[instruction_index], 0, sizeof(block->instructions[instruction_index]));
                continue;
            }
            if (write_index != instruction_index) {
                block->instructions[write_index] = block->instructions[instruction_index];
                memset(&block->instructions[instruction_index], 0, sizeof(block->instructions[instruction_index]));
            }
            ++write_index;
        }
        block->instruction_count = write_index;

        free(delete_flags);
        free(needed);
    }

    free(predecessor_counts);
    free(block_exit_alias_values);
    free(block_exit_alias_valid);
    free(alias_values);
    free(alias_valid);
    free(live_out_sets);
    return 1;
}

#endif
