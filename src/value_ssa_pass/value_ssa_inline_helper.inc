#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

enum {
    VALUE_SSA_TINY_INLINE_HELPER_MAX_INSTRUCTIONS = 8,
    VALUE_SSA_TINY_INLINE_HELPER_MAX_RETURN_INSTRUCTIONS = 4,
    VALUE_SSA_TINY_INLINE_CALLSITE_MAX_INSERTED_INSTRUCTIONS = 8,
    VALUE_SSA_TINY_INLINE_FUNCTION_MAX_INSERTED_INSTRUCTIONS = 12,
};

static int value_ssa_inline_tiny_helper_call_is_tiny_helper(const ValueSsaProgram *program, const char *callee_name);

static const ValueSsaFunction *value_ssa_find_function_by_name(const ValueSsaProgram *program, const char *name) {
    size_t function_index;

    if (!program || !name || name[0] == '\0') {
        return NULL;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];

        if (function->name && strcmp(function->name, name) == 0) {
            return function;
        }
    }

    return NULL;
}

static int value_ssa_function_is_tiny_inline_helper_leaf(const ValueSsaProgram *program, const ValueSsaFunction *function) {
    const ValueSsaBasicBlock *entry_block;
    const ValueSsaBasicBlock *return_block = NULL;
    unsigned char *local_has_value = NULL;
    unsigned char *address_taken_locals = NULL;
    size_t instruction_index;
    size_t block_index;
    size_t total_instruction_count = 0u;
    int ok = 0;

    if (!program || !function || !function->has_body ||
        function->block_count == 0u || function->block_count > 2u) {
        return 0;
    }

    entry_block = &function->blocks[0];
    if (entry_block->phi_count != 0u || !entry_block->has_terminator) {
        return 0;
    }
    if (function->block_count == 1u) {
        return_block = entry_block;
    } else {
        if (entry_block->terminator.kind != VALUE_SSA_TERM_JUMP ||
            entry_block->terminator.as.jump_target >= function->block_count) {
            return 0;
        }
        return_block = &function->blocks[entry_block->terminator.as.jump_target];
        if (return_block->phi_count != 0u || !return_block->has_terminator ||
            return_block->terminator.kind != VALUE_SSA_TERM_RETURN ||
            return_block->instruction_count > VALUE_SSA_TINY_INLINE_HELPER_MAX_RETURN_INSTRUCTIONS) {
            return 0;
        }
    }
    if (return_block->terminator.kind != VALUE_SSA_TERM_RETURN) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        total_instruction_count += block->instruction_count;
    }
    if (total_instruction_count > VALUE_SSA_TINY_INLINE_HELPER_MAX_INSTRUCTIONS) {
        return 0;
    }

    if (function->local_count > 0u) {
        local_has_value = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
        address_taken_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
        if (!local_has_value || !address_taken_locals) {
            goto cleanup;
        }
        for (block_index = 0; block_index < function->parameter_count && block_index < function->local_count; ++block_index) {
            if (function->locals[block_index].is_parameter) {
                local_has_value[block_index] = 1u;
            }
        }
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL &&
                instruction->as.addr_slot.kind == VALUE_SSA_SLOT_LOCAL &&
                instruction->as.addr_slot.id < function->local_count &&
                address_taken_locals) {
                address_taken_locals[instruction->as.addr_slot.id] = 1u;
            }
        }
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_LOAD_LOCAL:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
                    instruction->as.load_slot.id >= function->local_count ||
                    (local_has_value && !local_has_value[instruction->as.load_slot.id])) {
                    goto cleanup;
                }
                if (!function->locals[instruction->as.load_slot.id].is_parameter &&
                    (function->locals[instruction->as.load_slot.id].array_rank != 0u ||
                        (address_taken_locals && address_taken_locals[instruction->as.load_slot.id]))) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_STORE_LOCAL:
                if (instruction->as.store.slot.kind != VALUE_SSA_SLOT_LOCAL ||
                    instruction->as.store.slot.id >= function->local_count ||
                    function->locals[instruction->as.store.slot.id].array_rank != 0u ||
                    (address_taken_locals && address_taken_locals[instruction->as.store.slot.id])) {
                    goto cleanup;
                }
                if (local_has_value) {
                    local_has_value[instruction->as.store.slot.id] = 1u;
                }
                break;
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.load_slot.kind != VALUE_SSA_SLOT_GLOBAL ||
                    instruction->as.load_slot.id >= program->global_count) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                if (instruction->as.store.slot.kind != VALUE_SSA_SLOT_GLOBAL ||
                    instruction->as.store.slot.id >= program->global_count) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_ADDR_LOCAL:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.addr_slot.kind != VALUE_SSA_SLOT_LOCAL ||
                    instruction->as.addr_slot.id >= function->local_count) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_ADDR_GLOBAL:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.addr_slot.kind != VALUE_SSA_SLOT_GLOBAL ||
                    instruction->as.addr_slot.id >= program->global_count) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_LOAD_INDIRECT:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_STORE_INDIRECT:
                break;
            case VALUE_SSA_INSTR_MOV:
            case VALUE_SSA_INSTR_BINARY:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
                    goto cleanup;
                }
                break;
            default:
                goto cleanup;
            }
        }
    }

    ok = 1;

