#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_SPLIT_AGGREGATOR)
#define MEMORY_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa.c"
#else

static const MemorySsaTrackedSlot *memory_ssa_find_tracked_slot_by_ref(const MemorySsaFunction *function,
    ValueSsaSlotRef slot) {
    size_t slot_index;

    if (!function) {
        return NULL;
    }

    for (slot_index = 0; slot_index < function->tracked_slot_count; ++slot_index) {
        const MemorySsaTrackedSlot *tracked_slot = &function->tracked_slots[slot_index];

        if (tracked_slot->slot.kind == slot.kind && tracked_slot->slot.id == slot.id) {
            return tracked_slot;
        }
    }

    return NULL;
}

static int memory_ssa_append_value_name(MemorySsaStringBuilder *builder, ValueSsaValueRef value) {
    if (value.kind == VALUE_SSA_VALUE_IMMEDIATE) {
        return memory_ssa_append_format(builder, "%lld", value.immediate);
    }
    if (value.kind == VALUE_SSA_VALUE_ID) {
        return memory_ssa_append_format(builder, "ssa.%zu", value.value_id);
    }
    return memory_ssa_append_string(builder, "<bad-value>");
}

static int memory_ssa_append_slot_name_with_kind(MemorySsaStringBuilder *builder,
    const MemorySsaFunction *function,
    ValueSsaSlotRef slot) {
    const MemorySsaTrackedSlot *tracked_slot = memory_ssa_find_tracked_slot_by_ref(function, slot);

    if (tracked_slot && tracked_slot->debug_name) {
        return memory_ssa_append_format(builder,
            "%s %s",
            slot.kind == VALUE_SSA_SLOT_LOCAL ? "local" : "global",
            tracked_slot->debug_name);
    }

    return memory_ssa_append_format(builder,
        "%s %zu",
        slot.kind == VALUE_SSA_SLOT_LOCAL ? "local" : "global",
        slot.id);
}

static int memory_ssa_append_slot_name(MemorySsaStringBuilder *builder,
    const MemorySsaFunction *function,
    ValueSsaSlotRef slot) {
    const MemorySsaTrackedSlot *tracked_slot = memory_ssa_find_tracked_slot_by_ref(function, slot);

    if (tracked_slot && tracked_slot->debug_name) {
        return memory_ssa_append_string(builder, tracked_slot->debug_name);
    }

    return memory_ssa_append_format(builder, "%zu", slot.id);
}

static int memory_ssa_append_call_instruction(MemorySsaStringBuilder *builder,
    const MemorySsaInstruction *instruction) {
    size_t arg_index;

    if (instruction->instruction.has_result &&
        (!memory_ssa_append_value_name(builder, instruction->instruction.result) ||
            !memory_ssa_append_string(builder, " = "))) {
        return 0;
    }
    if (!memory_ssa_append_format(builder,
            "call %s(",
            instruction->instruction.as.call.callee_name ? instruction->instruction.as.call.callee_name : "<anon>")) {
        return 0;
    }
    for (arg_index = 0; arg_index < instruction->instruction.as.call.arg_count; ++arg_index) {
        if (arg_index > 0 && !memory_ssa_append_string(builder, ", ")) {
            return 0;
        }
        if (!memory_ssa_append_value_name(builder, instruction->instruction.as.call.args[arg_index])) {
            return 0;
        }
    }
    if (!memory_ssa_append_string(builder, ")")) {
        return 0;
    }

    if (instruction->access_count > 0) {
        size_t access_index;

        if (!memory_ssa_append_string(builder, " [")) {
            return 0;
        }
        for (access_index = 0; access_index < instruction->access_count; ++access_index) {
            const MemorySsaAccess *access = &instruction->accesses[access_index];

            if (access_index > 0 && !memory_ssa_append_string(builder, ", ")) {
                return 0;
            }
            if (!memory_ssa_append_format(builder, "slot.%zu", access->slot_id)) {
                return 0;
            }
            if (access->has_use_version &&
                !memory_ssa_append_format(builder, " use=mem.%zu", access->use_version_id)) {
                return 0;
            }
            if (access->has_def_version &&
                !memory_ssa_append_format(builder, " def=mem.%zu", access->def_version_id)) {
                return 0;
            }
        }
        if (!memory_ssa_append_string(builder, "]")) {
            return 0;
        }
    }

    return 1;
}

