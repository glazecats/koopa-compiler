#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_SPLIT_AGGREGATOR)
#define MEMORY_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa.c"
#else

static int memory_ssa_slot_ref_equal(ValueSsaSlotRef lhs, ValueSsaSlotRef rhs) {
    return lhs.kind == rhs.kind && lhs.id == rhs.id;
}

static int memory_ssa_verify_reachable_blocks(const MemorySsaFunction *function,
    unsigned char *reachable,
    MemorySsaError *error) {
    size_t *stack = NULL;
    size_t stack_size = 0;

    if (!function || !reachable) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-031: invalid reachability contract");
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }

    stack = (size_t *)malloc(function->block_count * sizeof(size_t));
    if (!stack) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-032: out of memory while computing reachability");
        return 0;
    }

    stack[stack_size++] = 0;
    reachable[0] = 1;
    while (stack_size > 0) {
        const MemorySsaBasicBlock *block = &function->blocks[stack[--stack_size]];

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
            memory_ssa_set_error(error,
                0,
                0,
                "MEMORY-SSA-033: bb.%zu uses unknown terminator kind %d",
                block->id,
                (int)block->terminator.kind);
            return 0;
        }
    }

    free(stack);
    return 1;
}

int memory_ssa_verify_program(const MemorySsaProgram *program, MemorySsaError *error) {
    size_t function_index;

    if (!program) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-034: invalid program verifier contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MemorySsaFunction *function = &program->functions[function_index];
        unsigned char *reachable = NULL;
        size_t *version_defs = NULL;
        size_t block_index;

        if (function->has_body && function->block_count == 0) {
            memory_ssa_set_error(error,
                0,
                0,
                "MEMORY-SSA-035: function '%s' has a body but no blocks",
                function->name ? function->name : "<anon>");
            return 0;
        }

        for (block_index = 0; block_index < function->tracked_slot_count; ++block_index) {
            const MemorySsaTrackedSlot *slot = &function->tracked_slots[block_index];

            if (slot->id != block_index) {
                memory_ssa_set_error(error,
                    0,
                    0,
                    "MEMORY-SSA-036: function '%s' tracked slot index %zu stores id %zu",
                    function->name ? function->name : "<anon>",
                    block_index,
                    slot->id);
                return 0;
            }
            if (!slot->debug_name || slot->debug_name[0] == '\0') {
                memory_ssa_set_error(error,
                    0,
                    0,
                    "MEMORY-SSA-037: function '%s' tracked slot %zu is missing a debug name",
                    function->name ? function->name : "<anon>",
                    slot->id);
                return 0;
            }
            if (slot->has_entry_version && slot->entry_version_id >= function->next_version_id) {
                memory_ssa_set_error(error,
                    0,
                    0,
                    "MEMORY-SSA-083: function '%s' tracked slot %zu has invalid entry version mem.%zu",
                    function->name ? function->name : "<anon>",
                    slot->id,
                    slot->entry_version_id);
                return 0;
            }
        }

        reachable = (unsigned char *)calloc(function->block_count ? function->block_count : 1, sizeof(unsigned char));
        version_defs = (size_t *)calloc(function->next_version_id ? function->next_version_id : 1, sizeof(size_t));
        if (!reachable || !version_defs) {
            free(reachable);
            free(version_defs);
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-038: out of memory while preparing verification");
            return 0;
        }

        if (function->block_count > 0 && !memory_ssa_verify_reachable_blocks(function, reachable, error)) {
            free(reachable);
            free(version_defs);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MemorySsaBasicBlock *block = &function->blocks[block_index];
            size_t phi_index;
            size_t instruction_index;

            if (block->id != block_index) {
                free(reachable);
                free(version_defs);
                memory_ssa_set_error(error,
                    0,
                    0,
                    "MEMORY-SSA-039: function '%s' block index %zu stores id %zu",
                    function->name ? function->name : "<anon>",
                    block_index,
                    block->id);
                return 0;
            }
            if (!reachable[block_index]) {
                free(reachable);
                free(version_defs);
                memory_ssa_set_error(error,
                    0,
                    0,
                    "MEMORY-SSA-040: function '%s' has unreachable bb.%zu",
                    function->name ? function->name : "<anon>",
                    block->id);
                return 0;
            }
            if (!block->has_terminator) {
                free(reachable);
                free(version_defs);
                memory_ssa_set_error(error,
                    0,
                    0,
                    "MEMORY-SSA-041: function '%s' bb.%zu is missing a terminator",
                    function->name ? function->name : "<anon>",
                    block->id);
                return 0;
            }

            switch (block->terminator.kind) {
            case VALUE_SSA_TERM_RETURN:
                break;
            case VALUE_SSA_TERM_JUMP:
                if (block->terminator.as.jump_target >= function->block_count) {
                    free(reachable);
                    free(version_defs);
                    memory_ssa_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-042: bb.%zu jumps to invalid target bb.%zu",
                        block->id,
                        block->terminator.as.jump_target);
                    return 0;
                }
                break;
            case VALUE_SSA_TERM_BRANCH:
                if (block->terminator.as.branch.then_target >= function->block_count ||
                    block->terminator.as.branch.else_target >= function->block_count) {
                    free(reachable);
                    free(version_defs);
                    memory_ssa_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-043: bb.%zu branches to an invalid target",
                        block->id);
                    return 0;
                }
                break;
            default:
                free(reachable);
                free(version_defs);
                memory_ssa_set_error(error,
                    0,
                    0,
                    "MEMORY-SSA-044: bb.%zu uses unknown terminator kind %d",
                    block->id,
                    (int)block->terminator.kind);
                return 0;
            }

            for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                const MemorySsaPhi *phi = &block->phis[phi_index];
                size_t pred_index;
                size_t predecessor_count = 0;

                if (phi->slot_id >= function->tracked_slot_count) {
                    free(reachable);
                    free(version_defs);
                    memory_ssa_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-045: bb.%zu phi %zu references invalid slot %zu",
                        block->id,
                        phi_index,
                        phi->slot_id);
                    return 0;
                }
                if (phi->result_version_id >= function->next_version_id) {
                    free(reachable);
                    free(version_defs);
                    memory_ssa_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-046: bb.%zu phi %zu defines invalid version mem.%zu",
                        block->id,
                        phi_index,
                        phi->result_version_id);
                    return 0;
                }
                ++version_defs[phi->result_version_id];

                for (pred_index = 0; pred_index < function->block_count; ++pred_index) {
                    const MemorySsaBasicBlock *pred_block = &function->blocks[pred_index];
                    int is_predecessor = 0;

                    if (!reachable[pred_index]) {
                        continue;
                    }

                    switch (pred_block->terminator.kind) {
                    case VALUE_SSA_TERM_JUMP:
                        is_predecessor = pred_block->terminator.as.jump_target == block->id;
                        break;
                    case VALUE_SSA_TERM_BRANCH:
                        is_predecessor = pred_block->terminator.as.branch.then_target == block->id ||
                            pred_block->terminator.as.branch.else_target == block->id;
                        break;
                    default:
                        break;
                    }

                    predecessor_count += is_predecessor ? 1u : 0u;
                }

                if (phi->input_count != predecessor_count) {
                    free(reachable);
                    free(version_defs);
                    memory_ssa_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-047: bb.%zu phi %zu input_count=%zu does not match predecessor count %zu",
                        block->id,
                        phi_index,
                        phi->input_count,
                        predecessor_count);
                    return 0;
                }

                for (pred_index = 0; pred_index < phi->input_count; ++pred_index) {
                    size_t other_index;

                    if (phi->inputs[pred_index].predecessor_block_id >= function->block_count ||
                        phi->inputs[pred_index].version_id >= function->next_version_id) {
                        free(reachable);
                        free(version_defs);
                        memory_ssa_set_error(error,
                            0,
                            0,
                            "MEMORY-SSA-048: bb.%zu phi %zu has invalid input",
                            block->id,
                            phi_index);
                        return 0;
                    }
                    for (other_index = pred_index + 1; other_index < phi->input_count; ++other_index) {
                        if (phi->inputs[pred_index].predecessor_block_id ==
                            phi->inputs[other_index].predecessor_block_id) {
                            free(reachable);
                            free(version_defs);
                            memory_ssa_set_error(error,
                                0,
                                0,
                                "MEMORY-SSA-049: bb.%zu phi %zu repeats predecessor bb.%zu",
                                block->id,
                                phi_index,
                                phi->inputs[pred_index].predecessor_block_id);
                            return 0;
                        }
                    }
                }
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const MemorySsaInstruction *instruction = &block->instructions[instruction_index];
                size_t access_index;
                size_t expected_access_count = 0;

                switch (instruction->instruction.kind) {
                case VALUE_SSA_INSTR_ADDR_LOCAL:
                case VALUE_SSA_INSTR_ADDR_GLOBAL:
                case VALUE_SSA_INSTR_LOAD_INDIRECT:
                case VALUE_SSA_INSTR_STORE_INDIRECT:
                    expected_access_count = 0;
                    break;
                case VALUE_SSA_INSTR_LOAD_LOCAL:
                case VALUE_SSA_INSTR_LOAD_GLOBAL:
                case VALUE_SSA_INSTR_STORE_LOCAL:
                case VALUE_SSA_INSTR_STORE_GLOBAL:
                    expected_access_count = 1;
                    break;
                case VALUE_SSA_INSTR_MOV:
                case VALUE_SSA_INSTR_BINARY:
                case VALUE_SSA_INSTR_CALL:
                    expected_access_count = instruction->instruction.kind == VALUE_SSA_INSTR_CALL
                        ? instruction->access_count
                        : 0;
                    break;
                default:
                    free(reachable);
                    free(version_defs);
                    memory_ssa_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-050: bb.%zu uses unknown instruction kind %d",
                        block->id,
                        (int)instruction->instruction.kind);
                    return 0;
                }

                if (instruction->access_count != expected_access_count) {
                    free(reachable);
                    free(version_defs);
                    memory_ssa_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-051: bb.%zu instr %zu access_count=%zu does not match expected %zu",
                        block->id,
                        instruction_index,
                        instruction->access_count,
                        expected_access_count);
                    return 0;
                }

                for (access_index = 0; access_index < instruction->access_count; ++access_index) {
                    const MemorySsaAccess *access = &instruction->accesses[access_index];
                    size_t other_access_index;

                    if (access->slot_id >= function->tracked_slot_count) {
                        free(reachable);
                        free(version_defs);
                        memory_ssa_set_error(error,
                            0,
                            0,
                            "MEMORY-SSA-052: bb.%zu instr %zu references invalid slot %zu",
                            block->id,
                            instruction_index,
                            access->slot_id);
                        return 0;
                    }
                    if (access->has_use_version && access->use_version_id >= function->next_version_id) {
                        free(reachable);
                        free(version_defs);
                        memory_ssa_set_error(error,
                            0,
                            0,
                            "MEMORY-SSA-053: bb.%zu instr %zu reads invalid version mem.%zu",
                            block->id,
                            instruction_index,
                            access->use_version_id);
                        return 0;
                    }
                    if (access->has_def_version) {
                        if (access->def_version_id >= function->next_version_id) {
                            free(reachable);
                            free(version_defs);
                            memory_ssa_set_error(error,
                                0,
                                0,
                                "MEMORY-SSA-054: bb.%zu instr %zu defines invalid version mem.%zu",
                                block->id,
                                instruction_index,
                            access->def_version_id);
                            return 0;
                        }
                        ++version_defs[access->def_version_id];
                    }
                    if (instruction->instruction.kind == VALUE_SSA_INSTR_CALL) {
                        if (!access->has_use_version && !access->has_def_version) {
                            free(reachable);
                            free(version_defs);
                            memory_ssa_set_error(error,
                                0,
                                0,
                                "MEMORY-SSA-057: bb.%zu call instr %zu has effect-free slot.%zu access",
                                block->id,
                                instruction_index,
                                access->slot_id);
                            return 0;
                        }
                        if (function->tracked_slots[access->slot_id].slot.kind != VALUE_SSA_SLOT_GLOBAL) {
                            free(reachable);
                            free(version_defs);
                            memory_ssa_set_error(error,
                                0,
                                0,
                                "MEMORY-SSA-058: bb.%zu call instr %zu references non-global slot.%zu",
                                block->id,
                                instruction_index,
                                access->slot_id);
                            return 0;
                        }
                        for (other_access_index = access_index + 1;
                             other_access_index < instruction->access_count;
                             ++other_access_index) {
                            if (instruction->accesses[other_access_index].slot_id == access->slot_id) {
                                free(reachable);
                                free(version_defs);
                                memory_ssa_set_error(error,
                                    0,
                                    0,
                                    "MEMORY-SSA-059: bb.%zu call instr %zu repeats slot.%zu",
                                    block->id,
                                    instruction_index,
                                    access->slot_id);
                                return 0;
                            }
                        }
                    }
                }

                if (instruction->instruction.kind == VALUE_SSA_INSTR_LOAD_LOCAL ||
                    instruction->instruction.kind == VALUE_SSA_INSTR_LOAD_GLOBAL ||
                    instruction->instruction.kind == VALUE_SSA_INSTR_STORE_LOCAL ||
                    instruction->instruction.kind == VALUE_SSA_INSTR_STORE_GLOBAL) {
                    const MemorySsaAccess *access = &instruction->accesses[0];
                    const ValueSsaSlotRef slot = function->tracked_slots[access->slot_id].slot;
                    ValueSsaSlotRef instruction_slot = instruction->instruction.kind == VALUE_SSA_INSTR_LOAD_LOCAL ||
                            instruction->instruction.kind == VALUE_SSA_INSTR_LOAD_GLOBAL
                        ? instruction->instruction.as.load_slot
                        : instruction->instruction.as.store.slot;

                    if (!memory_ssa_slot_ref_equal(slot, instruction_slot)) {
                        free(reachable);
                        free(version_defs);
                        memory_ssa_set_error(error,
                            0,
                            0,
                            "MEMORY-SSA-055: bb.%zu instr %zu slot annotation mismatch",
                            block->id,
                            instruction_index);
                        return 0;
                    }
                }
            }
        }

        for (block_index = 0; block_index < function->tracked_slot_count; ++block_index) {
            const MemorySsaTrackedSlot *slot = &function->tracked_slots[block_index];

            if (slot->has_entry_version) {
                ++version_defs[slot->entry_version_id];
            }
        }

        for (block_index = 0; block_index < function->next_version_id; ++block_index) {
            if (version_defs[block_index] != 1) {
                free(reachable);
                free(version_defs);
                memory_ssa_set_error(error,
                    0,
                    0,
                    "MEMORY-SSA-056: function '%s' does not define mem.%zu exactly once",
                    function->name ? function->name : "<anon>",
                    block_index);
                return 0;
            }
        }
        free(reachable);
        free(version_defs);
    }

    return 1;
}

#endif