cleanup:
    free(local_has_value);
    free(address_taken_locals);
    return ok;
}

static int value_ssa_function_is_tiny_inline_helper(const ValueSsaProgram *program, const ValueSsaFunction *function) {
    const ValueSsaBasicBlock *entry_block;
    const ValueSsaBasicBlock *return_block = NULL;
    unsigned char *local_has_value = NULL;
    unsigned char *address_taken_locals = NULL;
    size_t instruction_index;
    size_t block_index;
    size_t total_instruction_count = 0u;
    int ok = 0;

    if (!program || !function || !function->has_body ||
        function->block_count == 0u || function->block_count > 2u) {
        return 0;
    }

    entry_block = &function->blocks[0];
    if (entry_block->phi_count != 0u || !entry_block->has_terminator) {
        return 0;
    }
    if (function->block_count == 1u) {
        return_block = entry_block;
    } else {
        if (entry_block->terminator.kind != VALUE_SSA_TERM_JUMP ||
            entry_block->terminator.as.jump_target >= function->block_count) {
            return 0;
        }
        return_block = &function->blocks[entry_block->terminator.as.jump_target];
        if (return_block->phi_count != 0u || !return_block->has_terminator ||
            return_block->terminator.kind != VALUE_SSA_TERM_RETURN ||
            return_block->instruction_count > VALUE_SSA_TINY_INLINE_HELPER_MAX_RETURN_INSTRUCTIONS) {
            return 0;
        }
    }
    if (return_block->terminator.kind != VALUE_SSA_TERM_RETURN) {
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        total_instruction_count += block->instruction_count;
    }
    if (total_instruction_count > VALUE_SSA_TINY_INLINE_HELPER_MAX_INSTRUCTIONS) {
        return 0;
    }

    if (function->local_count > 0u) {
        local_has_value = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
        address_taken_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
        if (!local_has_value || !address_taken_locals) {
            goto cleanup;
        }
        for (block_index = 0; block_index < function->parameter_count && block_index < function->local_count; ++block_index) {
            if (function->locals[block_index].is_parameter) {
                local_has_value[block_index] = 1u;
            }
        }
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL &&
                instruction->as.addr_slot.kind == VALUE_SSA_SLOT_LOCAL &&
                instruction->as.addr_slot.id < function->local_count &&
                address_taken_locals) {
                address_taken_locals[instruction->as.addr_slot.id] = 1u;
            }
        }
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_LOAD_LOCAL:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
                    instruction->as.load_slot.id >= function->local_count ||
                    (local_has_value && !local_has_value[instruction->as.load_slot.id])) {
                    goto cleanup;
                }
                if (!function->locals[instruction->as.load_slot.id].is_parameter &&
                    (function->locals[instruction->as.load_slot.id].array_rank != 0u ||
                        (address_taken_locals && address_taken_locals[instruction->as.load_slot.id]))) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_STORE_LOCAL:
                if (instruction->as.store.slot.kind != VALUE_SSA_SLOT_LOCAL ||
                    instruction->as.store.slot.id >= function->local_count ||
                    function->locals[instruction->as.store.slot.id].array_rank != 0u ||
                    (address_taken_locals && address_taken_locals[instruction->as.store.slot.id])) {
                    goto cleanup;
                }
                if (local_has_value) {
                    local_has_value[instruction->as.store.slot.id] = 1u;
                }
                break;
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.load_slot.kind != VALUE_SSA_SLOT_GLOBAL ||
                    instruction->as.load_slot.id >= program->global_count) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                if (instruction->as.store.slot.kind != VALUE_SSA_SLOT_GLOBAL ||
                    instruction->as.store.slot.id >= program->global_count) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_ADDR_LOCAL:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.addr_slot.kind != VALUE_SSA_SLOT_LOCAL ||
                    instruction->as.addr_slot.id >= function->local_count) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_ADDR_GLOBAL:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    instruction->as.addr_slot.kind != VALUE_SSA_SLOT_GLOBAL ||
                    instruction->as.addr_slot.id >= program->global_count) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_LOAD_INDIRECT:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_STORE_INDIRECT:
                break;
            case VALUE_SSA_INSTR_CALL:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID ||
                    !instruction->as.call.callee_name ||
                    !value_ssa_inline_tiny_helper_call_is_tiny_helper(program, instruction->as.call.callee_name)) {
                    goto cleanup;
                }
                break;
            case VALUE_SSA_INSTR_MOV:
            case VALUE_SSA_INSTR_BINARY:
                if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
                    goto cleanup;
                }
                break;
            default:
                goto cleanup;
            }
        }
    }

    ok = 1;

