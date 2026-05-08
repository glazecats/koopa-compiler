#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

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

static int value_ssa_function_is_tiny_inline_helper(const ValueSsaProgram *program, const ValueSsaFunction *function) {
    const ValueSsaBasicBlock *block;
    size_t instruction_index;

    if (!program || !function || !function->has_body || function->block_count != 1u || function->parameter_count == 0u) {
        return 0;
    }

    block = &function->blocks[0];
    if (block->phi_count != 0u || !block->has_terminator || block->terminator.kind != VALUE_SSA_TERM_RETURN ||
        !block->terminator.has_return_value || block->instruction_count == 0u || block->instruction_count > 8u) {
        return 0;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        if (!instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
            return 0;
        }

        switch (instruction->kind) {
        case VALUE_SSA_INSTR_LOAD_LOCAL:
            if (instruction->as.load_slot.kind != VALUE_SSA_SLOT_LOCAL ||
                instruction->as.load_slot.id >= function->parameter_count ||
                instruction->as.load_slot.id >= function->local_count ||
                !function->locals[instruction->as.load_slot.id].is_parameter) {
                return 0;
            }
            break;
        case VALUE_SSA_INSTR_LOAD_GLOBAL:
            if (instruction->as.load_slot.kind != VALUE_SSA_SLOT_GLOBAL ||
                instruction->as.load_slot.id >= program->global_count) {
                return 0;
            }
            break;
        case VALUE_SSA_INSTR_MOV:
        case VALUE_SSA_INSTR_BINARY:
            break;
        default:
            return 0;
        }
    }

    return 1;
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

static int value_ssa_inline_tiny_helper_call_into_block(const ValueSsaProgram *program,
    ValueSsaFunction *caller,
    const ValueSsaFunction *callee,
    const ValueSsaInstruction *call_instruction,
    ValueSsaBasicBlock *rebuilt_block,
    ValueSsaError *error) {
    const ValueSsaBasicBlock *callee_block;
    unsigned char *has_mapping = NULL;
    ValueSsaValueRef *mappings = NULL;
    ValueSsaInstruction cloned_instruction;
    size_t instruction_index;
    ValueSsaValueRef return_value;
    int ok = 0;

    if (!program || !caller || !callee || !call_instruction || !rebuilt_block) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-260: invalid inline-helper block contract");
        return 0;
    }
    if (!value_ssa_function_is_tiny_inline_helper(program, callee) ||
        call_instruction->kind != VALUE_SSA_INSTR_CALL ||
        !call_instruction->has_result ||
        call_instruction->result.kind != VALUE_SSA_VALUE_ID ||
        !call_instruction->as.call.callee_name ||
        call_instruction->as.call.arg_count != callee->parameter_count) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-261: invalid inline-helper call shape");
        return 0;
    }

    has_mapping = (unsigned char *)calloc(callee->next_value_id ? callee->next_value_id : 1u, sizeof(unsigned char));
    mappings = (ValueSsaValueRef *)calloc(callee->next_value_id ? callee->next_value_id : 1u, sizeof(ValueSsaValueRef));
    if (!has_mapping || !mappings) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-262: out of memory preparing inline-helper mappings");
        goto cleanup;
    }

    callee_block = &callee->blocks[0];
    for (instruction_index = 0; instruction_index < callee_block->instruction_count; ++instruction_index) {
        const ValueSsaInstruction *source = &callee_block->instructions[instruction_index];
        ValueSsaValueRef mapped_value;

        switch (source->kind) {
        case VALUE_SSA_INSTR_LOAD_LOCAL:
            mapped_value = call_instruction->as.call.args[source->as.load_slot.id];
            mappings[source->result.value_id] = mapped_value;
            has_mapping[source->result.value_id] = 1u;
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
            mappings[source->result.value_id] = cloned_instruction.result;
            has_mapping[source->result.value_id] = 1u;
            break;
        default:
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-263: unsupported inline-helper instruction kind");
            goto cleanup;
        }
    }

    if (!value_ssa_inline_helper_map_value_ref(callee_block->terminator.as.return_value,
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
    if (!value_ssa_block_append_instruction(rebuilt_block, &cloned_instruction, error)) {
        goto cleanup;
    }

    ok = 1;

cleanup:
    free(has_mapping);
    free(mappings);
    return ok;
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

                if (callee && callee != function && value_ssa_function_is_tiny_inline_helper(program, callee)) {
                    ok = value_ssa_inline_tiny_helper_call_into_block(
                        program, function, callee, instruction, &rebuilt_block, error);
                    if (ok) {
                        changed = 1;
                        continue;
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