int memory_ssa_dump_program(const MemorySsaProgram *program, char **out_text) {
    MemorySsaStringBuilder builder;
    size_t function_index;

    if (!program || !out_text) {
        return 0;
    }

    *out_text = NULL;
    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MemorySsaFunction *function = &program->functions[function_index];
        size_t slot_index;
        size_t block_index;

        if (function_index > 0 && !memory_ssa_append_string(&builder, "\n")) {
            free(builder.data);
            return 0;
        }

        if (!memory_ssa_append_format(&builder,
                function->has_body ? "func %s() {\n" : "declare %s()\n",
                function->name ? function->name : "<anon>")) {
            free(builder.data);
            return 0;
        }
        if (!function->has_body) {
            continue;
        }

        if (!memory_ssa_append_string(&builder, "  slots:\n")) {
            free(builder.data);
            return 0;
        }
        for (slot_index = 0; slot_index < function->tracked_slot_count; ++slot_index) {
            const MemorySsaTrackedSlot *tracked_slot = &function->tracked_slots[slot_index];

            if (!memory_ssa_append_format(&builder, "    slot.%zu = ", tracked_slot->id) ||
                !memory_ssa_append_slot_name_with_kind(&builder, function, tracked_slot->slot) ||
                (tracked_slot->has_entry_version &&
                    !memory_ssa_append_format(&builder, " entry=mem.%zu", tracked_slot->entry_version_id)) ||
                !memory_ssa_append_string(&builder, "\n")) {
                free(builder.data);
                return 0;
            }
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MemorySsaBasicBlock *block = &function->blocks[block_index];
            size_t phi_index;
            size_t instruction_index;

            if (!memory_ssa_append_format(&builder, "  bb.%zu:\n", block->id)) {
                free(builder.data);
                return 0;
            }

            for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
                const MemorySsaPhi *phi = &block->phis[phi_index];
                size_t input_index;

                if (!memory_ssa_append_format(&builder, "    mem.%zu = phi slot.%zu ", phi->result_version_id, phi->slot_id)) {
                    free(builder.data);
                    return 0;
                }
                for (input_index = 0; input_index < phi->input_count; ++input_index) {
                    if (input_index > 0 && !memory_ssa_append_string(&builder, ", ")) {
                        free(builder.data);
                        return 0;
                    }
                    if (!memory_ssa_append_format(&builder,
                            "[bb.%zu: mem.%zu]",
                            phi->inputs[input_index].predecessor_block_id,
                            phi->inputs[input_index].version_id)) {
                        free(builder.data);
                        return 0;
                    }
                }
                if (!memory_ssa_append_string(&builder, "\n")) {
                    free(builder.data);
                    return 0;
                }
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const MemorySsaInstruction *instruction = &block->instructions[instruction_index];

                if (!memory_ssa_append_string(&builder, "    ")) {
                    free(builder.data);
                    return 0;
                }

                switch (instruction->instruction.kind) {
                case VALUE_SSA_INSTR_MOV:
                    if (!memory_ssa_append_value_name(&builder, instruction->instruction.result) ||
                        !memory_ssa_append_string(&builder, " = mov ") ||
                        !memory_ssa_append_value_name(&builder, instruction->instruction.as.mov_value)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case VALUE_SSA_INSTR_BINARY:
                    if (!memory_ssa_append_value_name(&builder, instruction->instruction.result) ||
                        !memory_ssa_append_format(&builder,
                            " = %d ",
                            (int)instruction->instruction.as.binary.op) ||
                        !memory_ssa_append_value_name(&builder, instruction->instruction.as.binary.lhs) ||
                        !memory_ssa_append_string(&builder, ", ") ||
                        !memory_ssa_append_value_name(&builder, instruction->instruction.as.binary.rhs)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case VALUE_SSA_INSTR_LOAD_LOCAL:
                case VALUE_SSA_INSTR_LOAD_GLOBAL:
                    if (!memory_ssa_append_value_name(&builder, instruction->instruction.result) ||
                        !memory_ssa_append_string(&builder,
                            instruction->instruction.kind == VALUE_SSA_INSTR_LOAD_LOCAL ? " = load_local " : " = load_global ") ||
                        !memory_ssa_append_slot_name(&builder, function, instruction->instruction.as.load_slot) ||
                        !memory_ssa_append_format(&builder,
                            " @ mem.%zu",
                            instruction->accesses[0].use_version_id)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case VALUE_SSA_INSTR_STORE_LOCAL:
                case VALUE_SSA_INSTR_STORE_GLOBAL:
                    if (!memory_ssa_append_format(&builder,
                            "mem.%zu = %s ",
                            instruction->accesses[0].def_version_id,
                            instruction->instruction.kind == VALUE_SSA_INSTR_STORE_LOCAL ? "store_local" : "store_global") ||
                        !memory_ssa_append_slot_name(&builder, function, instruction->instruction.as.store.slot) ||
                        (instruction->accesses[0].has_use_version &&
                            (!memory_ssa_append_format(&builder,
                                 " @ mem.%zu",
                                 instruction->accesses[0].use_version_id))) ||
                        !memory_ssa_append_string(&builder, ", ") ||
                        !memory_ssa_append_value_name(&builder, instruction->instruction.as.store.value)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case VALUE_SSA_INSTR_CALL:
                    if (!memory_ssa_append_call_instruction(&builder, instruction)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                default:
                    free(builder.data);
                    return 0;
                }

                if (!memory_ssa_append_string(&builder, "\n")) {
                    free(builder.data);
                    return 0;
                }
            }

            if (!memory_ssa_append_string(&builder, "    ")) {
                free(builder.data);
                return 0;
            }
            switch (block->terminator.kind) {
            case VALUE_SSA_TERM_RETURN:
                if (!memory_ssa_append_string(&builder, "ret ") ||
                    !memory_ssa_append_value_name(&builder, block->terminator.as.return_value)) {
                    free(builder.data);
                    return 0;
                }
                break;
            case VALUE_SSA_TERM_JUMP:
                if (!memory_ssa_append_format(&builder, "jmp bb.%zu", block->terminator.as.jump_target)) {
                    free(builder.data);
                    return 0;
                }
                break;
            case VALUE_SSA_TERM_BRANCH:
                if (!memory_ssa_append_string(&builder, "br ") ||
                    !memory_ssa_append_value_name(&builder, block->terminator.as.branch.condition) ||
                    !memory_ssa_append_format(&builder,
                        ", bb.%zu, bb.%zu",
                        block->terminator.as.branch.then_target,
                        block->terminator.as.branch.else_target)) {
                    free(builder.data);
                    return 0;
                }
                break;
            default:
                free(builder.data);
                return 0;
            }
            if (!memory_ssa_append_string(&builder, "\n")) {
                free(builder.data);
                return 0;
            }
        }

        if (!memory_ssa_append_string(&builder, "}\n")) {
            free(builder.data);
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

#endif