cleanup:
    free(local_has_value);
    free(address_taken_locals);
    return ok;
}

static int value_ssa_function_is_tiny_inline_helper_call_candidate(const ValueSsaProgram *program,
    const ValueSsaFunction *function) {
    if (!program || !function || !function->has_body) {
        return 0;
    }

    return value_ssa_function_is_tiny_inline_helper_leaf(program, function);
}

static int value_ssa_inline_helper_map_value_ref(ValueSsaValueRef value,
    const unsigned char *has_mapping,
    const ValueSsaValueRef *mappings,
    size_t mapping_count,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    if (!out_value) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-258: invalid inline-helper value-map contract");
        return 0;
    }

    if (value.kind != VALUE_SSA_VALUE_ID) {
        *out_value = value;
        return 1;
    }

    if (!has_mapping || !mappings || value.value_id >= mapping_count || !has_mapping[value.value_id]) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-259: missing inline-helper mapping for ssa.%zu",
            value.value_id);
        return 0;
    }

    *out_value = mappings[value.value_id];
    return 1;
}

static size_t value_ssa_tiny_inline_helper_inserted_instruction_cost(const ValueSsaFunction *callee) {
    const ValueSsaBasicBlock *entry_block;
    const ValueSsaBasicBlock *return_block;
    size_t instruction_index;
    size_t inserted;

    if (!callee || !callee->has_body || callee->block_count == 0u || callee->block_count > 2u) {
        return (size_t)-1;
    }

    entry_block = &callee->blocks[0];
    return_block = callee->block_count == 1u
        ? entry_block
        : &callee->blocks[entry_block->terminator.as.jump_target];
    inserted = return_block->terminator.has_return_value ? 1u : 0u;

    for (instruction_index = 0; instruction_index < entry_block->instruction_count; ++instruction_index) {
        if (entry_block->instructions[instruction_index].kind != VALUE_SSA_INSTR_LOAD_LOCAL &&
            entry_block->instructions[instruction_index].kind != VALUE_SSA_INSTR_STORE_LOCAL) {
            ++inserted;
        }
    }
    if (return_block != entry_block) {
        for (instruction_index = 0; instruction_index < return_block->instruction_count; ++instruction_index) {
            if (return_block->instructions[instruction_index].kind != VALUE_SSA_INSTR_LOAD_LOCAL &&
                return_block->instructions[instruction_index].kind != VALUE_SSA_INSTR_STORE_LOCAL) {
                ++inserted;
            }
        }
    }

    return inserted;
}

static int value_ssa_inline_tiny_helper_call_into_block(const ValueSsaProgram *program,
    ValueSsaFunction *caller,
    const ValueSsaFunction *callee,
    const ValueSsaInstruction *call_instruction,
    ValueSsaBasicBlock *rebuilt_block,
    ValueSsaError *error) {
    const ValueSsaBasicBlock *entry_block;
    const ValueSsaBasicBlock *return_block;
    unsigned char *has_mapping = NULL;
    ValueSsaValueRef *mappings = NULL;
    unsigned char *local_has_mapping = NULL;
    ValueSsaValueRef *local_mappings = NULL;
    ValueSsaInstruction cloned_instruction;
    size_t instruction_index;
    size_t inserted_instruction_count = 0u;
    ValueSsaValueRef return_value;
    int ok = 0;

    if (!program || !caller || !callee || !call_instruction || !rebuilt_block) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-260: invalid inline-helper block contract");
        return 0;
    }

    entry_block = &callee->blocks[0];
    return_block = callee->block_count == 1u
        ? &callee->blocks[0]
        : &callee->blocks[entry_block->terminator.as.jump_target];

    if (!value_ssa_function_is_tiny_inline_helper(program, callee) ||
        call_instruction->kind != VALUE_SSA_INSTR_CALL ||
        !call_instruction->as.call.callee_name ||
        call_instruction->as.call.arg_count != callee->parameter_count) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-261: invalid inline-helper call shape");
        return 0;
    }
    if (call_instruction->has_result && call_instruction->result.kind != VALUE_SSA_VALUE_ID) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-261: invalid inline-helper call result shape");
        return 0;
    }
    if (return_block->terminator.has_return_value && !call_instruction->has_result) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-261: inline-helper value return requires call result");
        return 0;
    }

    has_mapping = (unsigned char *)calloc(callee->next_value_id ? callee->next_value_id : 1u, sizeof(unsigned char));
    mappings = (ValueSsaValueRef *)calloc(callee->next_value_id ? callee->next_value_id : 1u, sizeof(ValueSsaValueRef));
    local_has_mapping = (unsigned char *)calloc(callee->local_count ? callee->local_count : 1u, sizeof(unsigned char));
    local_mappings = (ValueSsaValueRef *)calloc(callee->local_count ? callee->local_count : 1u, sizeof(ValueSsaValueRef));
    if (!has_mapping || !mappings || !local_has_mapping || !local_mappings) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-262: out of memory preparing inline-helper mappings");
        goto cleanup;
    }

    for (instruction_index = 0; instruction_index < entry_block->instruction_count; ++instruction_index) {
        const ValueSsaInstruction *source = &entry_block->instructions[instruction_index];
        ValueSsaValueRef mapped_value;

        if (inserted_instruction_count >= VALUE_SSA_TINY_INLINE_CALLSITE_MAX_INSERTED_INSTRUCTIONS) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-265: inline-helper callsite budget exceeded");
            goto cleanup;
        }

        switch (source->kind) {
        case VALUE_SSA_INSTR_LOAD_LOCAL:
            if (source->as.load_slot.id < callee->local_count &&
                local_has_mapping[source->as.load_slot.id]) {
                mapped_value = local_mappings[source->as.load_slot.id];
            } else if (source->as.load_slot.id < callee->parameter_count &&
                       source->as.load_slot.id < call_instruction->as.call.arg_count) {
                mapped_value = call_instruction->as.call.args[source->as.load_slot.id];
            } else {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-259A: missing inline-helper local mapping");
                goto cleanup;
            }
            mappings[source->result.value_id] = mapped_value;
            has_mapping[source->result.value_id] = 1u;
            break;
        case VALUE_SSA_INSTR_STORE_LOCAL:
            if (!value_ssa_inline_helper_map_value_ref(
                    source->as.store.value, has_mapping, mappings, callee->next_value_id, &mapped_value, error)) {
                goto cleanup;
            }
            local_mappings[source->as.store.slot.id] = mapped_value;
            local_has_mapping[source->as.store.slot.id] = 1u;
            break;
        case VALUE_SSA_INSTR_MOV:
            if (!value_ssa_inline_helper_map_value_ref(
                    source->as.mov_value, has_mapping, mappings, callee->next_value_id, &mapped_value, error)) {
                goto cleanup;
            }
            mappings[source->result.value_id] = mapped_value;
            has_mapping[source->result.value_id] = 1u;
            break;
        case VALUE_SSA_INSTR_LOAD_GLOBAL:
            memset(&cloned_instruction, 0, sizeof(cloned_instruction));
            cloned_instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
            cloned_instruction.has_result = 1;
            cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
            cloned_instruction.as.load_slot = source->as.load_slot;
            if (cloned_instruction.result.value_id == (size_t)-1 ||
                !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                goto cleanup;
            }
            ++inserted_instruction_count;
            mappings[source->result.value_id] = cloned_instruction.result;
            has_mapping[source->result.value_id] = 1u;
            break;
        case VALUE_SSA_INSTR_STORE_GLOBAL:
            memset(&cloned_instruction, 0, sizeof(cloned_instruction));
            cloned_instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
            cloned_instruction.as.store.slot = source->as.store.slot;
            if (!value_ssa_inline_helper_map_value_ref(
                    source->as.store.value,
                    has_mapping,
                    mappings,
                    callee->next_value_id,
                    &cloned_instruction.as.store.value,
                    error) ||
                !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                goto cleanup;
            }
            ++inserted_instruction_count;
            break;
        case VALUE_SSA_INSTR_ADDR_LOCAL:
            memset(&cloned_instruction, 0, sizeof(cloned_instruction));
            cloned_instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
            cloned_instruction.has_result = 1;
            cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
            cloned_instruction.as.addr_slot = source->as.addr_slot;
            if (cloned_instruction.result.value_id == (size_t)-1 ||
                !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                goto cleanup;
            }
            ++inserted_instruction_count;
            mappings[source->result.value_id] = cloned_instruction.result;
            has_mapping[source->result.value_id] = 1u;
            break;
        case VALUE_SSA_INSTR_ADDR_GLOBAL:
            memset(&cloned_instruction, 0, sizeof(cloned_instruction));
            cloned_instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
            cloned_instruction.has_result = 1;
            cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
            cloned_instruction.as.addr_slot = source->as.addr_slot;
            if (cloned_instruction.result.value_id == (size_t)-1 ||
                !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                goto cleanup;
            }
            ++inserted_instruction_count;
            mappings[source->result.value_id] = cloned_instruction.result;
            has_mapping[source->result.value_id] = 1u;
            break;
        case VALUE_SSA_INSTR_BINARY:
            memset(&cloned_instruction, 0, sizeof(cloned_instruction));
            cloned_instruction.kind = VALUE_SSA_INSTR_BINARY;
            cloned_instruction.has_result = 1;
            cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
            cloned_instruction.as.binary.op = source->as.binary.op;
            if (cloned_instruction.result.value_id == (size_t)-1 ||
                !value_ssa_inline_helper_map_value_ref(source->as.binary.lhs,
                    has_mapping,
                    mappings,
                    callee->next_value_id,
                    &cloned_instruction.as.binary.lhs,
                    error) ||
                !value_ssa_inline_helper_map_value_ref(source->as.binary.rhs,
                    has_mapping,
                    mappings,
                    callee->next_value_id,
                    &cloned_instruction.as.binary.rhs,
                    error) ||
                !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                goto cleanup;
            }
            ++inserted_instruction_count;
            mappings[source->result.value_id] = cloned_instruction.result;
            has_mapping[source->result.value_id] = 1u;
            break;
        case VALUE_SSA_INSTR_LOAD_INDIRECT:
            memset(&cloned_instruction, 0, sizeof(cloned_instruction));
            cloned_instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
            cloned_instruction.has_result = 1;
            cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
            if (cloned_instruction.result.value_id == (size_t)-1 ||
                !value_ssa_inline_helper_map_value_ref(
                    source->as.load_indirect_addr,
                    has_mapping,
                    mappings,
                    callee->next_value_id,
                    &cloned_instruction.as.load_indirect_addr,
                    error) ||
                !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                goto cleanup;
            }
            ++inserted_instruction_count;
            mappings[source->result.value_id] = cloned_instruction.result;
            has_mapping[source->result.value_id] = 1u;
            break;
        case VALUE_SSA_INSTR_STORE_INDIRECT:
            memset(&cloned_instruction, 0, sizeof(cloned_instruction));
            cloned_instruction.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
            if (!value_ssa_inline_helper_map_value_ref(
                    source->as.store_indirect.addr,
                    has_mapping,
                    mappings,
                    callee->next_value_id,
                    &cloned_instruction.as.store_indirect.addr,
                    error) ||
                !value_ssa_inline_helper_map_value_ref(
                    source->as.store_indirect.value,
                    has_mapping,
                    mappings,
                    callee->next_value_id,
                    &cloned_instruction.as.store_indirect.value,
                    error) ||
                !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                goto cleanup;
            }
            ++inserted_instruction_count;
            break;
        case VALUE_SSA_INSTR_CALL:
            memset(&cloned_instruction, 0, sizeof(cloned_instruction));
            cloned_instruction.kind = VALUE_SSA_INSTR_CALL;
            cloned_instruction.has_result = 1;
            cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
            cloned_instruction.as.call.callee_name = source->as.call.callee_name;
            cloned_instruction.as.call.arg_count = source->as.call.arg_count;
            cloned_instruction.as.call.args = NULL;
            if (cloned_instruction.result.value_id == (size_t)-1) {
                goto cleanup;
            }
            if (source->as.call.arg_count != 0u) {
                size_t arg_index;

                cloned_instruction.as.call.args =
                    (ValueSsaValueRef *)calloc(source->as.call.arg_count, sizeof(ValueSsaValueRef));
                if (!cloned_instruction.as.call.args) {
                    value_ssa_instruction_free(&cloned_instruction);
                    value_ssa_set_error(error, 0, 0, "VALUE-SSA-262A: out of memory cloning inline-helper call");
                    goto cleanup;
                }
                for (arg_index = 0; arg_index < source->as.call.arg_count; ++arg_index) {
                    if (!value_ssa_inline_helper_map_value_ref(source->as.call.args[arg_index],
                            has_mapping,
                            mappings,
                            callee->next_value_id,
                            &cloned_instruction.as.call.args[arg_index],
                            error)) {
                        value_ssa_instruction_free(&cloned_instruction);
                        goto cleanup;
                    }
                }
            }
            if (!value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                value_ssa_instruction_free(&cloned_instruction);
                goto cleanup;
            }
            ++inserted_instruction_count;
            mappings[source->result.value_id] = cloned_instruction.result;
            has_mapping[source->result.value_id] = 1u;
            break;
        default:
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-263: unsupported inline-helper instruction kind");
            goto cleanup;
        }
    }

    if (return_block != entry_block) {
        for (instruction_index = 0; instruction_index < return_block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *source = &return_block->instructions[instruction_index];
            ValueSsaValueRef mapped_value;

            if (inserted_instruction_count >= VALUE_SSA_TINY_INLINE_CALLSITE_MAX_INSERTED_INSTRUCTIONS) {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-265: inline-helper callsite budget exceeded");
                goto cleanup;
            }

        switch (source->kind) {
        case VALUE_SSA_INSTR_LOAD_LOCAL:
            if (source->as.load_slot.id < callee->local_count &&
                local_has_mapping[source->as.load_slot.id]) {
                mapped_value = local_mappings[source->as.load_slot.id];
            } else if (source->as.load_slot.id < callee->parameter_count &&
                       source->as.load_slot.id < call_instruction->as.call.arg_count) {
                mapped_value = call_instruction->as.call.args[source->as.load_slot.id];
            } else {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-259A: missing inline-helper local mapping");
                goto cleanup;
            }
            mappings[source->result.value_id] = mapped_value;
            has_mapping[source->result.value_id] = 1u;
            break;
        case VALUE_SSA_INSTR_STORE_LOCAL:
            if (!value_ssa_inline_helper_map_value_ref(
                    source->as.store.value, has_mapping, mappings, callee->next_value_id, &mapped_value, error)) {
                goto cleanup;
            }
            local_mappings[source->as.store.slot.id] = mapped_value;
            local_has_mapping[source->as.store.slot.id] = 1u;
            break;
        case VALUE_SSA_INSTR_MOV:
            if (!value_ssa_inline_helper_map_value_ref(
                        source->as.mov_value, has_mapping, mappings, callee->next_value_id, &mapped_value, error)) {
                    goto cleanup;
                }
                mappings[source->result.value_id] = mapped_value;
                has_mapping[source->result.value_id] = 1u;
                break;
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                memset(&cloned_instruction, 0, sizeof(cloned_instruction));
                cloned_instruction.kind = VALUE_SSA_INSTR_LOAD_GLOBAL;
                cloned_instruction.has_result = 1;
                cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
                cloned_instruction.as.load_slot = source->as.load_slot;
                if (cloned_instruction.result.value_id == (size_t)-1 ||
                    !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                    goto cleanup;
                }
                ++inserted_instruction_count;
                mappings[source->result.value_id] = cloned_instruction.result;
                has_mapping[source->result.value_id] = 1u;
                break;
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                memset(&cloned_instruction, 0, sizeof(cloned_instruction));
                cloned_instruction.kind = VALUE_SSA_INSTR_STORE_GLOBAL;
                cloned_instruction.as.store.slot = source->as.store.slot;
                if (!value_ssa_inline_helper_map_value_ref(
                        source->as.store.value,
                        has_mapping,
                        mappings,
                        callee->next_value_id,
                        &cloned_instruction.as.store.value,
                        error) ||
                    !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                    goto cleanup;
                }
                ++inserted_instruction_count;
                break;
            case VALUE_SSA_INSTR_ADDR_LOCAL:
                memset(&cloned_instruction, 0, sizeof(cloned_instruction));
                cloned_instruction.kind = VALUE_SSA_INSTR_ADDR_LOCAL;
                cloned_instruction.has_result = 1;
                cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
                cloned_instruction.as.addr_slot = source->as.addr_slot;
                if (cloned_instruction.result.value_id == (size_t)-1 ||
                    !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                    goto cleanup;
                }
                ++inserted_instruction_count;
                mappings[source->result.value_id] = cloned_instruction.result;
                has_mapping[source->result.value_id] = 1u;
                break;
            case VALUE_SSA_INSTR_ADDR_GLOBAL:
                memset(&cloned_instruction, 0, sizeof(cloned_instruction));
                cloned_instruction.kind = VALUE_SSA_INSTR_ADDR_GLOBAL;
                cloned_instruction.has_result = 1;
                cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
                cloned_instruction.as.addr_slot = source->as.addr_slot;
                if (cloned_instruction.result.value_id == (size_t)-1 ||
                    !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                    goto cleanup;
                }
                ++inserted_instruction_count;
                mappings[source->result.value_id] = cloned_instruction.result;
                has_mapping[source->result.value_id] = 1u;
                break;
            case VALUE_SSA_INSTR_BINARY:
                memset(&cloned_instruction, 0, sizeof(cloned_instruction));
                cloned_instruction.kind = VALUE_SSA_INSTR_BINARY;
                cloned_instruction.has_result = 1;
                cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
                cloned_instruction.as.binary.op = source->as.binary.op;
                if (cloned_instruction.result.value_id == (size_t)-1 ||
                    !value_ssa_inline_helper_map_value_ref(source->as.binary.lhs,
                        has_mapping,
                        mappings,
                        callee->next_value_id,
                        &cloned_instruction.as.binary.lhs,
                        error) ||
                    !value_ssa_inline_helper_map_value_ref(source->as.binary.rhs,
                        has_mapping,
                        mappings,
                        callee->next_value_id,
                        &cloned_instruction.as.binary.rhs,
                        error) ||
                    !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                    goto cleanup;
                }
                ++inserted_instruction_count;
                mappings[source->result.value_id] = cloned_instruction.result;
                has_mapping[source->result.value_id] = 1u;
                break;
            case VALUE_SSA_INSTR_LOAD_INDIRECT:
                memset(&cloned_instruction, 0, sizeof(cloned_instruction));
                cloned_instruction.kind = VALUE_SSA_INSTR_LOAD_INDIRECT;
                cloned_instruction.has_result = 1;
                cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
                if (cloned_instruction.result.value_id == (size_t)-1 ||
                    !value_ssa_inline_helper_map_value_ref(
                        source->as.load_indirect_addr,
                        has_mapping,
                        mappings,
                        callee->next_value_id,
                        &cloned_instruction.as.load_indirect_addr,
                        error) ||
                    !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                    goto cleanup;
                }
                ++inserted_instruction_count;
                mappings[source->result.value_id] = cloned_instruction.result;
                has_mapping[source->result.value_id] = 1u;
                break;
            case VALUE_SSA_INSTR_STORE_INDIRECT:
                memset(&cloned_instruction, 0, sizeof(cloned_instruction));
                cloned_instruction.kind = VALUE_SSA_INSTR_STORE_INDIRECT;
                if (!value_ssa_inline_helper_map_value_ref(
                        source->as.store_indirect.addr,
                        has_mapping,
                        mappings,
                        callee->next_value_id,
                        &cloned_instruction.as.store_indirect.addr,
                        error) ||
                    !value_ssa_inline_helper_map_value_ref(
                        source->as.store_indirect.value,
                        has_mapping,
                        mappings,
                        callee->next_value_id,
                        &cloned_instruction.as.store_indirect.value,
                        error) ||
                    !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                    goto cleanup;
                }
                ++inserted_instruction_count;
                break;
            case VALUE_SSA_INSTR_CALL:
                memset(&cloned_instruction, 0, sizeof(cloned_instruction));
                cloned_instruction.kind = VALUE_SSA_INSTR_CALL;
                cloned_instruction.has_result = 1;
                cloned_instruction.result = value_ssa_value_id(value_ssa_function_allocate_value(caller));
                cloned_instruction.as.call.callee_name = source->as.call.callee_name;
                cloned_instruction.as.call.arg_count = source->as.call.arg_count;
                cloned_instruction.as.call.args = NULL;
                if (cloned_instruction.result.value_id == (size_t)-1) {
                    goto cleanup;
                }
                if (source->as.call.arg_count != 0u) {
                    size_t arg_index;

                    cloned_instruction.as.call.args =
                        (ValueSsaValueRef *)calloc(source->as.call.arg_count, sizeof(ValueSsaValueRef));
                    if (!cloned_instruction.as.call.args) {
                        value_ssa_instruction_free(&cloned_instruction);
                        value_ssa_set_error(error, 0, 0, "VALUE-SSA-262A: out of memory cloning inline-helper call");
                        goto cleanup;
                    }
                    for (arg_index = 0; arg_index < source->as.call.arg_count; ++arg_index) {
                        if (!value_ssa_inline_helper_map_value_ref(source->as.call.args[arg_index],
                                has_mapping,
                                mappings,
                                callee->next_value_id,
                                &cloned_instruction.as.call.args[arg_index],
                                error)) {
                            value_ssa_instruction_free(&cloned_instruction);
                            goto cleanup;
                        }
                    }
                }
                if (!value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
                    value_ssa_instruction_free(&cloned_instruction);
                    goto cleanup;
                }
                ++inserted_instruction_count;
                mappings[source->result.value_id] = cloned_instruction.result;
                has_mapping[source->result.value_id] = 1u;
                break;
            default:
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-263: unsupported inline-helper instruction kind");
                goto cleanup;
            }
        }
    }

    if (return_block->terminator.has_return_value) {
        if (!value_ssa_inline_helper_map_value_ref(return_block->terminator.as.return_value,
                has_mapping,
                mappings,
                callee->next_value_id,
                &return_value,
                error)) {
            goto cleanup;
        }

        memset(&cloned_instruction, 0, sizeof(cloned_instruction));
        cloned_instruction.kind = VALUE_SSA_INSTR_MOV;
        cloned_instruction.has_result = 1;
        cloned_instruction.result = call_instruction->result;
        cloned_instruction.as.mov_value = return_value;
        if (inserted_instruction_count >= VALUE_SSA_TINY_INLINE_CALLSITE_MAX_INSERTED_INSTRUCTIONS ||
            !value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
            if (inserted_instruction_count >= VALUE_SSA_TINY_INLINE_CALLSITE_MAX_INSERTED_INSTRUCTIONS) {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-265: inline-helper callsite budget exceeded");
            }
            goto cleanup;
        }
    }

    ok = 1;

cleanup:
    free(local_has_mapping);
    free(local_mappings);
    free(has_mapping);
    free(mappings);
    return ok;
}

static int value_ssa_inline_tiny_helper_call_fits_budget(const ValueSsaFunction *callee) {
    size_t inserted;

    if (!callee || !callee->has_body) {
        return 0;
    }
    inserted = value_ssa_tiny_inline_helper_inserted_instruction_cost(callee);
    if (inserted == (size_t)-1) {
        return 0;
    }

    return inserted <= VALUE_SSA_TINY_INLINE_CALLSITE_MAX_INSERTED_INSTRUCTIONS;
}

static int value_ssa_inline_tiny_helper_call_is_tiny_helper(const ValueSsaProgram *program,
    const char *callee_name) {
    const ValueSsaFunction *callee = NULL;

    if (!program || !callee_name || callee_name[0] == '\0') {
        return 0;
    }

    callee = value_ssa_find_function_by_name(program, callee_name);
    return callee && value_ssa_function_is_tiny_inline_helper_call_candidate(program, callee);
}

int value_ssa_inline_tiny_internal_helpers(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-264: invalid inline-helper contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;
        size_t function_inline_inserted_budget = 0u;

        if (!function->has_body) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            ValueSsaBasicBlock rebuilt_block;
            size_t instruction_index;
            int changed = 0;
            int ok = 1;

            memset(&rebuilt_block, 0, sizeof(rebuilt_block));
            rebuilt_block.id = block->id;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                const ValueSsaFunction *callee = NULL;

                if (instruction->kind == VALUE_SSA_INSTR_CALL && instruction->as.call.callee_name) {
                    callee = value_ssa_find_function_by_name(program, instruction->as.call.callee_name);
                }

                if (callee && callee != function &&
                    value_ssa_function_is_tiny_inline_helper_call_candidate(program, callee) &&
                    value_ssa_inline_tiny_helper_call_fits_budget(callee)) {
                    size_t callsite_cost = value_ssa_tiny_inline_helper_inserted_instruction_cost(callee);

                    if (callsite_cost != (size_t)-1 &&
                        function_inline_inserted_budget + callsite_cost <=
                            VALUE_SSA_TINY_INLINE_FUNCTION_MAX_INSERTED_INSTRUCTIONS) {
                    ok = value_ssa_inline_tiny_helper_call_into_block(
                        program, function, callee, instruction, &rebuilt_block, error);
                    if (ok) {
                            function_inline_inserted_budget += callsite_cost;
                        changed = 1;
                        continue;
                    }
                    }
                }

                if (!value_ssa_block_append_instruction(&rebuilt_block, instruction, error)) {
                    ok = 0;
                }
                if (!ok) {
                    break;
                }
            }

            if (!ok) {
                value_ssa_basic_block_free(&rebuilt_block);
                return 0;
            }

            if (!changed) {
                value_ssa_basic_block_free(&rebuilt_block);
                continue;
            }

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                value_ssa_instruction_free(&block->instructions[instruction_index]);
            }
            free(block->instructions);
            block->instructions = rebuilt_block.instructions;
            block->instruction_count = rebuilt_block.instruction_count;
            block->instruction_capacity = rebuilt_block.instruction_capacity;
        }
    }

    return value_ssa_verify_program(program, error);
}

#endif
